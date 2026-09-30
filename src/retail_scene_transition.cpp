#include "sgn/retail_scene_transition.hpp"
#include <cstring>

namespace sgn::retail
{
namespace
{
void all_overlays(SceneTransitionWorld &w, std::uint32_t value, std::uint32_t target)
{
    // 442A40 always leaves4722D0 at controller3, including inactive controls.
    for (unsigned i = 0; i < 4; ++i)
    {
        w.controls.current = i;
        if (w.controls.controllers[i].active == 1)
            set_overlay_transition(w.overlays[i], 2, 60, 0, 60, value, target, 1);
    }
}
} // namespace
bool select_completed_scene_ghost(SceneRaceResultWorld &w)
{
    // 443330: only physical role2/3, unsigned bounds and strict comparison.
    // The active word is not a predicate; ties retain the first physical role.
    const TimeTrialGhost *selected = nullptr;
    std::uint32_t best = 0xffffffffu;
    for (unsigned i = 2; i < 4; ++i)
    {
        const auto &candidate = w.ghosts.roles[i];
        if (candidate.lap_ticks < best && candidate.lap_ticks < 0x1c20u)
        {
            selected = &candidate;
            best = candidate.lap_ticks;
        }
    }
    // 442FD9..443008 compares against physical role1, not role0 or a saved copy.
    if (!selected || selected->lap_ticks >= w.ghosts.roles[1].lap_ticks)
        return false;
    // 442FEC/442FF1 precede the actual replay header read. Retain these
    // source stores if the reached replay identity cannot resolve its bytes.
    w.trial_selection.result_b.vehicle = selected->vehicle;
    w.trial_selection.result_b.replay = selected->replay;
    const auto at = ReplayBank::base(selected->replay) + 0x3cu;
    std::uint32_t header_word;
    std::memcpy(&header_word, w.recordings.bytes().data() + at, sizeof(header_word));
    w.trial_selection.result_b.header_value = header_word; // 442FFF
    return true;
}
void save_scene_race_result(SceneRaceResultWorld &w)
{
    // 445530..445601, including actual442FD0/443330 and replay-header read.
    w.options.display_selector = w.camera_preset;
    if (w.game_type == 4)
    {
        w.trial_selection.previous_completed = w.ghosts.reference_completed;
        if (select_completed_scene_ghost(w))
        {
            w.trial_selection.selected.header_value = w.trial_selection.result_b.header_value; // 44556B
            w.trial_selection.selected.replay = w.trial_selection.result_b.replay;             // 445571
            w.trial_selection.selected.vehicle = w.trial_selection.result_b.vehicle;           // 445576
        }
    }
    w.options.record = w.records.lap_ticks;
    w.options.record_f004 = w.finish.best_total_ticks;
    w.options.record_f008 = w.records.score;
    w.options.record_f00c = w.finish.best_total_score;
    for (unsigned i = 0; i < 6; ++i)
    {
        const auto &race = w.vehicles[i];
        if (race.active == 1)
        {
            auto &out = w.selection[i].previous_race;
            out.finish_state_plus_one = race.finish_state + 1u; // 4455C0 before lap read/store
            out.lap_index = race.lap_index;                     // 4455C9
            out.best_lap_ticks = race.best_lap_ticks;
            out.race_timer = race.race_timer;
            out.total_ticks = race.total_ticks;
            out.stunt_score = race.stunt_score;
        }
    }
}
void request_scene_transition(SceneTransitionWorld &w, SceneRaceResultWorld &result)
{
    // 441E20..441EB5. pending is tested for exactly1, not for nonzero.
    if (w.pending == 1)
        return;
    if (w.scene != 1)
    {
        w.target = 1;
        w.pending = 1;
        w.transition_ticks = 0;
        return;
    }
    w.target = 2;
    save_scene_race_result(result);
    if (w.suppress_exit_overlay != 1)
        all_overlays(w, w.frame_pause == 1 ? 60u : 0u, 100);
    w.sound_output = 0; // 460CA0
    w.music_stream_delay = 0;
    w.music_gain_step = 0xfffffffbu; // 45F8E0(0,-5)
    w.transition_ticks = 0;
    w.pending = 1;
}
void unload_scene_resources(SceneTransitionServices &services)
{
    services.perform(SceneOperation::release_image_resources);
    services.perform(SceneOperation::release_allocations);
    services.select_audio_environment(0); // 4420EA XOR ECX,ECX before tail460AF0
}
void initialize_scene(SceneTransitionWorld &w, SceneLifecycleState &state, SceneTransitionServices &services)
{
    w.audio_pause = 0; // 442030 writes the same4D5B20 read by41BEA0 vehicle audio.
    for (const auto operation :
         {SceneOperation::stop_music, SceneOperation::stop_sound_instances, SceneOperation::reset_sound_output,
          SceneOperation::reset_scene_state, SceneOperation::begin_allocations, SceneOperation::load_scene_resources,
          SceneOperation::ensure_score_configuration, SceneOperation::ensure_gameplay_configuration})
        services.perform(operation);
    // Each scene predicate rereads612BE8 after all preceding synchronous calls.
    if (w.scene == 1)
        for (const auto operation :
             {SceneOperation::prepare_race_state, SceneOperation::configure_level,
              SceneOperation::initialize_vehicle_numeric, SceneOperation::initialize_vehicle_state,
              SceneOperation::initialize_vehicle_presentation, SceneOperation::initialize_vehicle_models,
              SceneOperation::initialize_color_tables, SceneOperation::reset_recordings,
              SceneOperation::initialize_race})
            services.perform(operation);
    if (w.scene == 2)
    {
        services.initialize_menu_viewport(1);
        if (state.menu_initialized == 0)
        {
            state.menu_initialized = 1;
            services.perform(SceneOperation::initialize_menu);
        }
        else
            services.perform(SceneOperation::return_to_menu);
    }
    if (w.scene == 1)
        services.perform(SceneOperation::leave_menu_for_race);
    services.reset_tick_clock();
}
bool dispatch_scene_transition(SceneTransitionWorld &w, SceneLifecycleState &state, SceneTransitionServices &services)
{
    // 441DC0..441E17 reads the physical controller0 overlay, regardless ofcursor.
    if (w.target == w.scene || (w.scene == 1 && w.overlays[0].active == 1))
        return false;
    unload_scene_resources(services);
    w.pending = 0;
    w.scene = w.target;
    initialize_scene(w, state, services);
    all_overlays(w, 100, 0);
    return true;
}
} // namespace sgn::retail
