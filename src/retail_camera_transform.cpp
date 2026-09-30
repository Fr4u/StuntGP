#include "sgn/retail_camera_transform.hpp"

namespace sgn::retail
{
namespace
{
float f32(double value) noexcept
{
    return static_cast<float>(value);
}
void scale(CameraMatrix &m, CameraVector value) noexcept
{
    // 0044eeb0 is a 3x3-only product. Its y,x,z order and additions of zero
    // are retained; translation and the fourth lanes are not scaled.
    const CameraMatrix s{value.x, 0, 0, 0, 0, value.y, 0, 0, 0, 0, value.z, 0, 0, 0, 0, 1};
    for (unsigned col = 0; col < 3; ++col)
    {
        const unsigned i = col * 4;
        const double x = m[i], y = m[i + 1], z = m[i + 2];
        for (unsigned row = 0; row < 3; ++row)
            m[i + row] = f32((double(s[4 + row]) * y + double(s[row]) * x) + double(s[8 + row]) * z);
    }
}
void translate(CameraMatrix &m, CameraVector value) noexcept
{
    // 0044f150 stores each separate translation operation to binary32.
    m[12] = f32(double(value.x) + m[12]);
    m[13] = f32(double(value.y) + m[13]);
    m[14] = f32(double(value.z) + m[14]);
}
CameraVector negate(CameraVector v) noexcept
{
    return {-v.x, -v.y, -v.z, v.w};
}
void multiply(CameraMatrix &m, const CameraMatrix &r) noexcept
{
    // 0044f280 has a different four-term order in every result row.
    for (unsigned col = 0; col < 4; ++col)
    {
        const unsigned i = col * 4;
        const double x = m[i], y = m[i + 1], z = m[i + 2], w = m[i + 3];
        m[i] = f32(((w * r[12] + double(r[8]) * z) + double(r[4]) * y) + double(r[0]) * x);
        m[i + 1] = f32(((w * r[13] + double(r[5]) * y) + z * r[9]) + x * r[1]);
        m[i + 2] = f32(((double(r[10]) * z + y * r[6]) + w * r[14]) + double(r[2]) * x);
        m[i + 3] = f32(((w * r[15] + z * r[11]) + double(r[3]) * x) + y * r[7]);
    }
}
} // namespace
CameraMatrix camera_model_view(const CameraState &camera, const CameraModelTransform &model, std::uint32_t part_count,
                               const CameraModelTransform *part) noexcept
{
    CameraMatrix result{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; // 0044ee10
    if (part_count > 1 && part)
    {
        scale(result, part->scale);
        translate(result, negate(part->pivot));
        multiply(result, part->rotation);
        translate(result, part->pivot);
        translate(result, part->position);
    }
    scale(result, model.scale);
    translate(result, model.pivot);
    multiply(result, model.rotation);
    translate(result, negate(model.pivot));
    const CameraVector relative{f32(double(model.position.x) - camera.position.x),
                                f32(double(model.position.y) - camera.position.y),
                                f32(double(model.position.z) - camera.position.z), 0};
    translate(result, relative);
    multiply(result, camera.world_to_camera_rotation);
    return result;
}
CameraMatrix camera_world_view(const CameraState &camera) noexcept
{
    return camera_model_view(camera, CameraModelTransform{});
}
} // namespace sgn::retail
