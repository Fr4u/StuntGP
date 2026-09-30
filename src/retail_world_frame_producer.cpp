#include "sgn/retail_world_frame_producer.hpp"
#include "sgn/retail_world_model_detail.hpp"

namespace sgn::retail
{
namespace
{
CameraVector vector(Vec3 value, std::uint32_t w = 0) noexcept
{
    return {value.x, value.y, value.z, std::bit_cast<float>(w)};
}
CameraMatrix initial_track_view(ModelResource &model, const CameraState &camera, const ViewMatrixArena &matrices,
                                const CameraMatrix &identity)
{
    const auto *outer = model.source_outer_transform();
    if (!outer)
        throw FormatError("source42B040 track previous descriptor0C is absent");
    const auto lod = model.source_lod_index(0);
    const auto count = static_cast<std::uint32_t>(model.part_count(lod));
    if (count == 0)
        throw FormatError("source42B040 track descriptor44 base part is absent");
    const auto part = model.part(lod, 0);
    const auto &pivot = model.source_model_vector_10();
    const CameraModelTransform outer_view{vector(outer->position, outer->position_w),
                                          vector(outer->scale, outer->scale_w),
                                          {pivot.x, pivot.y, pivot.z, pivot.w},
                                          resolve_model_matrix(matrices, identity, outer->tail[1])};
    const CameraModelTransform part_view{vector(part.transform.position, part.transform.position_w),
                                         vector(part.transform.scale, part.transform.scale_w), vector(part.pivot),
                                         resolve_model_matrix(matrices, identity, part.transform.tail[0])};
    auto bound_camera = camera;
    bound_camera.world_to_camera_rotation = resolve_model_matrix(matrices, identity, camera.world_to_camera_reference);
    return camera_model_view(bound_camera, outer_view, count, &part_view);
}
} // namespace
void produce_world_frame(WorldFrameProduction world, TrackGeometryAnimationAudit audit)
{
    auto &model = world.models.at(world.track);
    auto &frame = world.frame;
    auto &background = world.draws.background.records;
    if (background[0].model_identity != world.models.identity(world.track) ||
        background[1].model_identity != world.models.identity(world.sky))
        throw FormatError("source42B040 static-world queue does not name the selected track/sky");
    frame.accepted_models = 0; // 42B04B precedes the initial44FBD0
    world.frame_world_view = initial_track_view(model, world.camera, world.matrices, world.identity);
    reset_world_polygon_groups(frame);
    begin_world_polygon_group(frame, 1);
    model.source_outer_transform() = &background[0].pose;
    auto live_projection = world.projection;
    live_projection.clip_near = world.lifecycle.clip_near;
    produce_world_track_base(
        model, world.camera, world.presentation, world.lifecycle, world.matrices, world.identity, live_projection,
        frame, world.animations, world.normals,
        {world.random, world.particles, world.gravity, world.source_rate, world.draws.template_pose}, audit);
    finish_world_polygon_group(frame, 1);
    frame.remaining_first = frame.transformed.count;
    produce_world_sky(background[1], world.models, world.camera, world.lifecycle, world.matrices, world.identity,
                      live_projection, frame);
    world.lifecycle.clip_far = std::bit_cast<float>(0x48f42400u); // 42B0A2,500000
    begin_world_polygon_group(frame, 2);
    produce_world_background_models(std::span(background).subspan(2), world.models, world.draws, world.sprites,
                                    world.strip, world.camera, world.presentation, world.lifecycle, world.matrices,
                                    world.identity, live_projection, frame, world.source_rate, world.registry);
    if (world.source_rate >= 25)
        produce_world_track_parts(
            {model, model.source_outer_transform(), model.current_detail_level()}, world.draws.template_pose,
            world.camera, world.presentation, world.lifecycle, world.matrices, world.identity, live_projection,
            frame.model_view, frame.bound_matrix, frame.transformed, frame.packets, world.lighting_enabled,
            world.quality, world.source_rate, frame.low_quality_initialized, frame.accepted_models, world.lighting);
    finish_world_polygon_group(frame, 2);
    begin_world_polygon_group(frame, 3);
    produce_world_regular_models(world.draws.regular.records, world.models, world.camera, world.presentation,
                                 world.lifecycle, world.matrices, world.identity, live_projection, frame,
                                 world.source_rate, world.lighting_enabled, world.lighting);
    finish_world_polygon_group(frame, 3);
    seed_world_model_draws(world.draws, world.models, world.track, world.sky, world.sprites, world.ground_level,
                           world.scene_mode, world.matrices, world.registry);
    publish_world_polygon_totals(frame);
}
} // namespace sgn::retail
