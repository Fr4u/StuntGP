#include "sgn/race_session_menu.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/race_session_music.hpp"
#include "sgn/race_session_scene.hpp"
#include "sgn/session_world_materials.hpp"

namespace sgn::retail
{
RaceSession &SessionMenuInvocation::checked_session(RaceSession &session, const RaceOverlayTextures &textures)
{
    session.shared_platform()->require_owner_thread();
    textures.require_for(session.shared_platform());
    return session;
}

TextSubmission SessionMenuInvocation::Drawing::borrow_text()
{
    session_.shared_platform()->require_owner_thread();
    const auto &materials = materials_.validate_for(session_);
    (void)materials_.validate_fonts_for(session_);
    auto &s = session_.state();
    // The same owners used by prepare_session_scene_frame. Borrow the material
    // projection anew after preceding synchronous children, never cache a span.
    return {s.ui.fonts,
            s.ui.selected_material,
            submission_.specular_enabled,
            materials.names.materials.active_materials(),
            s.view_quality.second,
            s.view_quality.legacy_driver_restriction,
            s.viewport};
}

std::span<PolygonSubmissionVertex> SessionMenuInvocation::Drawing::borrow_alpha_vertices()
{
    session_.shared_platform()->require_owner_thread();
    // Capacity preparation belongs to the explicit outer caller. A preceding
    // child may have replaced the allocation; acquire the vector's current data.
    return submission_.alpha_vertices;
}

PauseMenuRestartInvocation SessionMenuInvocation::Restart::borrow_restart()
{
    // Existing446AF0 calls this only after the real stop_audio_music45F9D0.
    // The provider must keep its invocation-owned parameters/views alive through
    // initialize_race40D500. Never acquire or cache this invocation at bind time.
    auto invocation = provider_.borrow_restart();
    if (&invocation.current_vehicle_output != &state_.current_slot)
        throw FormatError("pause restart must publish to the Session current vehicle owner");
    // The existing restart port separately validates clock/vehicles/HUD/audio/
    // projection aliases, executes40D500 and publishes before countdown/resume.
    return invocation;
}

SessionMenuInvocation::SessionMenuInvocation(RaceSession &session, const SessionWorldMaterials &materials,
                                             PolygonSubmissionState &submission, RaceOverlayTextures &textures,
                                             PauseMenuRestartSource &restart, PolygonSubmissionSink &sink)
    : session_(checked_session(session, textures)), pause_(session_.state(), session_.audio_device()),
      transition_(scene_transition_world(session_.state())), results_(scene_race_result_world(session_.state())),
      labels_(session_.state().menu.meter_labels), drawing_(session_, materials, submission),
      restart_(session_.state(), restart), world_([&] {
          auto &s = session_.state();
          const auto &lifecycle = pause_.lifecycle();
          return PauseMenuWorld{{s.menu.release_count, s.menu.selected_row, s.menu.highlight, s.menu.overlay_ready,
                                 s.menu.requested_language, s.menu.rows, s.frontend_launch.input_bindings,
                                 s.menu_input.edges},
                                lifecycle,
                                s.clock,
                                s.render_b,
                                s.level.game_type,
                                s.impact_particles.rate_selector,
                                s.selected_camera_preset,
                                s.options,
                                lifecycle.vehicle_audio.driver,
                                audio_stream_world(session_),
                                restart_,
                                transition_,
                                results_,
                                s.vehicles,
                                {s.frame_projection, s.inverse_depth_scale, s.depth_offset, s.shared_model_identity},
                                s.game_text,
                                textures,
                                s.ui.descriptor,
                                s.ui.line_clip,
                                labels_,
                                drawing_,
                                sink};
      }())
{
    // Host wiring validation only, at adapter construction. This does not add
    // a validation preamble to the recovered function's source early returns.
    validate_pause_menu_bindings(world_);
}

PauseMenuWorld &SessionMenuInvocation::view() &
{
    session_.shared_platform()->require_owner_thread();
    return world_;
}

const PauseMenuWorld &SessionMenuInvocation::view() const &
{
    session_.shared_platform()->require_owner_thread();
    return world_;
}
} // namespace sgn::retail
