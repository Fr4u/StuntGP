#include "sgn/race_session_iteration.hpp"
#include "sgn/race_session_music.hpp"
#include "sgn/source_music_calls.hpp"
#include "sgn/race_session_pause.hpp"
#include "sgn/retail_hud_lifetime.hpp"
#include "sgn/retail_tick_clock.hpp"

namespace sgn::retail
{
void advance_session_iteration(RaceSession &session, RaceTextureReplacement &textures,
                               std::span<const contact::effects::EffectAnimationFrames> animations,
                               SessionWorldTickAudit *audit)
{
    session.shared_platform()->require_owner_thread();
    auto &s = session.state();
    if (s.hud_block != 0)
        throw FormatError("Session iteration requires the local non-demo domain");
    // 442838..44283F: the source gate's exact-one comparisons are authoritative.
    if (advance_pause_gate_4423d0(session_pause_gate_view(s)) == 0)
        tick_session_world(session, textures, animations, audit); // 442841..44285F
    update_controller_overlays(s.controls, s.overlays);           // 442864 ->442BE0
    update_race_hud({s.controls, s.hud_countdown, s.hud_layout, s.selected_hud, s.hud_margins}, s.vehicles.physics,
                    s.vehicles.race); // 442869 ->447C90
    // 44286E ->40DE30: hud_block == 0 takes 40DE37 directly to RET at40DE74.
    advance_retail_tick(s.simulation_tick, s.tick_clock); // 442873 ->441EC0
    if (session.shared_platform()->source_construction_only_entry())
        advance_source_music_delay_441f70(*session.shared_platform());
    else
        advance_audio_music_delay(audio_stream_world(session), s.music, s.startup.start_delay_ticks); // 442878 ->441F70
}
} // namespace sgn::retail
