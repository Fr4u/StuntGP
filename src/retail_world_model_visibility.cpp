#include "sgn/retail_world_model_visibility.hpp"

namespace sgn::retail
{
namespace
{
float f32(double value) noexcept
{
    return static_cast<float>(value);
}
bool negative(float value) noexcept
{
    return (std::bit_cast<std::uint32_t>(value) & 0x80000000u) != 0;
}
CameraVector vector(Vec3 value, std::uint32_t w = 0) noexcept
{
    return {value.x, value.y, value.z, std::bit_cast<float>(w)};
}
} // namespace
std::uint32_t classify_world_model_radius_fields(std::uint16_t radius, const VehicleVector &bound_center, bool whole,
                                                 const ModelTransform &outer, WorldModelRadiusPartRead part, Vec3 pivot,
                                                 const CameraMatrix &rotation, Vec3 camera, CameraVector forward,
                                                 std::span<const CameraVector, 4> planes, float far)
{
    float x, y, z;
    if (whole)
    {
        x = f32(double(outer.position.x) - camera.x);
        y = f32(double(outer.position.y) - camera.y);
        z = f32(double(outer.position.z) - camera.z);
    }
    else
    {
        const double nx = -double(pivot.x), ny = -double(pivot.y), nz = -double(pivot.z);
        float px = f32((nz * rotation[8] + ny * rotation[4]) + nx * rotation[0]);
        float py = f32((nx * rotation[1] + nz * rotation[9]) + ny * rotation[5]);
        double pz = (nx * rotation[2] + nz * rotation[10]) + ny * rotation[6];
        px = f32(double(px) + pivot.x);
        py = f32(double(py) + pivot.y);
        pz += pivot.z;
        px = f32(double(px) + part.read(0));
        py = f32(double(py) + part.read(4));
        pz += part.read(8);
        const double bx = (double(bound_center.y) * rotation[4] + double(bound_center.z) * rotation[8]) +
                          double(bound_center.x) * rotation[0];
        const float by = f32((double(bound_center.y) * rotation[5] + double(bound_center.z) * rotation[9]) +
                             double(bound_center.x) * rotation[1]);
        const float bz = f32((double(bound_center.y) * rotation[6] + double(bound_center.z) * rotation[10]) +
                             double(bound_center.x) * rotation[2]);
        px = f32(double(px) + bx);
        py = f32(double(py) + by);
        pz += bz;
        px = f32(double(px) + outer.position.x);
        py = f32(double(py) + outer.position.y);
        pz += outer.position.z;
        x = f32(double(px) - camera.x);
        y = f32(double(py) - camera.y);
        z = f32(pz - camera.z);
    }
    const auto extent = [&](float part_scale, float outer_scale) {
        return f32((double(part_scale) * outer_scale) * float(radius));
    };
    const auto dot = [&](const CameraVector &axis) {
        return (double(z) * axis.z + double(y) * axis.y) + double(x) * axis.x;
    };
    const float extent_z = extent(part.read(24), outer.scale.z);
    const double depth = dot(forward);
    const float stored_depth = f32(depth);
    if (negative(f32(depth + extent_z)))
        return 1;
    const float far_difference = f32(double(stored_depth) - far);
    if (!negative(far_difference))
        return 1;
    const float extent_x = extent(part.read(16), outer.scale.x);
    std::array<float, 4> sides;
    for (unsigned p = 0; p < 2; ++p)
    {
        const double value = dot(planes[p]);
        sides[p] = f32(value);
        if (!negative(f32(value - extent_x)))
            return 1;
    }
    const float extent_y = extent(part.read(20), outer.scale.y);
    // 427F39..427F83 subtract X extent for both vertical rejection tests.
    // The later inside tests use Y extent. This asymmetry is intentional source.
    for (unsigned p = 2; p < 4; ++p)
    {
        const double value = dot(planes[p]);
        sides[p] = f32(value);
        if (!negative(f32(value - extent_x)))
            return 1;
    }
    if (negative(f32(double(stored_depth) - extent_z)))
        return 2;
    for (unsigned p = 0; p < 4; ++p)
        if (!negative(f32(double(sides[p]) + (p < 2 ? extent_x : extent_y))))
            return 2;
    return negative(far_difference) ? 0u : 2u;
}
std::uint32_t classify_world_model_radius(std::uint16_t radius, const VehicleVector &center, bool whole,
                                          const ModelTransform &outer, const ModelTransform &part, Vec3 pivot,
                                          const CameraMatrix &rotation, Vec3 camera, CameraVector forward,
                                          std::span<const CameraVector, 4> planes, float far)
{
    const WorldModelRadiusPartRead fields{&part, [](const void *owner, unsigned offset) {
                                              const auto &value = *static_cast<const ModelTransform *>(owner);
                                              switch (offset)
                                              {
                                              case 0:
                                                  return value.position.x;
                                              case 4:
                                                  return value.position.y;
                                              case 8:
                                                  return value.position.z;
                                              case 16:
                                                  return value.scale.x;
                                              case 20:
                                                  return value.scale.y;
                                              case 24:
                                                  return value.scale.z;
                                              default:
                                                  throw FormatError("unexpected model radius component");
                                              }
                                          }};
    return classify_world_model_radius_fields(radius, center, whole, outer, fields, pivot, rotation, camera, forward,
                                              planes, far);
}
std::uint32_t classify_world_model_eight_points(std::span<const VehicleVector *const, 8> points,
                                                const CameraMatrix &matrix, const ProjectionParameters &projection,
                                                float far)
{
    std::uint32_t common = 63, any = 0;
    for (const auto *point : points)
    {
        if (!point)
            throw FormatError("null source model bound point");
        const auto component = [&](unsigned c) {
            return ((double(point->y) * matrix[c + 4] + double(point->z) * matrix[c + 8]) +
                    double(point->x) * matrix[c]) +
                   matrix[c + 12];
        };
        const double z = component(2), x = component(0) * projection.x;
        const float y = f32(component(1) * projection.y);
        const double projected_z = double(projection.z) * z + projection.offset_z;
        const float negative_z = f32(-z);
        std::uint32_t mask = 63;
        if (!(projected_z > projection.clip_near))
            mask = 62;
        if (projected_z >= far)
            mask &= ~2u;
        if (!(x > negative_z))
            mask &= ~4u;
        if (x >= z)
            mask &= ~8u;
        if (!(y > negative_z))
            mask &= ~16u;
        if (y >= z)
            mask &= ~32u;
        if (mask == 63 && common != mask)
            return 2;
        any |= mask;
        common &= mask;
    }
    return common == 63 ? 0 : any == 63 ? 2 : 1;
}
std::uint32_t classify_world_model_visibility(ModelResource &model, unsigned source_lod, std::int32_t part_index,
                                              const ModelTransform &outer, const CameraState &camera,
                                              const CameraPresentationState &presentation,
                                              const CameraLifecycle &lifecycle, const ViewMatrixArena &arena,
                                              const CameraMatrix &identity, const ProjectionParameters &projection,
                                              CameraMatrix &bound_matrix)
{
    if (part_index < -1)
        throw FormatError("invalid source model visibility part");
    const auto lod = model.source_lod_index(source_lod);
    const bool whole = part_index == -1;
    const auto index = whole ? 0u : static_cast<unsigned>(part_index);
    const auto part = model.part(lod, index);
    const auto bounds = whole ? model.source_model_bounds() : model.source_part_bounds(lod, index);
    if (bounds[0])
    {
        const auto &center = model.source_bound_vertex(bounds[1]);
        const auto &rotation = whole ? identity : resolve_model_matrix(arena, identity, part.transform.tail[0]);
        return classify_world_model_radius(bounds[0], center, whole, outer, part.transform, part.pivot, rotation,
                                           {camera.position.x, camera.position.y, camera.position.z},
                                           presentation.forward_axis, presentation.planes, lifecycle.clip_far);
    }
    if (!bounds[1])
        return 2;
    const auto &pivot = model.source_model_vector_10();
    const CameraModelTransform outer_view{vector(outer.position, outer.position_w),
                                          vector(outer.scale, outer.scale_w),
                                          {pivot.x, pivot.y, pivot.z, pivot.w},
                                          resolve_model_matrix(arena, identity, outer.tail[1])};
    CameraModelTransform part_view;
    const auto count = static_cast<std::uint32_t>(model.part_count(lod));
    if (!whole && count > 1)
        part_view = {vector(part.transform.position, part.transform.position_w),
                     vector(part.transform.scale, part.transform.scale_w), vector(part.pivot),
                     resolve_model_matrix(arena, identity, part.transform.tail[0])};
    auto bound_camera = camera;
    bound_camera.world_to_camera_rotation = resolve_model_matrix(arena, identity, camera.world_to_camera_reference);
    bound_matrix = camera_model_view(bound_camera, outer_view, count, whole ? nullptr : &part_view);
    std::array<const VehicleVector *, 8> points;
    for (unsigned i = 0; i < 8; ++i)
        points[i] = &model.source_bound_vertex(std::size_t(bounds[1]) + i);
    auto live_projection = projection;
    live_projection.clip_near = lifecycle.clip_near;
    return classify_world_model_eight_points(points, bound_matrix, live_projection, lifecycle.clip_far);
}
} // namespace sgn::retail
