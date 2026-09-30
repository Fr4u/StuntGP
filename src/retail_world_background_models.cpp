#include "sgn/retail_world_background_models.hpp"
#include "sgn/retail_world_model_detail.hpp"
#include "sgn/source_world_frame_consumers.hpp"

namespace sgn::retail
{
void produce_world_background_models(std::span<const ModelDrawCommand> commands, ModelResources &resources,
                                     WorldModelDrawState &draws, const SourceSpritePacketStorage &sprites,
                                     VehicleStripGeometry &strip, const CameraState &camera,
                                     const CameraPresentationState &presentation, const CameraLifecycle &lifecycle,
                                     const ViewMatrixArena &matrices, const CameraMatrix &identity,
                                     const ProjectionParameters &projection, WorldPolygonFrame &frame,
                                     const std::uint32_t &source_rate, const SceneResources *source_scene)
{
    if (sprites.source_part.source_bound())
    {
        if (!source_scene || &source_scene->models() != &resources)
            throw SourceAllocationError(0x42af30, "source background consumer requires its actual scene registry");
        produce_source_world_background_models_42af30(commands, *source_scene, draws, sprites, camera, presentation,
                                                      lifecycle, matrices, identity, projection, frame, source_rate);
        return;
    }
    auto live_projection = projection;
    live_projection.clip_near = lifecycle.clip_near;
    for (const auto &command : commands)
    {
        if (command.model_identity == 0)
            return;
        if (command.model_identity == sprite_model_identity)
        {
            draws.sprite_outer_transform = &command.pose;
            // 434090 fixes both bound counts and maximumLOD to zero. Therefore
            // 428040 returns2 without touching the camera scratch;42B620 skips.
            produce_world_sprite_unlit(draws, sprites, camera, matrices, identity, live_projection, true,
                                       frame.model_view, frame.transformed, frame.packets[0], frame.packets[1]);
        }
        else if (command.model_identity == strip_model_identity)
        {
            // 41C530 fixes zero bounds and maximumLOD.42AF43 still publishes
            // the actual command pose before an empty or hidden part returns.
            strip.outer_transform = &command.pose;
            produce_world_strip_unlit(strip, camera, matrices, identity, live_projection, true, frame.model_view,
                                      frame.transformed, frame.packets[0], frame.packets[1]);
        }
        else
        {
            const auto handle = resources.resolve(command.model_identity);
            if (!handle)
                throw FormatError("source background command has no imported model");
            auto &model = resources.at(*handle);
            model.source_outer_transform() = &command.pose;
            const auto visibility = classify_world_model_visibility(
                model, model.current_detail_level(), -1, command.pose, camera, presentation, lifecycle, matrices,
                identity, live_projection, frame.bound_matrix);
            if (visibility == 1)
                continue;
            if (model.source_maximum_detail_level() != 0)
                select_world_model_detail(command.pose.position, camera, presentation.forward_axis, source_rate,
                                          model.current_detail_level(), model.previous_detail_level());
            const bool clipped = visibility != 0;
            produce_world_model_unlit(model, model.current_detail_level(), command.pose, camera, matrices, identity,
                                      live_projection, clipped, frame.model_view, frame.transformed,
                                      frame.packets[clipped ? 0 : 2], frame.packets[clipped ? 1 : 3]);
        }
        ++frame.accepted_models;
    }
    throw FormatError("unterminated source background model list");
}
} // namespace sgn::retail
