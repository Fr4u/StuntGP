#pragma once
#include "sgn/assets.hpp"
#include <cmath>
#include <cstddef>
#include <array>
#include <optional>
#include <span>
#include <algorithm>
#include <stdexcept>

namespace sgn
{
// New presentation interface. Coordinates and poses come from the recovered
// simulation; this interface supplies no movement or collision behavior.
struct Pose
{
    Vec3 right{1, 0, 0}, up{0, 1, 0}, forward{0, 0, 1}, position{};
    float scale{1};
};
struct SceneInstance
{
    std::size_t mesh{};
    Pose pose{};
    struct Part
    {
        Pose pose{};
        bool visible{true};
    };
    // Empty for a flattened mesh. Otherwise these are local-to-instance poses.
    std::span<const Part> parts{};
};
inline Vec3 transform_direction(const Pose &p, Vec3 v) noexcept
{
    return (p.right * v.x + p.up * v.y + p.forward * v.z) * p.scale;
}
inline Pose compose_pose(const Pose &parent, const Pose &child) noexcept
{
    return {transform_direction(parent, child.right) * child.scale, transform_direction(parent, child.up) * child.scale,
            transform_direction(parent, child.forward) * child.scale,
            transform_direction(parent, child.position) + parent.position, 1};
}
struct InspectionFrustum
{
    float distance{}, near_plane{}, far_plane{};
};
// Modern model inspection only; this is not the recovered gameplay camera.
inline InspectionFrustum inspection_frustum(float radius, float orbit_distance, unsigned width, unsigned height)
{
    if (!std::isfinite(radius) || radius <= 0 || !std::isfinite(orbit_distance) || orbit_distance <= 0 || !width ||
        !height)
        throw std::invalid_argument("invalid inspection frustum");
    const double fitted = double(orbit_distance) * std::max(1.0, double(height) / width);
    InspectionFrustum result{static_cast<float>(fitted), std::max(0.001f, radius * 0.0005f),
                             static_cast<float>(std::max(double(radius) * 60, fitted + double(radius) * 4))};
    if (!std::isfinite(result.far_plane) || result.far_plane <= result.near_plane)
        throw std::invalid_argument("inspection frustum outside representable range");
    return result;
}
inline double pose_determinant(const Pose &p) noexcept
{
    return (double(p.right.y) * p.up.z - double(p.right.z) * p.up.y) * p.forward.x +
           (double(p.right.z) * p.up.x - double(p.right.x) * p.up.z) * p.forward.y +
           (double(p.right.x) * p.up.y - double(p.right.y) * p.up.x) * p.forward.z;
}
inline std::optional<std::array<Vec3, 3>> normal_basis(const Pose &p) noexcept
{
    const double divisor = pose_determinant(p) * p.scale;
    if (!std::isfinite(divisor) || divisor == 0)
        return std::nullopt;
    auto cofactor = [&](Vec3 a, Vec3 b) {
        return Vec3{static_cast<float>((double(a.y) * b.z - double(a.z) * b.y) / divisor),
                    static_cast<float>((double(a.z) * b.x - double(a.x) * b.z) / divisor),
                    static_cast<float>((double(a.x) * b.y - double(a.y) * b.x) / divisor)};
    };
    std::array result{cofactor(p.up, p.forward), cofactor(p.forward, p.right), cofactor(p.right, p.up)};
    for (auto v : result)
        if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z))
            return std::nullopt;
    return result;
}
inline bool valid_pose(const Pose &p) noexcept
{
    auto finite = [](Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); };
    if (!finite(p.right) || !finite(p.up) || !finite(p.forward) || !finite(p.position) || !std::isfinite(p.scale))
        return false;
    // Retail's quantized rotations need not be exactly orthonormal. Preserve
    // the supplied affine basis, including zero axes. Rank-two transforms can
    // preserve visible triangles; lighting must not reject their geometry.
    return finite(p.right * p.scale) && finite(p.up * p.scale) && finite(p.forward * p.scale);
}
inline bool mirrored_pose(const Pose &p) noexcept
{
    return pose_determinant(p) * p.scale < 0;
}
// Presentation lighting only. A common positive rescale of the cofactor matrix
// preserves normal directions, works for rank two and avoids division by a
// vanishing determinant. Rank-zero/one geometry has no area and gets zero normals.
inline std::array<Vec3, 3> lighting_normal_basis(const Pose &p) noexcept
{
    const std::array<Vec3, 3> axes{p.right, p.up, p.forward};
    std::array<std::array<double, 3>, 3> cofactors{};
    double maximum = 0;
    for (unsigned i = 0; i < 3; ++i)
    {
        const auto a = axes[(i + 1) % 3], b = axes[(i + 2) % 3];
        cofactors[i] = {double(a.y) * b.z - double(a.z) * b.y, double(a.z) * b.x - double(a.x) * b.z,
                        double(a.x) * b.y - double(a.y) * b.x};
        for (double value : cofactors[i])
            maximum = std::max(maximum, std::abs(value));
    }
    std::array<Vec3, 3> result{};
    if (maximum == 0 || p.scale == 0)
        return result;
    const double divisor = mirrored_pose(p) ? -maximum : maximum;
    for (unsigned i = 0; i < 3; ++i)
        result[i] = {static_cast<float>(cofactors[i][0] / divisor), static_cast<float>(cofactors[i][1] / divisor),
                     static_cast<float>(cofactors[i][2] / divisor)};
    return result;
}
inline void validate_instances(std::span<const SceneInstance> instances, std::size_t meshes)
{
    for (const auto &instance : instances)
    {
        if (instance.mesh >= meshes || !valid_pose(instance.pose))
            throw std::invalid_argument("invalid scene mesh index or nonfinite affine pose");
        for (const auto &part : instance.parts)
            if (part.visible && !valid_pose(compose_pose(instance.pose, part.pose)))
                throw std::invalid_argument("nonfinite visible part pose");
    }
}
} // namespace sgn
