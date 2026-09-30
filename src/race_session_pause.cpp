#include "sgn/race_session_pause.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
namespace
{
RaceSessionState &checked_state(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    return platform.state();
}
} // namespace
SessionPauseInvocation::SessionPauseInvocation(RaceSessionState &s, AudioDevice &device,
                                               AudioRequestObserver *observer) noexcept
    : cars_{{{s.vehicles.parameters[0], s.vehicles.physics[0], s.vehicles.race[0], s.vehicles.contact[0],
              s.vehicles.load[0], s.vehicles.audio[0], 0},
             {s.vehicles.parameters[1], s.vehicles.physics[1], s.vehicles.race[1], s.vehicles.contact[1],
              s.vehicles.load[1], s.vehicles.audio[1], 1},
             {s.vehicles.parameters[2], s.vehicles.physics[2], s.vehicles.race[2], s.vehicles.contact[2],
              s.vehicles.load[2], s.vehicles.audio[2], 2},
             {s.vehicles.parameters[3], s.vehicles.physics[3], s.vehicles.race[3], s.vehicles.contact[3],
              s.vehicles.load[3], s.vehicles.audio[3], 3},
             {s.vehicles.parameters[4], s.vehicles.physics[4], s.vehicles.race[4], s.vehicles.contact[4],
              s.vehicles.load[4], s.vehicles.audio[4], 4},
             {s.vehicles.parameters[5], s.vehicles.physics[5], s.vehicles.race[5], s.vehicles.contact[5],
              s.vehicles.load[5], s.vehicles.audio[5], 5}}},
      audio_{s.audio_driver, device, s.audio_level, s.controls, s.split_setting, observer},
      vehicle_audio_{cars_, audio_, s.controls, s.vehicle_audio, s.audio_frontier, s.pause_flag},
      lifecycle_{s.startup.field_60f2c0,
                 s.frame_pause,
                 s.pause.age,
                 s.pause.status,
                 s.ui.second_pause,
                 s.pause.physical_input,
                 {s.controls, s.hud_countdown, s.hud_layout, s.selected_hud, s.hud_margins},
                 s.overlays,
                 s.vehicles.physics,
                 vehicle_audio_,
                 s.audio_level,
                 {s.audio_level.output_a, s.audio_stream.playback_active, s.audio_stream.field_e100,
                  s.audio_maintenance.music.gain_step}}
{
}

SessionPauseInvocation::SessionPauseInvocation(RaceSessionPlatform &platform, AudioDevice &device,
                                               AudioRequestObserver *observer)
    : SessionPauseInvocation(checked_state(platform), device, observer)
{
}

void SessionPauseInvocation::enter()
{
    enter_pause_4422c0(lifecycle_);
}
void SessionPauseInvocation::resume()
{
    resume_pause_442350(lifecycle_);
}

PauseGateWorld session_pause_gate_view(RaceSessionState &s) noexcept
{
    return {s.frame_pause,          s.pause.age,       s.pause.raw_activity, s.trial_start.phase_ticks,
            s.startup.field_60f2c0, s.pause.raw_60d620};
}
PauseGateWorld session_pause_gate_view(RaceSessionPlatform &platform)
{
    return session_pause_gate_view(checked_state(platform));
}
void enter_session_pause(RaceSession &session)
{
    session.shared_platform()->require_owner_thread();
    SessionPauseInvocation invocation(session.state(), session.audio_device());
    invocation.enter();
}
void resume_session_pause(RaceSession &session)
{
    session.shared_platform()->require_owner_thread();
    SessionPauseInvocation invocation(session.state(), session.audio_device());
    invocation.resume();
}
} // namespace sgn::retail
