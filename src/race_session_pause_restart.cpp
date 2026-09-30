#include "sgn/race_session_pause_restart.hpp"
#include "sgn/race_session.hpp"
#include "sgn/platform_audio.hpp"
#include <bit>

namespace sgn::retail
{
SessionPauseRestartSource::Invocation::Invocation(RaceSession &session, AudioRequestObserver *observer)
    : reset_parameters(session.reset_parameters()), reset_vehicles(session.state().vehicles.reset_views()),
      selected_vehicles(session.selected_vehicles()), audio(session.audio_view()),
      audio_startup{audio,
                    session.state().audio_maintenance,
                    session.state().audio_stream,
                    session.state().audio_initialization,
                    session.state().audio_level,
                    session.state().options,
                    session.assets(),
                    session.audio_device(),
                    session.audio_stream_platform(),
                    session.window()},
      reset{reset_vehicles, session.state().current_slot, session.state().controls, session.state().reset_globals,
            reset_parameters}
{
    // This is an invocation-local diagnostic pointer, never a source store.
    audio.request_observer = observer;
}

SessionPauseRestartSource::SessionPauseRestartSource(RaceSession &session, AudioRequestObserver *observer) noexcept
    : session_(session), observer_(observer)
{
}

PauseMenuRestartInvocation SessionPauseRestartSource::borrow_restart()
{
    // Guard before reading any source word, text, spline or platform device.
    session_.shared_platform()->require_owner_thread();
    auto &invocation = invocation_.emplace(session_, observer_);
    auto &s = session_.state();
    return {{s.vehicles,
             session_.assets(),
             invocation.reset,
             session_.splines(),
             s.grid,
             s.load_selection,
             s.clock,
             s.automatic,
             {s.controls, s.hud_countdown, s.hud_layout, s.selected_hud, s.hud_margins},
             s.selected_camera_preset,
             {s.frame_projection, s.inverse_depth_scale, s.depth_offset, s.shared_model_identity},
             s.startup_counters,
             s.startup,
             invocation.audio_startup,
             s.music},
            {s.cars, s.selection, invocation.selected_vehicles, std::bit_cast<std::int32_t>(s.control_kind),
             s.scene_mode, s.level.music_track},
            s.current_slot};
}
} // namespace sgn::retail
