#include "sgn/vehicle_segments.hpp"
#include "sgn/model_resources.hpp"
#include "sgn/retail_camera.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>
namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::compile_time::as_float;
double transform_component(VehicleVector v, const VehicleMatrix &m, unsigned component)
{
    return (double(v.z) * m[8 + component] + double(v.y) * m[4 + component]) + double(v.x) * m[component];
}
} // namespace
void update_vehicle_segments(const VehicleParameters &parameters, const VehicleState &s,
                             const VehiclePresentationState &p, const VehicleAnchorState &anchor,
                             VehicleSegments &segments, VehicleMatrix *audit_rotation)
{
    if (p.field_3134 == 0)
        return;
    const auto forward = anchor.forward;
    const auto &a = s.acceleration;
    const double wx = transform_component(forward, s.orientation, 0);
    const float wy = vehicle_f32(transform_component(forward, s.orientation, 1)),
                wz = vehicle_f32(transform_component(forward, s.orientation, 2));
    const double projection = (double(a.z) * wz + double(a.y) * wy) + double(a.x) * wx;
    const float stored_projection = vehicle_f32(projection);
    const double rx = double(a.x) - projection * wx;
    const double ry = double(a.y) - vehicle_f32(double(stored_projection) * wy);
    const double rz = double(a.z) - vehicle_f32(double(stored_projection) * wz);
    float longitudinal =
        vehicle_f32(((rz * s.basis_z.z + ry * s.basis_z.y) + rx * s.basis_z.x) * as_float(0xbe000000u)); // 46E5A8
    if (s.turbo_rpm > as_float(0x3dcccccdu))
    {
        const double amplified = ((double(s.turbo_rpm) * 4.0) / parameters.turbo_max + 1.0) * longitudinal;
        longitudinal = vehicle_f32(amplified);
        if (std::abs(amplified) > as_float(0x3e19999au))
            longitudinal = longitudinal < 0 ? as_float(0xbe19999au) : as_float(0x3e19999au);
    }
    const float lateral =
        vehicle_f32(((rz * s.basis_x.z + ry * s.basis_x.y) + rx * s.basis_x.x) * as_float(0x3e000000u));
    CameraVector direction{forward.x, forward.y, forward.z, forward.w};
    for (auto &segment : segments.items)
    {
        const auto &angles = segment.angles;
        direction = camera_rotate_degrees({angles.x, angles.y, angles.z, angles.w}, direction, audit_rotation);
        segment.direction = {direction.x, direction.y, direction.z, direction.w};
        const double vx = double(segment.velocity.x) - longitudinal;
        const float vz = vehicle_f32(double(segment.velocity.z) - lateral);
        segment.angles.x = vehicle_f32(vx + segment.angles.x);
        segment.angles.y = vehicle_f32(double(segment.velocity.y) + segment.angles.y);
        segment.angles.z = vehicle_f32(double(vz) + segment.angles.z);
        segment.velocity.x =
            vehicle_f32((vx - double(segment.angles.x) * as_float(0x3d000000u)) * as_float(0x3f666666u));
        segment.velocity.z =
            vehicle_f32((double(vz) - double(segment.angles.z) * as_float(0x3d000000u)) * as_float(0x3f666666u));
    }
}
void present_vehicle_segments(const VehicleState &s, const VehiclePresentationState &p,
                              const VehicleAnchorState &anchor, const VehicleSegments &segments,
                              const VehicleMatrix &basis, std::span<ModelTransform> transforms,
                              contact::Vector4 &effect)
{
    if (p.field_3134 == 0)
        return;
    if (transforms.size() != 6)
        throw FormatError("vehicle segments require six contiguous source transforms");
    VehicleVector current{vehicle_f32(transform_component(anchor.position, basis, 0)),
                          vehicle_f32(transform_component(anchor.position, basis, 1)),
                          vehicle_f32(transform_component(anchor.position, basis, 2)), 0};
    current.y = vehicle_f32(double(current.y) + s.body_heave);
    for (unsigned i = 0; i < 6; ++i)
    {
        const auto source = segments.items[i].direction;
        const VehicleVector direction{vehicle_f32(transform_component(source, basis, 0)),
                                      vehicle_f32(transform_component(source, basis, 1)),
                                      vehicle_f32(transform_component(source, basis, 2)), source.w};
        auto &model = transforms[i];
        model.forward = {direction.x, direction.y, direction.z};
        model.forward_w = std::bit_cast<std::uint32_t>(direction.w);
        model.position = {current.x, current.y,
                          current.z}; // position_w is undefined original stack data: retain host lane
        const double x = double(anchor.extent) * direction.x;
        const float y = vehicle_f32(double(anchor.extent) * direction.y),
                    z = vehicle_f32(double(anchor.extent) * direction.z);
        current.x = vehicle_f32(double(current.x) + x);
        current.y = vehicle_f32(double(current.y) + y);
        current.z = vehicle_f32(double(current.z) + z);
    }
    const double x = -double(current.x) * s.basis_x.x;
    const float y = vehicle_f32(-double(current.x) * s.basis_x.y), z = vehicle_f32(-double(current.x) * s.basis_x.z);
    // 46E53C is the original local height reference; all FST points below matter.
    const double vertical = double(as_float(0x428c0000u)) - current.y;
    const float ux = vehicle_f32(vertical * s.up.x), uy = vehicle_f32(vertical * s.up.y),
                uz = vehicle_f32(vertical * s.up.z);
    const float fx = vehicle_f32(double(current.z) * s.basis_z.x), fy = vehicle_f32(double(current.z) * s.basis_z.y),
                fz = vehicle_f32(double(current.z) * s.basis_z.z);
    const double ox = (x + ux) + fx;
    const float oy = vehicle_f32(double(fy) + vehicle_f32(double(y) + uy));
    const float oz = vehicle_f32(double(fz) + vehicle_f32(double(z) + uz));
    effect.x = vehicle_f32(double(s.position.x) - ox);
    effect.y = vehicle_f32(double(s.position.y) - oy);
    effect.z = vehicle_f32(double(s.position.z) - oz);
}
} // namespace sgn::retail
