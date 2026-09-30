#include "sgn/retail_camera_spline.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
float f32(double v)
{
    return static_cast<float>(v);
}
using sgn::bit_reinterpret::as_float;
} // namespace
void camera_spline_chase_tick(CameraState &camera, const CameraVehicle &vehicle, float &parameter, SplineView spline,
                              CameraVector gravity, CameraCollision collision)
{
    if (camera.mode != 7)
        throw FormatError("spline chase requires source camera mode7");
    if (!std::isfinite(parameter) || !std::isfinite(vehicle.progress) || spline.descriptor.point_count == 0)
        throw FormatError("spline chase requires finite progress and authored route");
    camera.previous_target = camera.target; // 40445D..404478
    camera_track_target(camera, vehicle);   // 404FAD..404FB3
    double difference = double(vehicle.progress) - parameter;
    difference -= vehicle.progress < parameter ? -2.0 : 2.0;
    if (std::abs(difference) > as_float(0x3c23d70a))
    {
        double step = difference * as_float(0x3ca3d70a);
        if (std::abs(step) > as_float(0x3e4ccccd))
            step = step < 0 ? double(as_float(0xbe4ccccd)) : double(as_float(0x3e4ccccd));
        parameter = f32(double(parameter) + step);
    }
    const double scaled = double(parameter) * as_float(0x3c23d70a);
    const float stored = f32(scaled);
    const double fraction = scaled < 0 ? 1.0 - std::fmod(-double(stored), 1.0) : std::fmod(double(stored), 1.0);
    const auto sample = camera_sample_path(spline, f32(double(spline.descriptor.point_count) * fraction));
    camera.position.x = sample.position.x;
    camera.position.y = sample.position.y;
    camera.position.z = sample.position.z;
    // 405106..405146 retain both horizontal differences, root and products on
    // x87 until the clamp. Only gravity X/Y products are stored before subtract.
    const double dx = double(camera.position.x) - vehicle.position.x,
                 dz = double(camera.position.z) - vehicle.position.z;
    double height = std::sqrt(dz * dz + dx * dx) * as_float(0x3dcccccd) * as_float(0x3dcccccd);
    height *= height;
    if (height < 1000)
        height = 1000;
    else if (height > 3000)
        height = 3000;
    camera.position.x = f32(double(camera.position.x) - f32(double(gravity.x) * height));
    camera.position.y = f32(double(camera.position.y) - f32(double(gravity.y) * height));
    camera.position.z = f32(double(camera.position.z) - double(gravity.z) * height);
    camera_move(camera, vehicle, collision);
    camera_translate(camera, vehicle);
}
} // namespace sgn::retail
