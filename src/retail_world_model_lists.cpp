#include "sgn/retail_world_model_lists.hpp"
#include "sgn/retail_world_model_detail.hpp"

namespace sgn::retail
{
namespace
{
ModelResource &imported_model(ModelResources &resources, std::uint32_t identity)
{
    const auto handle = resources.resolve(identity);
    if (!handle)
        throw FormatError("source draw command has no imported model");
    return resources.at(*handle);
}
} // namespace
void produce_world_regular_models(std::span<const ModelDrawCommand> commands, ModelResources &resources,
                                  const CameraState &camera, const CameraPresentationState &presentation,
                                  const CameraLifecycle &lifecycle, const ViewMatrixArena &matrices,
                                  const CameraMatrix &identity, const ProjectionParameters &projection,
                                  WorldPolygonFrame &frame, std::uint32_t source_rate, std::uint32_t lighting_enabled,
                                  WorldModelLightingBinding lighting)
{
    // 42952C..42953A reads the current camera198 on every clipped producer.
    auto live_projection = projection;
    live_projection.clip_near = lifecycle.clip_near;
    for (const auto &command : commands)
    {
        if (command.model_identity == 0)
            return;
        auto &model = imported_model(resources, command.model_identity);
        model.source_outer_transform() = &command.pose;
        const auto visibility =
            classify_world_model_visibility(model, model.current_detail_level(), -1, command.pose, camera, presentation,
                                            lifecycle, matrices, identity, live_projection, frame.bound_matrix);
        if (visibility == 1)
            continue;
        if (model.source_maximum_detail_level() != 0)
            select_world_model_detail(command.pose.position, camera, presentation.forward_axis, source_rate,
                                      model.current_detail_level(), model.previous_detail_level());
        const auto slot = model.current_detail_level();
        const bool clipped = visibility != 0;
        auto &opaque = frame.packets[clipped ? 0 : 2];
        auto &alpha = frame.packets[clipped ? 1 : 3];
        if (lighting_enabled == 0 || slot >= 2)
            produce_world_model_unlit(model, slot, command.pose, camera, matrices, identity, live_projection, clipped,
                                      frame.model_view, frame.transformed, opaque, alpha);
        else
            produce_world_model_lit(
                model, slot, command.pose, camera, matrices, identity, live_projection, clipped, frame.model_view,
                frame.transformed, opaque, alpha,
                slot == 0 ? WorldModelLightingRoute::material_specular : WorldModelLightingRoute::diffuse, lighting);
        ++frame.accepted_models;
    }
    throw FormatError("unterminated source regular model list");
}
void produce_world_sky(const ModelDrawCommand &command, ModelResources &resources, const CameraState &camera,
                       CameraLifecycle &lifecycle, const ViewMatrixArena &matrices, const CameraMatrix &identity,
                       const ProjectionParameters &projection, WorldPolygonFrame &frame)
{
    auto &model = imported_model(resources, command.model_identity);
    // 42AD3B stores literal4A989680, exactly5,000,000 (not10,000,000).
    lifecycle.clip_far = std::bit_cast<float>(0x4a989680u);
    model.source_outer_transform() = &command.pose;
    auto live_projection = projection;
    live_projection.clip_near = lifecycle.clip_near;
    begin_world_polygon_group(frame, 0);
    produce_world_model_unlit(model, model.current_detail_level(), command.pose, camera, matrices, identity,
                              live_projection, true, frame.model_view, frame.transformed, frame.packets[0],
                              frame.packets[1]);
    finish_world_polygon_group(frame, 0);
}
} // namespace sgn::retail
