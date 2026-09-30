#pragma once
#include "sgn/assets.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace sgn::retail
{
struct ProjectionPoint
{
    float x{}, y{}, z{}, camera_z{};
    std::uint16_t inside{};
};
struct ProjectionParameters
{
    float x{}, y{}, z{}, offset_z{}, clip_near{};
};
struct ViewportPoint
{
    float x{}, y{}, reciprocal_depth{};
};

inline std::uint32_t projection_bits(double value) noexcept
{
    return std::bit_cast<std::uint32_t>(static_cast<float>(value));
}
inline std::uint16_t projection_mask(double x, float y, double z, double camera_z, float clip_near) noexcept
{
    // 004295DE..0042964C. X and camera Z remain in x87 registers; Y and
    // negative camera Z are explicitly rounded stores in this instruction path.
    return static_cast<std::uint16_t>(2u | (projection_bits(double(clip_near) - z) >> 31) |
                                      ((projection_bits(-camera_z - x) >> 29) & 4u) |
                                      ((projection_bits(x - camera_z) >> 28) & 8u) |
                                      ((projection_bits(double(static_cast<float>(-camera_z)) - y) >> 27) & 16u) |
                                      ((projection_bits(double(y) - camera_z) >> 26) & 32u));
}
// Scalar reference of full 00429520, not a generic DirectX projection. All
// products/additions retain the retail 53-bit x87 precision until its stores.
inline ProjectionPoint project_retail_vertex(Vec3 vertex, const std::array<float, 16> &matrix,
                                             const ProjectionParameters &p) noexcept
{
    const auto component = [&](unsigned c) {
        return ((double(matrix[c]) * vertex.x + double(matrix[c + 4]) * vertex.y) + double(matrix[c + 8]) * vertex.z) +
               matrix[c + 12];
    };
    const float x = static_cast<float>(component(0)), y = static_cast<float>(component(1));
    const double camera_z = component(2), px = double(p.x) * x, pz = double(p.z) * camera_z + p.offset_z;
    const float py = static_cast<float>(double(p.y) * y);
    return {static_cast<float>(px), py, static_cast<float>(pz), static_cast<float>(camera_z),
            projection_mask(px, py, pz, camera_z, p.clip_near)};
}
// 00424AA0 + 00429A40: +0.5 viewport scales and 32/(stored_camera_z+1).
// This is separate from the projection's near/far coefficients by design.
inline ViewportPoint retail_viewport(const ProjectionPoint &p, unsigned width, unsigned height) noexcept
{
    const double inverse = 1.0 / (double(p.camera_z) + 1.0);
    const float center_x = static_cast<float>(double(width) * 0.5), center_y = static_cast<float>(double(height) * 0.5);
    const float scale_x = static_cast<float>(double(center_x) + 0.5),
                scale_y = static_cast<float>(double(center_y) + 0.5);
    return {static_cast<float>((inverse * p.x) * scale_x + center_x),
            static_cast<float>((inverse * p.y) * scale_y + center_y), static_cast<float>(inverse * 32.0)};
}
struct ClipCorner
{
    float u{}, v{};
    std::uint32_t color{};
    ProjectionPoint point;
};
// Full valid-plane behavior of 004272A0. Preserve attributes of intersections;
// no arbitrary epsilon, parameter clamp, or replacement clip plane.
inline ClipCorner retail_edge_intersection(const ClipCorner &a, const ClipCorner &b, unsigned plane, float clip_near)
{
    const auto distance = [&](const ProjectionPoint &p) -> double {
        switch (plane)
        {
        case 1:
            return double(p.z) - clip_near;
        case 4:
            return (double(p.z) + p.x) + 1.0;
        case 8:
            return (double(p.x) - p.z) - 1.0;
        case 16:
            return (double(p.z) + p.y) + 1.0;
        case 32:
            return (double(p.y) - p.z) - 1.0;
        default:
            throw FormatError("unsupported retail clipping plane");
        }
    };
    const double da = distance(a.point), delta = distance(b.point) - da;
    const double t = std::abs(delta) > 1.0 ? -(da / delta) : 1.0;
    const auto mix = [&](float first, float last) { return static_cast<float>((double(last) - first) * t + first); };
    ClipCorner out{mix(a.u, b.u),
                   mix(a.v, b.v),
                   0,
                   {mix(a.point.x, b.point.x), mix(a.point.y, b.point.y), mix(a.point.z, b.point.z),
                    mix(a.point.camera_z, b.point.camera_z), 0}};
    const double scaled = std::trunc(t * 256.0);
    // __ftol returns a signed 64-bit integer; callers use its low 32 bits.
    if (!std::isfinite(scaled) || scaled < -9223372036854775808.0 || scaled >= 9223372036854775808.0)
        throw FormatError("nonrepresentable retail clip color interpolation");
    const auto fraction = static_cast<std::uint32_t>(static_cast<std::int64_t>(scaled));
    for (unsigned shift = 0; shift < 32; shift += 8)
    {
        const auto first = (a.color >> shift) & 255u, last = (b.color >> shift) & 255u;
        const auto product = std::bit_cast<std::int32_t>((last - first) * fraction);
        out.color |= ((static_cast<std::uint32_t>(product >> 8) + first) & 255u) << shift;
    }
    out.point.inside = projection_mask(out.point.x, out.point.y, out.point.z, out.point.camera_z, clip_near);
    return out;
}
} // namespace sgn::retail
