#include "sgn/retail_world_lit_model_producer.hpp"

namespace sgn::retail
{
namespace
{
CameraVector camera_vector(Vec3 vector, std::uint32_t w) noexcept
{
    return {vector.x, vector.y, vector.z, std::bit_cast<float>(w)};
}
} // namespace
void produce_world_model_lit(ModelResource &model, unsigned source_lod, const ModelTransform &outer,
                             const CameraState &camera, const ViewMatrixArena &arena, const CameraMatrix &identity,
                             const ProjectionParameters &projection, bool clipped, WorldModelViewScratch &scratch,
                             WorldTransformedVertexCursor &cursor, WorldPolygonProducerList &opaque,
                             WorldPolygonProducerList &translucent, WorldModelLightingRoute route,
                             WorldModelLightingBinding lighting)
{
    const auto lod = model.source_lod_index(source_lod);
    const auto count = static_cast<std::uint32_t>(model.part_count(lod));
    for (unsigned i = 0; i < count; ++i)
    {
        auto part = model.part(lod, i);
        if (part.flags != 1)
            continue;
        const auto &inverse_outer = resolve_model_matrix(arena, identity, outer.tail[2]);
        const auto *inverse_part = count > 1 ? &resolve_model_matrix(arena, identity, part.transform.tail[1]) : nullptr;
        world_model_local_camera(scratch.local_camera, scratch.normalized_camera,
                                 {camera.position.x, camera.position.y, camera.position.z}, outer.position,
                                 inverse_outer, part.transform.position, part.pivot, inverse_part, count);
        const auto &descriptor_pivot = model.source_model_vector_10();
        const CameraModelTransform outer_view{
            camera_vector(outer.position, outer.position_w),
            camera_vector(outer.scale, outer.scale_w),
            {descriptor_pivot.x, descriptor_pivot.y, descriptor_pivot.z, descriptor_pivot.w},
            resolve_model_matrix(arena, identity, outer.tail[1])};
        CameraModelTransform part_view;
        if (count > 1)
        {
            part_view = {camera_vector(part.transform.position, part.transform.position_w),
                         camera_vector(part.transform.scale, part.transform.scale_w),
                         {part.pivot.x, part.pivot.y, part.pivot.z, 0},
                         resolve_model_matrix(arena, identity, part.transform.tail[0])};
        }
        auto bound_camera = camera;
        bound_camera.world_to_camera_rotation = resolve_model_matrix(arena, identity, camera.world_to_camera_reference);
        scratch.model_view = camera_model_view(bound_camera, outer_view, count, count > 1 ? &part_view : nullptr);
        const auto source = world_polygon_part_source(model, lod, i);
        if (cursor.count > cursor.allocation.size() || source.vertices.size() > cursor.allocation.size() - cursor.count)
            throw FormatError("source lit model transformed vertex allocation");
        auto vertices = cursor.allocation.subspan(cursor.count, source.vertices.size());
        if (clipped)
        {
            transform_world_vertices_clipped(source.vertices, vertices, scratch.model_view, projection);
            emit_world_polygons_clipped(source, vertices, scratch.local_camera, opaque, translucent);
        }
        else
        {
            transform_world_vertices_unclipped(source.vertices, vertices, scratch.model_view, projection);
            emit_world_polygons_unclipped(source, vertices, scratch.local_camera, opaque, translucent);
        }
        const auto flags = model.source_part_flags_02(lod, i);
        if (flags & 4)
        {
            select_world_part_lighting_stride(lighting.stride, lighting.quality, source_lod,
                                              model.previous_detail_level(), model.source_part_word_04(lod, i));
            const WorldModelLightingPose pose{
                scratch.normalized_camera, outer.position, part.transform.position, inverse_outer, inverse_part, count};
            const WorldModelLightingPart light_part{model.source_model_ambience(), flags,
                                                    model.source_coordinates(lod, i), source.vertices, source.normals};
            if (route == WorldModelLightingRoute::material_specular && (flags & 3))
                shade_world_model_specular(pose, light_part, lighting.lights, lighting.selection, lighting.prepared,
                                           lighting.ambient, lighting.colors, lighting.frame_stamp, lighting.stride);
            else
                shade_world_model_diffuse(pose, light_part, lighting.lights, lighting.selection, lighting.prepared,
                                          lighting.ambient, lighting.frame_stamp, lighting.stride);
        }
        cursor.count += static_cast<std::uint32_t>(source.vertices.size());
    }
}
} // namespace sgn::retail
