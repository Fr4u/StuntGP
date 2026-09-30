#include "sgn/session_world_frame.hpp"
#include "sgn/retail_camera_frame_session.hpp"
#include "sgn/retail_world_view_prefix_session.hpp"

namespace sgn::retail
{
namespace
{
class Stages final : public CameraFrameStages
{
    RaceSession &session_;
    const SessionWorldFrameResources &resources_;

  public:
    Stages(RaceSession &session, const SessionWorldFrameResources &resources) : session_(session), resources_(resources)
    {
    }
    void stage_entities_442500(const CameraFrameSnapshot &snapshot) override
    {
        auto &state = session_.state();
        const auto current = state.controls.current;
        if (current >= state.controls.controllers.size() || snapshot.controller() != current)
            throw FormatError("Session entity stage camera does not match its live controller");
        const auto &control = state.controls.controllers[current];
        const LightCameraView camera{control.camera,
                                     state.camera_lifecycle[current],
                                     state.camera_presentation[current],
                                     state.overlays[current],
                                     &state.view_matrices,
                                     &state.shared_model_identity};
        auto screen_access = lock_screen_polygon_owner(session_.shared_platform());
        const VehiclePresentationFrame vehicles{state.vehicles,
                                                session_.models(),
                                                state.selected_lod,
                                                state.current_slot,
                                                state.impact_particles.rate_selector,
                                                control.vehicle,
                                                state.split_setting,
                                                camera,
                                                state.strip,
                                                state.model_draws,
                                                state.view_matrices,
                                                state.model_basis,
                                                state.trail_profiles,
                                                state.trail_staging,
                                                state.sprite_packets,
                                                {live_screen_polygon_reader(screen_access), screen_access.count_word()},
                                                state.viewport};
        (void)prepare_scene_entity_presentation({vehicles, state.effect_particles, state.impact_particles,
                                                 state.impact_draw_list, resources_.effect_animations,
                                                 resources_.adjacent_angles, resources_.effect_colors});
    }
    void prepare_world_42e870() override
    {
        prepare_world_view_prefix(world_view_prefix(session_));
    }
    void consume_model_views(const CameraFrameSnapshot &snapshot) override
    {
        if (snapshot.controller() != session_.state().controls.current)
            throw FormatError("Session geometry stage camera does not match its live controller");
        finish_session_world_geometry(session_, resources_.geometry_audit);
    }
};
} // namespace
CameraFrameSnapshot prepare_session_world_frame(RaceSession &session, const SessionWorldMaterials &materials,
                                                SessionPolygonCapabilities capabilities,
                                                PolygonSubmissionState &scratch, PolygonSubmissionSink &sink,
                                                SessionWorldFrameResources resources)
{
    session.shared_platform()->require_owner_thread();
    auto &frame = session.state().polygon_frame;
    const auto ticket = capture_world_polygon_level(frame);
    validate_world_polygon_level(frame, ticket);
    (void)materials.validate_for(session);
    // These are invocation borrows; no projection/camera/queue snapshot is
    // substituted for the live source owner consumed by the next stage.
    auto world = camera_frame_world(session);
    Stages stages(session, resources);
    try
    {
        auto snapshot = camera_frame_normal_views(world, {}, stages, resources.gamma);
        submit_session_world_polygons(session, materials, capabilities, scratch, sink, resources.submission_budget,
                                      resources.geometry_audit.world_join);
        return snapshot;
    }
    catch (...)
    {
        detach_world_polygon_level(frame);
        throw;
    }
}
} // namespace sgn::retail
