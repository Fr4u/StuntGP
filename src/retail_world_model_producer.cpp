#include "sgn/retail_world_model_producer.hpp"

namespace sgn::retail
{
namespace
{
CameraVector camera_vector(Vec3 vector, std::uint32_t w) noexcept
{
    return {vector.x, vector.y, vector.z, std::bit_cast<float>(w)};
}
} // namespace
void produce_world_model_unlit(ModelResource &model, unsigned source_lod, const ModelTransform &outer,
                               const CameraState &camera, const ViewMatrixArena &arena, const CameraMatrix &identity,
                               const ProjectionParameters &projection, bool clipped, WorldModelViewScratch &scratch,
                               WorldTransformedVertexCursor &cursor, WorldPolygonProducerList &opaque,
                               WorldPolygonProducerList &translucent)
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
        // The source camera matrix is an arena pointer. The copied camera
        // below is only a value binding for44FBD0, not another camera owner.
        auto bound_camera = camera;
        bound_camera.world_to_camera_rotation = resolve_model_matrix(arena, identity, camera.world_to_camera_reference);
        scratch.model_view = camera_model_view(bound_camera, outer_view, count, count > 1 ? &part_view : nullptr);
        const auto source = world_polygon_part_source(model, lod, i);
        if (cursor.count > cursor.allocation.size() || source.vertices.size() > cursor.allocation.size() - cursor.count)
            throw FormatError("source model transformed vertex allocation");
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
        cursor.count += static_cast<std::uint32_t>(source.vertices.size());
    }
}
} // namespace sgn::retail
