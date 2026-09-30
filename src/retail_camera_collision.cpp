#include "sgn/retail_camera_collision.hpp"
#include <cmath>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
float f32(double value)
{
    return static_cast<float>(value);
}
Vec3 xyz(CameraVector v)
{
    return {v.x, v.y, v.z};
}
CameraVector vector(Vec3 v, float w = 0)
{
    return {v.x, v.y, v.z, w};
}
} // namespace
CameraSweepResult camera_sweep(const contact::TrackGeometry &track, const CameraSweep &input)
{
    auto finite = [](CameraVector v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); };
    if (!finite(input.position) || !finite(input.velocity) || !std::isfinite(input.radius) || input.radius <= 0)
        throw std::domain_error("invalid retail camera sweep input");
    CameraSweepResult result;
    result.position = input.position;
    CameraVector direction = input.velocity;
    const double length = std::sqrt((double(direction.x) * direction.x + double(direction.y) * direction.y) +
                                    double(direction.z) * direction.z);
    camera_normalize(direction); // same arithmetic as 0044eda0, without its length output
    const float stored_length = f32(length);
    const double count = std::ceil(double(stored_length) / (double(input.radius) + input.radius));
    if (count > 65536)
        throw std::domain_error("camera sweep exceeds bounded query workload");
    if (count == 0)
        return result; // retail computes unused infinities, then takes the zero-step return
    const double fraction = 1 / count;
    const CameraVector increment{f32(fraction * input.velocity.x), f32(fraction * input.velocity.y),
                                 f32(fraction * input.velocity.z), 0};
    auto position = input.position;
    for (std::uint32_t step = 0; step < static_cast<std::uint32_t>(count); ++step)
    {
        const CameraVector endpoint{f32(double(position.x) + increment.x), f32(double(position.y) + increment.y),
                                    f32(double(position.z) + increment.z), position.w};
        const double dz = double(endpoint.z) - position.z;
        const Vec3 motion{f32(double(endpoint.x) - position.x), f32(double(endpoint.y) - position.y), f32(dz)};
        const float step_length =
            f32(std::sqrt((dz * motion.z + double(motion.y) * motion.y) + double(motion.x) * motion.x));
        auto list = contact::find_polygon_list(track, xyz(endpoint), input.radius);
        for (std::size_t at = list;; ++at)
        {
            const auto id = track.polygon_lists.at(at);
            if (id == 0xffff)
                break;
            const contact::PolygonQuery query{xyz(position), xyz(endpoint),        motion,      xyz(direction),
                                              input.radius,  input.radius_squared, step_length, false};
            const auto hit = contact::intersect_polygon(track, id, query);
            if (hit.hit)
            {
                result.hit = true;
                result.position = position;
                result.normal = vector(hit.normal);
                result.center = vector(hit.center);
                result.surface = vector(hit.surface);
                result.polygon = id;
                return result;
            }
        }
        position = endpoint;
    }
    result.position = position;
    return result;
}
CameraVector camera_resolve_collision(const contact::TrackGeometry &track, const CameraSweep &input)
{
    auto sweep = input;
    for (unsigned attempt = 0; attempt < 33; ++attempt)
    {
        const auto result = camera_sweep(track, sweep);
        sweep.position = result.position;
        if (!result.hit)
            break;
        auto motion = xyz(sweep.velocity);
        contact::reflect_motion(xyz(result.normal), 0.01f, motion);
        sweep.velocity = vector(motion, sweep.velocity.w);
    }
    return sweep.position;
}
CameraCollision camera_collision_adapter(const contact::TrackGeometry &track)
{
    // The type-erased pointer is borrowed; neither callback nor solver mutates it.
    return {const_cast<contact::TrackGeometry *>(&track), [](void *context, const CameraSweep &input) {
                return camera_resolve_collision(*static_cast<const contact::TrackGeometry *>(context), input);
            }};
}
} // namespace sgn::retail
