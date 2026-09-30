#include "sgn/retail_world_sprite_producer.hpp"
#include "sgn/source_world_frame_consumers.hpp"

namespace sgn::retail
{
void produce_world_sprite_unlit(const WorldModelDrawState &state, const SourceSpritePacketStorage &sprites,
                                const CameraState &camera, const ViewMatrixArena &arena, const CameraMatrix &identity,
                                const ProjectionParameters &projection, bool clipped, WorldModelViewScratch &scratch,
                                WorldTransformedVertexCursor &cursor, WorldPolygonProducerList &opaque,
                                WorldPolygonProducerList &translucent, const SceneResources *source_scene)
{
    if (sprites.source_part.source_bound())
    {
        if (!source_scene)
            throw SourceAllocationError(0x42a280, "source sprite consumer requires its actual scene registry");
        produce_source_world_sprite_unlit_42a260(*source_scene, state, sprites, camera, arena, identity, projection,
                                                 clipped, scratch, cursor, opaque, translucent);
        return;
    }
    if (state.sprite_part_flags != 1)
        return; // 42A280/42A284
    if (!state.sprite_outer_transform)
        throw FormatError("generated sprite model requires its queued outer pose");
    if (sprites.polygons > sprites.headers.size() || sprites.corners > sprites.positions.size())
        throw FormatError("generated sprite model part count outside allocation");
    if (cursor.count > cursor.allocation.size() || sprites.corners > cursor.allocation.size() - cursor.count)
        throw FormatError("generated sprite transformed vertex allocation");
    const auto &outer = *state.sprite_outer_transform;
    const auto &inverse_outer = resolve_model_matrix(arena, identity, outer.tail[2]);
    const auto &forward_outer = resolve_model_matrix(arena, identity, outer.tail[1]);
    const auto &camera_rotation = resolve_model_matrix(arena, identity, camera.world_to_camera_reference);
    world_model_local_camera(scratch.local_camera, scratch.normalized_camera,
                             {camera.position.x, camera.position.y, camera.position.z}, outer.position, inverse_outer,
                             state.sprite_part_transform.position, {}, nullptr, 1);
    const CameraModelTransform outer_view{
        {outer.position.x, outer.position.y, outer.position.z, std::bit_cast<float>(outer.position_w)},
        {outer.scale.x, outer.scale.y, outer.scale.z, std::bit_cast<float>(outer.scale_w)},
        {},
        forward_outer};
    auto bound_camera = camera;
    bound_camera.world_to_camera_rotation = camera_rotation;
    scratch.model_view = camera_model_view(bound_camera, outer_view, 1, nullptr);
    // Two distinct native vector types represent the same source16-byte record.
    // This read-only, per-call conversion avoids aliasing object lifetimes.
    std::array<VehicleVector, 2048> converted;
    for (std::size_t i = 0; i < sprites.corners; ++i)
        converted[i] = std::bit_cast<VehicleVector>(sprites.positions[i]);
    const WorldPolygonPartSource source{sprites.headers.view().first(sprites.polygons),
                                        WorldPolygonAttributeView(sprites.attributes.view()),
                                        std::span(converted).first(sprites.corners),
                                        state.sprite_source.normals,
                                        sprites.indices,
                                        state.sprite_model_flags,
                                        state.sprite_part_flags_02};
    auto output = cursor.allocation.subspan(cursor.count, sprites.corners);
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
    cursor.count += sprites.corners; // 42A2E4..42A2F5
}
} // namespace sgn::retail
