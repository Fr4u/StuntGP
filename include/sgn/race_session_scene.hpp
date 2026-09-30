#pragma once
#include "sgn/race_session.hpp"
#include "sgn/retail_frontend_state.hpp"

namespace sgn::retail
{
inline SceneRaceResultWorld scene_race_result_world(RaceSessionState &s)
{
    return {s.options,       s.selection, s.trial_selection, s.ghosts,          s.replays,
            s.vehicles.race, s.records,   s.finish,          s.level.game_type, s.selected_camera_preset};
}
inline SceneTransitionWorld scene_transition_world(RaceSessionState &s)
{
    return {s.scene_mode,
            s.options.selection_phase,
            s.normal_finish.menu_requested,
            s.normal_finish.field_612cf8,
            s.normal_finish.field_612bf0,
            s.frame_pause,
            s.controls,
            s.overlays,
            s.audio_level.output_b_copy,
            s.audio_stream.field_e100,
            s.audio_maintenance.music.gain_step,
            s.pause_flag};
}
inline FrontendRaceReturnWorld frontend_race_return_world(FrontendState &frontend)
{
    auto &s = frontend.world();
    auto &f = frontend.source;
    return {f.persistence.bytes,   s.options, s.selection, s.trial_selection, f.race_return_selector,
            f.race_return_bindings};
}
inline FrontendLaunchWorld frontend_launch_world(FrontendState &frontend)
{
    auto &s = frontend.world();
    return {frontend.source, s.options, s.selection, s.trial_selection, s.other_selected_vehicles, s.frontend_launch};
}
} // namespace sgn::retail
