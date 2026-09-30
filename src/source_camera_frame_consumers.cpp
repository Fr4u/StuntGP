#include "sgn/source_camera_frame_consumers.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_camera_vehicle.hpp"
#include "sgn/menu_keyboard_acquisition.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
float f32(double x)
{
    return static_cast<float>(x);
}
using sgn::bit_reinterpret::as_float;
bool c0(double a, double b)
{
    return a < b || std::isnan(a) || std::isnan(b);
}
W floor_word(double value)
{
    // Finite462ECF domain; original NaN/Inf CRT error handling is not supplied.
    if (!std::isfinite(value))
        throw SourceAllocationError(0x462ecf, "camera floor requires recovered finite CRT domain");
    const double whole = std::floor(value);
    // 462860 truncates into a signed QWORD and returns only EAX. Preserve low
    // DWORD wrap; it is not a signed32 conversion. Masked indefinite is0 low.
    constexpr double limit = 9223372036854775808.0;
    if (whole < -limit || whole >= limit)
        return 0;
    return static_cast<W>(static_cast<std::int64_t>(whole));
}
double remainder_one(double value)
{
    if (!std::isfinite(value))
        throw SourceAllocationError(0x46291a, "camera remainder requires recovered finite CRT domain");
    return std::fmod(value, 1.0);
}
float coordinate(const SourceCameraSplineRef &points, W index, W component, W site)
{
    // Do not wrap a normalized PMD offset into an unrelated low file address.
    return as_float(points.read_word(std::uint64_t(index) * 16 + component, site));
}
std::array<float, 4> weights_4282a0(float t)
{
    // PC53 nearest, explicit source binary32 stores. /fp:strict forbids FMA.
    const double squared = double(t) * t;
    const double cubed = squared * t;
    const double k = as_float(0x3e2aaaab);
    std::array<float, 4> out;
    out[3] = f32(cubed * k); // 4282B6
    const double triple = double(t) * 3.0;
    out[2] = f32(((((3.0 - triple) * t + 3.0) * t) + 1.0) * k);   // 4282E2
    out[1] = f32(((cubed * 3.0 - squared * 6.0) + 4.0) * k);      // 428303
    out[0] = f32((((squared * 3.0 - cubed) - triple) + 1.0) * k); // 42831E
    return out;
}
void linear_428460(const SourceCameraSplineRef &spline, float parameter, Vec3 &out)
{
    const auto points = spline.at_normalized_offset(spline.read_word(4, 0x428473));
    auto first = floor_word(double(parameter)); // 42847C/428484 after pointer capture
    const auto count = spline.read_word(8, 0x42848b);
    if (first >= count)
        first = 0;
    auto second = first + 1u;
    if (second >= count)
        second = 0;
    const double fraction = remainder_one(double(parameter)); // 4284A6
    const auto bx = coordinate(points, second, 0, 0x4284b6);
    const double dx = double(bx) - coordinate(points, first, 0, 0x4284b8);
    const auto by = coordinate(points, second, 4, 0x4284bb);
    const float dy = f32(double(by) - coordinate(points, first, 4, 0x4284be));
    const auto bz = coordinate(points, second, 8, 0x4284c7);
    const float dz = f32(double(bz) - coordinate(points, first, 8, 0x4284ca));
    const float mx = f32(dx * fraction), my = f32(double(dy) * fraction);
    const double mz = fraction * dz;
    const float x = f32(double(mx) + coordinate(points, first, 0, 0x4284e3));
    const double y = double(my) + coordinate(points, first, 4, 0x4284eb);
    out.x = x; // 4284F1, before the final reached Z read
    const float stored_y = f32(y);
    const double z = mz + coordinate(points, first, 8, 0x4284fc);
    out.y = stored_y; // 4284FF
    out.z = f32(z);   // 428508; undefined42850B stack W is not fabricated
}
CameraState &current(contact::player::Controls &controls, W site)
{
    if (controls.current >= controls.controllers.size())
        throw SourceAllocationError(site, "camera current controller outside actual bank");
    return controls.controllers[controls.current].camera;
}
void advance_428740(CameraSessionState &session, std::span<const std::uint8_t, 256> keys)
{
    auto &speed = session.path_speed;
    auto &parameter = session.path_parameter;
    speed = f32(double(speed) * as_float(0x3f733333)); // 42875A, before62D4A6 read
    if ((keys[0x26] & 0x80) != 0 && c0(speed, as_float(0x3ac49ba6)))
        speed = f32(double(speed) + as_float(0x37d1b717));
    if ((keys[0x28] & 0x80) != 0 && speed > as_float(0xbac49ba6))
        speed = f32(double(speed) - as_float(0x37d1b717));
    const float next = f32(double(parameter) + speed);
    parameter = next; // 4287B7 precedes the reached remainder/error boundary
    parameter = f32(c0(next, 0.0) ? double(next) + 1.0 : remainder_one(double(next)));
    if (c0(std::abs(double(speed)), as_float(0x35d1b717)))
        speed = 0;
}
} // namespace
void sample_source_camera_path_428520(const SourceCameraSplineRef &spline, float parameter, Vec3 &out)
{
    if (spline.read_word(12, 0x42852f) == 1)
    {
        linear_428460(spline, parameter, out);
        return;
    }
    const double shifted = double(parameter) + 3.0;
    const float spilled = f32(shifted); // 428553, floor still consumes unrounded +3
    auto index = floor_word(shifted);
    const float t = f32(remainder_one(double(spilled)));
    const auto points = spline.at_normalized_offset(spline.read_word(4, 0x42857c));
    const auto weights = weights_4282a0(t);
    const auto count = spline.read_word(8, 0x4285b0); // captured once, outside loop
    double x = 0, y = 0, z = 0;
    for (unsigned remaining = 4; remaining != 0; --remaining)
    {
        W selected = index;
        if (index >= count)
        {
            if (spline.read_word(16, 0x4285bf) == 2)
            {
                if (count == 0)
                    throw SourceAllocationError(0x4285c9, "source camera spline unsigned DIV by zero");
                selected = index % count;
            }
            else
                selected = count - 1u;
        }
        const auto weight = weights[remaining - 1];
        x = double(weight) * coordinate(points, selected, 0, 0x4285d7) + x;
        y = double(coordinate(points, selected, 4, 0x4285df)) * weight + y;
        z = double(coordinate(points, selected, 8, 0x4285e6)) * weight + z;
        if (index > 0)
            --index;
    }
    const float stored_z = f32(z), stored_y = f32(y); // 4285FC/428605
    out.x = f32(x);
    out.y = stored_y;
    out.z = stored_z; // 42860C/0F/12; W untouched
}
void follow_source_camera_path_402b90(contact::player::Controls &controls,
                                      std::array<CameraPresentationState, 4> &presentation, CameraSessionState &session,
                                      std::span<const std::uint8_t, 256> keyboard)
{
    (void)current(controls, 0x402b96);
    const auto *binding = presentation[controls.current].spline.source();
    if (!binding)
        throw SourceAllocationError(0x402bac, "402B90 requires the actual captured source camera spline");
    const auto spline = *binding; // ESI captured at402BA6, metadata only
    const float offset = f32(double(std::bit_cast<std::int32_t>(spline.read_word(20, 0x402bac))));
    const auto count = spline.read_word(8, 0x402baf);
    Vec3 result; // only defined XYZ imported; retail stack W remains outside contract
    sample_source_camera_path_428520(spline, f32(double(count) * session.path_parameter), result);
    auto &position = current(controls, 0x402bcc).position;
    position.x = result.x;
    position.y = result.y;
    position.z = result.z;
    position.y = f32(double(position.y) + 500.0); // 402C02 committed before second call
    const auto target_count = std::bit_cast<std::int32_t>(spline.read_word(8, 0x402c0e));
    const float distance = f32((double(offset) * as_float(0x3a83126f) + session.path_parameter) * target_count);
    sample_source_camera_path_428520(spline, distance, result);
    auto &target = current(controls, 0x402c25).target;
    target.x = result.x;
    target.y = result.y;
    target.z = result.z;
    target.y = f32(double(target.y) + 500.0);
    advance_428740(session, keyboard); // actual live62D4A6/A8 read after both samples
    current(controls, 0x402c60).mode = 3;
}
void prepare_source_camera_view_402e90(const std::shared_ptr<RaceSessionPlatform> &platform)
{
    if (!platform)
        throw SourceAllocationError(0x402e90, "source camera platform missing");
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry())
        throw SourceAllocationError(0x402e90, "source camera requires source platform owner");
    auto &world = platform->state();
    auto &camera = current(world.controls, 0x402e90);
    camera_rebuild_projection(camera, world.shared_model_identity);
    world.frame_projection = camera.projection;
    world.inverse_depth_scale = camera.inverse_depth_scale;
    world.depth_offset = camera.depth_offset;
    const auto mode = current(world.controls, 0x402e95).mode;
    if (mode == 1 || mode == 2)
        camera_free_orientation_402ad0(current(world.controls, 0x402ec7));
    else if (mode == 3)
    {
        if (find_source_camera_spline_428800(platform, 1))
        {
            // The returned lookup pointer is only a presence gate.402B90 reads
            // the independently bound camera+1A8, never substitutes this lookup.
            follow_source_camera_path_402b90(world.controls, world.camera_presentation, world.camera_session,
                                             menu_keyboard_bytes(world.startup));
        }
        else
        {
            CameraEffectTargets targets;
            for (unsigned i = 0; i < 6; ++i)
            {
                targets.vehicles[i] = &world.vehicles.physics[i];
                targets.active[i] = static_cast<W>(world.vehicles.race[i].active);
            }
            camera_select_mode(current(world.controls, 0x402eb9), 1, camera_vehicle_reset_adapter(targets));
        }
    }
    auto &selected = current(world.controls, 0x402ecc);
    camera_finish_view_402ecc(selected, world.camera_presentation[world.controls.current], &world.view_matrices);
}
} // namespace sgn::retail
