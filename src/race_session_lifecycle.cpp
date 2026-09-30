#include "sgn/race_session_lifecycle.hpp"
#include "sgn/retail_normal_race_lifecycle.hpp"
#include "sgn/race_session_music.hpp"
#include "sgn/source_music_calls.hpp"
#include "sgn/retail_camera_finish_session.hpp"
#include "sgn/retail_model_matrices.hpp"
#include <array>
#include <bit>

namespace sgn::retail
{
namespace
{
class SessionNormalPresentation final : public NormalRacePresentation,
                                        public ChallengeClockAudio,
                                        public ChallengeRaceServices
{
    RaceSession &session_;
    AudioDriver &audio_;

  public:
    SessionNormalPresentation(RaceSession &session, AudioDriver &audio) : session_(session), audio_(audio) {}
    void music(std::uint32_t track, std::uint32_t loop) override
    {
        const auto p = session_.shared_platform();
        if (p->source_construction_only_entry())
            start_source_music_45f8f0(*p, track, loop);
        else
            start_audio_music(audio_stream_world(session_), session_.state().music, track, loop);
    }
    void fade_music() override
    {
        auto &s = session_.state();
        s.audio_stream.field_e100 = 0;
        s.audio_maintenance.music.gain_step = 0xfffffff6;
    }
    void finish_camera() override
    {
        finish_race_camera(session_.state());
    }
    void frequency_sound(std::uint32_t id, std::uint32_t frequency, std::uint32_t volume) override
    {
        audio_start(audio_, id, 0, frequency, volume, 0, 0, nullptr);
    }
    void play(std::uint32_t id, std::uint32_t volume) override
    {
        audio_start(audio_, id, 0, 0, volume, 255, 0, nullptr);
    }
    void countdown_beep(std::uint32_t frequency, std::uint32_t volume) override
    {
        audio_start(audio_, 22, 0, frequency, volume, 0, 1, nullptr);
    }
    void start_camera() override
    {
        start_challenge_session_camera(session_.state());
    }
    void request_menu() override
    {
        request_challenge_scene_transition(session_.state());
    }
};
} // namespace
void start_challenge_session_camera(RaceSessionState &s)
{
    CameraEffectTargets targets;
    for (unsigned i = 0; i < 6; ++i)
    {
        targets.vehicles[i] = &s.vehicles.physics[i];
        targets.active[i] = static_cast<std::uint32_t>(s.vehicles.race[i].active);
    }
    auto &camera = s.controls.controllers.at(s.controls.current).camera;
    camera_select_mode(camera, 7, camera_vehicle_reset_adapter(targets));
    camera_set_lens(camera, std::bit_cast<float>(0x40233333u), s.shared_model_identity);
    s.frame_projection = camera.projection;
    s.inverse_depth_scale = camera.inverse_depth_scale;
    s.depth_offset = camera.depth_offset;
}
void request_challenge_scene_transition(RaceSessionState &s)
{
    auto &p = s.normal_finish;
    if (p.menu_requested == 1)
        return;
    if (s.level.game_type != 3)
        throw FormatError("challenge scene request requires game_type3");
    if (s.scene_mode != 1)
    {
        s.options.selection_phase = 1;
        p.menu_requested = 1;
        p.field_612cf8 = 0;
        return;
    }
    s.options.selection_phase = 2;
    // 445530, non-time-trial branch: save the source result words to selection.
    s.options.display_selector = s.selected_camera_preset;
    s.options.record = s.records.lap_ticks;
    s.options.record_f004 = s.finish.best_total_ticks;
    s.options.record_f008 = s.records.score;
    s.options.record_f00c = s.finish.best_total_score;
    for (unsigned i = 0; i < 6; ++i)
        if (s.vehicles.race[i].active == 1)
        {
            const auto &race = s.vehicles.race[i];
            s.selection[i].previous_race = {race.lap_index,  race.finish_state + 1u, race.best_lap_ticks,
                                            race.race_timer, race.total_ticks,       race.stunt_score};
        }
    if (p.field_612bf0 != 1)
        for (unsigned i = 0; i < 4; ++i)
        {
            s.controls.current = i;
            if (s.controls.controllers[i].active == 1)
                set_overlay_transition(s.overlays[i], 2, 60, 0, 60, s.frame_pause == 1 ? 60u : 0u, 100, 1);
        }
    s.audio_level.output_b_copy = 0;
    s.audio_stream.field_e100 = 0;
    s.audio_maintenance.music.gain_step = 0xfffffffbu;
    p.field_612cf8 = 0;
    p.menu_requested = 1;
}
void SessionRacePresentation::sound(std::uint32_t id, std::uint32_t volume)
{
    // Original461150: ECX=sample,EDX=0; stack frequency0,volume,pan0,force0,null.
    audio_start(audio_, id, 0, 0, volume, 0, 0, nullptr);
}
std::uint32_t SessionRacePresentation::selected_texture() const
{
    return textures_.selected_overlay_texture();
}
void SessionRacePresentation::texture(std::uint32_t slot, std::uint32_t id)
{
    textures_.replace_source_texture(slot, id);
}

bool tick_session_lifecycle(RaceSessionState &s, const VehicleResetParameters &reset_parameters,
                            RaceStartPresentation &presentation, NormalRacePresentation *normal_services,
                            ChallengeClockAudio *challenge_audio, ChallengeRaceServices *challenge_services)
{
    auto reset_vehicles = s.vehicles.reset_views();
    TimeTrialStartWorld start{reset_vehicles,    s.vehicles.ai,  s.replays,     s.ghosts,
                              s.trial_selection, s.clock,        s.controls,    s.reset_globals,
                              reset_parameters,  s.score_popups, s.overlays,    s.hud_countdown,
                              s.current_slot,    s.selected_hud, s.audio_level, s.trial_start};
    // These are immutable invocation snapshots of the six interleaved config
    // records, not separately mutable persistent copies of level settings.
    std::array<LevelCarSettings, 6> settings;
    for (unsigned i = 0; i < 6; ++i)
        settings[i] = s.cars[i].settings;
    RaceStartWorld race{
        s.vehicles.physics, s.vehicles.race, s.vehicles.drivers,
        s.vehicles.ai,      settings,        s.clock,
        s.controls,         s.current_slot,  {s.gameplay.values[15], s.gameplay.values[16], s.gameplay.values[17]},
        presentation};
    if (s.level.game_type == 2 || s.level.game_type == 3)
    {
        NormalRaceLifecycleWorld normal{race,
                                        s.vehicles.startup,
                                        s.trial_start.phase_ticks,
                                        s.selected_hud,
                                        s.level.game_type,
                                        s.level.camera_mode,
                                        s.hud_block,
                                        {s.controls, s.hud_countdown, s.hud_layout, s.selected_hud, s.hud_margins},
                                        s.overlays,
                                        s.cars,
                                        s.finish,
                                        s.normal_finish,
                                        s.startup_counters.field_60d71c,
                                        s.startup.field_60f2c0,
                                        s.audio_level,
                                        s.audio_stream.playback_active,
                                        s.split_setting,
                                        normal_services};
        if (s.level.game_type == 2)
            tick_normal_race_lifecycle(normal);
        else
        {
            if (!challenge_audio || !challenge_services)
                throw FormatError("challenge lifecycle services are required");
            ChallengeClockWorld countdown{s.vehicles.physics,
                                          s.vehicles.race,
                                          s.clock,
                                          s.score_block,
                                          s.controls,
                                          s.current_slot,
                                          s.simulation_tick,
                                          s.score_popups,
                                          s.hud_block,
                                          s.game_text.required(GameTextId::challenge_finished),
                                          s.game_text.required(GameTextId::challenge_hurry),
                                          *challenge_audio};
            ChallengeRaceLifecycleWorld challenge{normal,
                                                  countdown,
                                                  s.vehicles.audio,
                                                  s.load_selection,
                                                  s.vehicles.parameters,
                                                  s.vehicles.contact,
                                                  s.game_text.required(GameTextId::challenge_start),
                                                  *challenge_services};
            tick_challenge_race_lifecycle(challenge);
        }
        return false; // grid placement already occurred in40D500
    }
    TimeTrialGhostWorld ghosts{s.vehicles.physics,
                               s.vehicles.race,
                               s.vehicles.progression,
                               s.cars,
                               s.replays,
                               s.ghosts,
                               s.trial_selection,
                               s.trial_start,
                               s.clock,
                               s.controls,
                               s.current_slot,
                               s.simulation_tick,
                               s.game_text.required(GameTextId::timetrial_start),
                               s.game_text.required(GameTextId::beat_the_laprecord_ghost),
                               s.game_text.required(GameTextId::you_have_beaten_the_goldenghost),
                               s.game_text.required(GameTextId::you_have_beaten_your_userghost)};
    TimeTrialLifecycleWorld world{start,
                                  race,
                                  ghosts,
                                  s.load_selection,
                                  s.vehicles.parameters,
                                  s.vehicles.contact,
                                  s.level.game_type,
                                  s.level.rolling_start_offset,
                                  s.hud_block};
    return tick_time_trial_lifecycle(world);
}
bool tick_session_lifecycle(RaceSession &session, RaceTextureReplacement &textures)
{
    auto audio = session.audio_view();
    return tick_session_lifecycle(session, textures, audio);
}
bool tick_session_lifecycle(RaceSession &session, RaceTextureReplacement &textures, AudioDriver &audio)
{
    auto parameters = session.reset_parameters();
    SessionRacePresentation presentation(audio, textures);
    SessionNormalPresentation normal(session, audio);
    const bool placed = tick_session_lifecycle(session.state(), parameters, presentation, &normal, &normal, &normal);
    if (placed)
        for (unsigned i = 0; i < 6; ++i)
            if (session.state().vehicles.race[i].active == 1)
                ++session.state().presentation_vehicle_continuity[i];
    return placed;
}
} // namespace sgn::retail
