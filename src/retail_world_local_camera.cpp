#include "sgn/retail_world_model_producer.hpp"
#include <cmath>

namespace sgn::retail
{
namespace
{
float f32(double value) noexcept
{
    return static_cast<float>(value);
}
} // namespace
void world_model_local_camera(VehicleVector &local, VehicleVector &normalized, Vec3 camera, Vec3 outer,
                              const CameraMatrix &inverse_outer, Vec3 part, Vec3 pivot,
                              const CameraMatrix *inverse_part, std::uint32_t part_count, WorldLocalCameraAudit *audit)
{
    const double dx = double(camera.x) - outer.x, dy = double(camera.y) - outer.y, dz = double(camera.z) - outer.z;
    const auto rotate = [&](unsigned c) {
        return f32((dz * inverse_outer[c + 8] + dy * inverse_outer[c + 4]) + dx * inverse_outer[c]);
    };
    float x = f32(double(rotate(0)) - part.x), y = f32(double(rotate(1)) - part.y), z = f32(double(rotate(2)) - part.z);
    if (part_count > 1)
    {
        if (!inverse_part)
            throw FormatError("source multipart local camera has no inverse matrix");
        x = f32(double(x) - pivot.x);
        y = f32(double(y) - pivot.y);
        // 42B34D stores but does not pop Z. Only the first result reads the
        // unrounded register; subsequent results reload its binary32 store.
        const double first_z = double(z) - pivot.z;
        z = f32(first_z);
        const auto &m = *inverse_part;
        local.x = f32(((first_z * m[8] + double(y) * m[4]) + double(x) * m[0]) + pivot.x);
        local.y = f32(((double(z) * m[9] + double(y) * m[5]) + double(x) * m[1]) + pivot.y);
        local.z = f32(((double(z) * m[10] + double(y) * m[6]) + double(x) * m[2]) + pivot.z);
    }
    else
    {
        local.x = x;
        local.y = y;
        local.z = z;
    }
    // 44ED5E/66 tests C3 alone: zero OR unordered squared length takes the
    // three-zero branch. The general vehicle helper only handles equality;
    // retain its arithmetic for the ordered case without changing other users.
    if (std::isnan(local.x) || std::isnan(local.y) || std::isnan(local.z))
        normalized = {0.0f, 0.0f, 0.0f, local.w};
    else
        normalized = vehicle_normalize(local);
    if (audit)
    {
        for (unsigned i = 0; i < 12; ++i)
            audit->local[i] = 0;
        if (part_count <= 1)
            for (unsigned i = 12; i < 16; ++i)
                audit->local[i] = 1;
        audit->normalized = audit->local;
    }
}
} // namespace sgn::retail
