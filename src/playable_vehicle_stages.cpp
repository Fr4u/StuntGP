#include "sgn/playable_vehicle_stages.hpp"
#include "sgn/retail_world_view_prefix_session.hpp"
#include "sgn/vehicle_draw_pose.hpp"
#include "sgn/vehicle_model_detail.hpp"
#include "sgn/vehicle_model_prepare.hpp"
#include "sgn/vehicle_shadow_draw.hpp"
#include "sgn/world_model_draw.hpp"
#include "sgn/source_pmd_geometry.hpp"

namespace sgn::retail
{
PlayableVehicleStages::PlayableVehicleStages(RaceSession &session) : session_(session)
{
    const auto &state = session.state().model_draws;
    if (!state.regular.cursor || !state.background.cursor)
        throw FormatError("Session model queues were not initialized at scene load");
    regular_ = state.regular;
    background_ = state.background;
}
void PlayableVehicleStages::stage_entities_442500(const CameraFrameSnapshot &snapshot)
{
    auto &s = session_.state();
    auto &v = s.vehicles;
    if (snapshot.controller() != s.controls.current || snapshot.controller() >= s.camera_presentation.size())
        throw FormatError("playable model stage requires current source camera");
    // 42B740 already seeded the authoritative queues at scene load or after
    // the previous source consumers. Never reset them before entity appends.
    auto &regular = s.model_draws.regular;
    auto &background = s.model_draws.background;
    cars_ = {};
    shadows_ = {};
    const auto forward = s.camera_presentation[snapshot.controller()].forward_axis;
    for (unsigned slot = 0; slot < VehicleStorage::physical_slots; ++slot)
    {
        s.current_slot = slot; // 418189/4182E4..4182FA, actual physical iteration
        if (v.race[slot].active != 1)
            continue;
        if (!v.models[slot].model)
            throw FormatError("active playable car has no source model");
        prepare_vehicle_model_detail(v.physics[slot], v.presentation[slot], v.particles[slot],
                                     vehicle_presentation_effects(v.model_effects[slot], v.anchors[slot]),
                                     session_.models(), *v.models[slot].model, snapshot.camera(), forward,
                                     s.impact_particles.rate_selector); // 4181B3
        prepare_vehicle_model(v.physics[slot], v.trail_inputs[slot], v.wheels[slot], v.presentation[slot],
                              v.anchors[slot], v.segments[slot], session_.models(), *v.models[slot].model,
                              s.model_basis, v.rail[slot].effect_position); // 4181BE
        if (v.presentation[slot].field_3928 != 0)
        {
            if (!v.models[slot].shadow)
                throw FormatError("visible source shadow has no resource");
            // Every shipped car shadow has one distinct LOD. Its source slots
            // alias that owner; no independent distance selector is invented.
            if (session_.models().distinct_lod_count(*v.models[slot].shadow) != 1)
                throw FormatError("playable shadow requires authored single LOD");
            const auto index = *background.cursor;
            if (append_vehicle_shadow_draw(v.presentation[slot], v.shadows[slot], session_.models(),
                                           *v.models[slot].model, *v.models[slot].shadow, background, s.view_matrices,
                                           s.model_draws.template_pose))
                shadows_[slot] = index; // 4181C3
        }
        // 4181C8..4181DC trail calls remain outside this connected subset.
        const auto index = *regular.cursor;
        append_vehicle_model_draw(v.physics[slot], v.contact[slot].rig.origin_y_offset, v.model_effects[slot],
                                  v.race[slot], v.models[slot], session_.models(), *v.models[slot].model, regular,
                                  s.view_matrices, s.model_draws.template_pose); // 4181E1..4182E3
        cars_[slot] = index;
    }
    s.current_slot = 0; // 418305
}
void PlayableVehicleStages::prepare_world_42e870()
{
    prepare_world_view_prefix(world_view_prefix(session_));
}
void PlayableVehicleStages::consume_model_views(const CameraFrameSnapshot &snapshot)
{
    auto &s = session_.state();
    regular_ = s.model_draws.regular;
    background_ = s.model_draws.background;
    const auto prepare = [&](const ModelDrawQueue &queue, std::vector<PreparedModelDraw> &output) {
        if (!queue.cursor)
            throw FormatError("source model consumer has null queue cursor");
        output.clear();
        output.reserve(*queue.cursor);
        for (unsigned i = 0; i < *queue.cursor; ++i)
        {
            const auto &command = queue.records[i];
            if (command.model_identity == sprite_model_identity)
            {
                const auto &p = command.pose;
                const CameraModelTransform outer{
                    {p.position.x, p.position.y, p.position.z, std::bit_cast<float>(p.position_w)},
                    {p.scale.x, p.scale.y, p.scale.z, std::bit_cast<float>(p.scale_w)},
                    {},
                    resolve_model_matrix(s.view_matrices, s.shared_model_identity, p.tail[1])};
                // Its single-part44FBD0 path does not read part+40. Sprite
                // polygon rendering remains a separate, explicitly open stage.
                LiveModelViews views;
                views.parts.push_back({{}, true});
                views.model_to_view.push_back(camera_model_view(snapshot.camera(), outer));
                output.push_back({command, 0, outer, std::move(views)});
                continue;
            }
            const auto handle = session_.models().resolve(command.model_identity);
            if (!handle)
                throw FormatError("playable view command has no model owner");
            unsigned source_lod = 0;
            for (unsigned slot = 0; slot < 6; ++slot)
                if (command.model_identity == session_.models().identity(s.vehicles.models[slot].model))
                    source_lod = session_.models().current_detail_level(*handle);
            const auto &p = command.pose;
            const auto pivot = session_.models().source_pivot(*handle);
            const CameraModelTransform outer{
                {p.position.x, p.position.y, p.position.z, std::bit_cast<float>(p.position_w)},
                {p.scale.x, p.scale.y, p.scale.z, std::bit_cast<float>(p.scale_w)},
                {pivot.x, pivot.y, pivot.z, pivot.w},
                resolve_model_matrix(s.view_matrices, s.shared_model_identity, p.tail[1])};
            output.push_back({command, source_lod, outer,
                              model_live_views(session_.models(), *handle, source_lod, snapshot.camera(), outer,
                                               s.view_matrices, s.shared_model_identity)});
        }
    };
    // Own derived values before42B0F6 can overwrite the referenced arena slots.
    prepare(background_, background_views_);
    prepare(regular_, regular_views_);
    seed_world_model_draws(s.model_draws, session_.models(), session_.scene_resources().model_handle(3),
                           session_.scene_resources().model_handle(2), s.sprite_packets, s.level.ground_level,
                           s.scene_mode, s.view_matrices, &session_.scene_resources());
}
const ModelDrawCommand *PlayableVehicleStages::car(unsigned slot) const
{
    const auto index = cars_.at(slot);
    return index ? &regular_.records[*index] : nullptr;
}
const ModelDrawCommand *PlayableVehicleStages::shadow(unsigned slot) const
{
    const auto index = shadows_.at(slot);
    return index ? &background_.records[*index] : nullptr;
}
const PreparedModelDraw *PlayableVehicleStages::car_view(unsigned slot) const
{
    const auto i = cars_.at(slot);
    return i ? &regular_views_.at(*i) : nullptr;
}
const PreparedModelDraw *PlayableVehicleStages::shadow_view(unsigned slot) const
{
    const auto i = shadows_.at(slot);
    return i ? &background_views_.at(*i) : nullptr;
}
const PreparedModelDraw *PlayableVehicleStages::static_view(std::uint32_t identity) const
{
    for (const auto &view : background_views_)
        if (view.command.model_identity == identity)
            return &view;
    return nullptr;
}
} // namespace sgn::retail
