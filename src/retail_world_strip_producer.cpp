#include "sgn/retail_world_strip_producer.hpp"

namespace sgn::retail
{
void produce_world_strip_unlit(const VehicleStripGeometry &strip, const CameraState &camera,
                               const ViewMatrixArena &arena, const CameraMatrix &identity,
                               const ProjectionParameters &projection, bool clipped, WorldModelViewScratch &scratch,
                               WorldTransformedVertexCursor &cursor, WorldPolygonProducerList &opaque,
                               WorldPolygonProducerList &translucent)
{
    if (strip.part_flags != 1)
        return; // 42A280..42A284
    if (!strip.outer_transform)
        throw FormatError("generated strip requires its queued outer pose");
    if (strip.polygon_count > strip.polygons.size() || strip.vertex_count > strip.vertices.size())
        throw FormatError("generated strip part count outside allocation");
    if (cursor.count > cursor.allocation.size() || strip.vertex_count > cursor.allocation.size() - cursor.count)
        throw FormatError("generated strip transformed vertex allocation");
    const auto &outer = *strip.outer_transform;
    const auto &inverse_outer = resolve_model_matrix(arena, identity, outer.tail[2]);
    const auto &forward_outer = resolve_model_matrix(arena, identity, outer.tail[1]);
    const auto &camera_rotation = resolve_model_matrix(arena, identity, camera.world_to_camera_reference);
    const auto &part = strip.part_transform.position;
    world_model_local_camera(scratch.local_camera, scratch.normalized_camera,
                             {camera.position.x, camera.position.y, camera.position.z}, outer.position, inverse_outer,
                             {part.x, part.y, part.z}, {}, nullptr, 1);
    const CameraModelTransform outer_view{
        {outer.position.x, outer.position.y, outer.position.z, std::bit_cast<float>(outer.position_w)},
        {outer.scale.x, outer.scale.y, outer.scale.z, std::bit_cast<float>(outer.scale_w)},
        {},
        forward_outer};
    auto bound_camera = camera;
    bound_camera.world_to_camera_rotation = camera_rotation;
    scratch.model_view = camera_model_view(bound_camera, outer_view, 1, nullptr);
    // HeaderRef and AttributeView borrow the actual typed strip objects. Float
    // UV object bits are read at submission; no synthetic PMD or second owner.
    const WorldPolygonPartSource source{
        WorldPolygonHeaderView(std::span(strip.polygons)).subspan(0, strip.polygon_count),
        WorldPolygonAttributeView(std::span(strip.corners)),
        std::span(strip.vertices).first(strip.vertex_count),
        strip.normals,
        strip.corner_indices,
        strip.model_flags_02,
        strip.part_flags_02};
    auto output = cursor.allocation.subspan(cursor.count, strip.vertex_count);
    if (clipped)
    {
        transform_world_vertices_clipped(source.vertices, output, scratch.model_view, projection);
        emit_world_polygons_clipped(source, output, scratch.local_camera, opaque, translucent);
    }
    else
    {
        transform_world_vertices_unclipped(source.vertices, output, scratch.model_view, projection);
        emit_world_polygons_unclipped(source, output, scratch.local_camera, opaque, translucent);
    }
    cursor.count += strip.vertex_count; // 42A2E4..42A2F5 reads part28, not part26
}
} // namespace sgn::retail
