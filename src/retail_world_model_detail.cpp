#include "sgn/retail_world_model_detail.hpp"

namespace sgn::retail
{
void select_world_model_detail(Vec3 position, const CameraState &camera, CameraVector forward, std::uint32_t rate,
                               std::uint32_t &current, std::uint32_t &previous)
{
    const double x = double(position.x) - camera.position.x, y = double(position.y) - camera.position.y,
                 z = double(position.z) - camera.position.z;
    const double distance = (((z * forward.z + y * forward.y) + x * forward.x) * double(3.2f)) / camera.lens;
    previous = current;
    // x87 unordered comparisons take the same branches as <= in the source.
    if (!(distance > 5000.0))
        current = distance > 3000.0 ? 1u : 0u;
    else
        current = distance > 10000.0 ? 3u : 2u;
    if (rate < 25)
        current += 2;
    else if (rate < 50)
        ++current;
    if (current > 3)
        current = 3;
}
} // namespace sgn::retail
