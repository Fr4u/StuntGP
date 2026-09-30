#include "sgn/retail_world_track_parts.hpp"

namespace sgn::retail
{
namespace
{
CameraVector vector(Vec3 value, std::uint32_t w = 0) noexcept
{
    return {value.x, value.y, value.z, std::bit_cast<float>(w)};
}
WorldModelLightingPose light_pose(WorldTrackModelBinding binding, unsigned lod, unsigned index,
                                  const ViewMatrixArena &matrices, const CameraMatrix &identity,
                                  const WorldModelViewScratch &scratch)
{
    if (!binding.outer)
        throw FormatError("source track descriptor has no bound outer transform");
    const auto part = binding.model.part(lod, index);
    const auto count = static_cast<std::uint32_t>(binding.model.part_count(lod));
    return {scratch.normalized_camera,
            binding.outer->position,
            part.transform.position,
            resolve_model_matrix(matrices, identity, binding.outer->tail[2]),
            count > 1 ? &resolve_model_matrix(matrices, identity, part.transform.tail[1]) : nullptr,
            count};
}
WorldModelLightingPart light_part(ModelResource &model, unsigned lod, unsigned index)
{
    return {model.source_model_ambience(), model.source_part_flags_02(lod, index), model.source_coordinates(lod, index),
            model.source_vertices(lod, index), model.source_normals()};
}
} // namespace
void initialize_world_track_shading(WorldTrackModelBinding binding, const ViewMatrixArena &matrices,
                                    const CameraMatrix &identity, WorldModelViewScratch &scratch, std::uint32_t quality,
                                    std::uint32_t &initialized, WorldModelLightingBinding lighting)
{
    if (quality == 1 || initialized == 1)
        return;
    initialized = 1;
    lighting.stride = 1;
    const auto lod = binding.model.source_lod_index(0);
    const auto count = binding.model.part_count(lod);
    for (unsigned i = 1; i < count; ++i)
    {
        if ((binding.model.source_part_flags_02(lod, i) & 4) == 0)
            continue;
        shade_world_model_diffuse(light_pose(binding, lod, i, matrices, identity, scratch),
                                  light_part(binding.model, lod, i), lighting.lights, lighting.selection,
                                  lighting.prepared, lighting.ambient, lighting.frame_stamp, lighting.stride);
    }
}
void produce_world_track_parts(WorldTrackModelBinding binding, const ModelTransform &persistent_template,
                               const CameraState &camera, const CameraPresentationState &presentation,
                               const CameraLifecycle &lifecycle, const ViewMatrixArena &matrices,
                               const CameraMatrix &identity, const ProjectionParameters &projection,
                               WorldModelViewScratch &scratch, CameraMatrix &bound_matrix,
                               WorldTransformedVertexCursor &cursor, std::array<WorldPolygonProducerList, 4> &packets,
                               std::uint32_t quality_first, std::uint32_t &quality_second, std::uint32_t rate,
                               std::uint32_t &initialized, std::uint32_t &accepted, WorldModelLightingBinding lighting)
{
    initialize_world_track_shading(binding, matrices, identity, scratch, quality_first, initialized, lighting);
    const bool shading = quality_first == 1 && rate == 100;
    const bool specular = shading && quality_second == 1;
    if (shading)
        select_world_model_lighting_stride(lighting.stride, lighting.quality, binding.current_lod,
                                           binding.model.previous_detail_level());
    else
        quality_second = 0;
    binding.outer = &persistent_template; // 42AE39, visible to subsequent calls and the next frame
    auto &model = binding.model;
    const auto lod = model.source_lod_index(0), count = static_cast<std::uint32_t>(model.part_count(lod));
    for (unsigned i = 1; i < count; ++i)
    {
        auto part = model.part(lod, i);
        if (part.flags != 1)
            continue;
        const auto classification = classify_world_model_visibility(
            model, binding.current_lod, static_cast<std::int32_t>(i), *binding.outer, camera, presentation, lifecycle,
            matrices, identity, projection, bound_matrix);
        auto &flags = model.source_part_flags_02(lod, i);
        if (classification == 1)
        {
            flags |= 0x10;
            continue;
        }
        flags &= 0xffef;
        const auto &outer = *binding.outer;
        const auto &inverse_outer = resolve_model_matrix(matrices, identity, outer.tail[2]);
        const auto *inverse_part =
            count > 1 ? &resolve_model_matrix(matrices, identity, part.transform.tail[1]) : nullptr;
        world_model_local_camera(scratch.local_camera, scratch.normalized_camera,
                                 {camera.position.x, camera.position.y, camera.position.z}, outer.position,
                                 inverse_outer, part.transform.position, part.pivot, inverse_part, count);
        const auto &pivot = model.source_model_vector_10();
        const CameraModelTransform outer_view{vector(outer.position, outer.position_w),
                                              vector(outer.scale, outer.scale_w),
                                              {pivot.x, pivot.y, pivot.z, pivot.w},
                                              resolve_model_matrix(matrices, identity, outer.tail[1])};
        const CameraModelTransform part_view{vector(part.transform.position, part.transform.position_w),
                                             vector(part.transform.scale, part.transform.scale_w), vector(part.pivot),
                                             resolve_model_matrix(matrices, identity, part.transform.tail[0])};
        auto bound_camera = camera;
        bound_camera.world_to_camera_rotation =
            resolve_model_matrix(matrices, identity, camera.world_to_camera_reference);
        scratch.model_view = camera_model_view(bound_camera, outer_view, count, &part_view);
        const auto source = world_polygon_part_source(model, lod, i);
        if (cursor.count > cursor.allocation.size() || source.vertices.size() > cursor.allocation.size() - cursor.count)
            throw FormatError("source track part transformed vertex allocation");
        auto vertices = cursor.allocation.subspan(cursor.count, source.vertices.size());
        if (classification == 0)
        {
            transform_world_vertices_unclipped(source.vertices, vertices, scratch.model_view, projection);
            emit_world_polygons_unclipped(source, vertices, scratch.local_camera, packets[2], packets[3]);
        }
        else
        {
            // 42952C..42953A reads live camera198 rather than projection cache.
            auto live_projection = projection;
            live_projection.clip_near = lifecycle.clip_near;
            transform_world_vertices_clipped(source.vertices, vertices, scratch.model_view, live_projection);
            emit_world_polygons_clipped(source, vertices, scratch.local_camera, packets[0], packets[1]);
        }
        if (shading && (flags & 4))
        {
            if (specular && (flags & 3))
                shade_world_model_specular(light_pose(binding, lod, i, matrices, identity, scratch),
                                           light_part(model, lod, i), lighting.lights, lighting.selection,
                                           lighting.prepared, lighting.ambient, lighting.colors, lighting.frame_stamp,
                                           lighting.stride);
            else
                shade_world_model_diffuse(light_pose(binding, lod, i, matrices, identity, scratch),
                                          light_part(model, lod, i), lighting.lights, lighting.selection,
                                          lighting.prepared, lighting.ambient, lighting.frame_stamp, lighting.stride);
        }
        cursor.count += static_cast<std::uint32_t>(source.vertices.size());
        ++accepted;
    }
}
} // namespace sgn::retail
