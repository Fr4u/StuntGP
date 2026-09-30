#pragma once
#include "sgn/retail_normal_race_lifecycle.hpp"
#include "sgn/retail_level_selection.hpp"
#include "sgn/vehicle_progression.hpp"

namespace sgn::retail
{
// Borrow the original selection, results and recordings. They must outlive a
// scene's resource allocation; a renderer or menu must not retain copies.
struct SceneRaceResultWorld
{
    LevelOverrideOptions &options;
    std::span<SelectionRecord, 6> selection;
    TimeTrialSelectionState &trial_selection;
    const TimeTrialGhosts &ghosts;
    const ReplayBank &recordings;
    std::span<const VehicleRaceState, 6> vehicles;
    const ProgressionRecordState &records;
    const VehicleRaceFinishState &finish;
    const std::uint32_t &game_type;
    const unsigned &camera_preset;
};
// Full443330/442FD0, then445530. All replay identities borrow recordings.
bool select_completed_scene_ghost(SceneRaceResultWorld &);
void save_scene_race_result(SceneRaceResultWorld &);

struct SceneTransitionWorld
{
    std::uint32_t &scene;                       // 612BE8
    std::uint32_t &target;                      // 612BEC, options.selection_phase
    std::uint32_t &pending;                     // 612BF4
    std::uint32_t &transition_ticks;            // 612CF8
    const std::uint32_t &suppress_exit_overlay; // 612BF0
    const std::uint32_t &frame_pause;           // 61294C
    contact::player::Controls &controls;        // 4722D0 + four shared controllers
    std::span<ControllerOverlayTransition, 4> overlays;
    std::uint32_t &sound_output;       // 48979C
    std::uint32_t &music_stream_delay; // 62E100
    std::uint32_t &music_gain_step;    // 62E108
    std::uint32_t &audio_pause;        // 4D5B20, same world word as vehicle audio
};
// These source globals belong to the long-lived frontend, not a level resource.
struct SceneLifecycleState
{
    std::uint32_t menu_initialized{}; // 60D61C
};

// Mandatory synchronous source dependencies. An adapter must perform each
// operation on the shared world or reject it explicitly; there is no no-op
// default, synthesized menu, callback reordering, or deferred event queue.
enum class SceneOperation : std::uint32_t
{
    release_image_resources = 0x431810,
    release_allocations = 0x42eaa0,
    stop_music = 0x45f9d0,
    stop_sound_instances = 0x460dd0,
    reset_sound_output = 0x460c90,
    reset_scene_state = 0x445fa0,
    begin_allocations = 0x42ea80,
    load_scene_resources = 0x42e6e0,
    ensure_score_configuration = 0x443d70,
    ensure_gameplay_configuration = 0x4457e0,
    prepare_race_state = 0x445f80,
    configure_level = 0x4469f0,
    initialize_vehicle_numeric = 0x40e050,
    initialize_vehicle_state = 0x40a2f0,
    initialize_vehicle_presentation = 0x401de0,
    initialize_vehicle_models = 0x413670,
    initialize_color_tables = 0x422420,
    reset_recordings = 0x4199a0,
    initialize_race = 0x40d500,
    initialize_menu = 0x43d0b0,
    return_to_menu = 0x43d1d0,
    leave_menu_for_race = 0x43d1b0,
};
struct SceneTransitionServices
{
    virtual ~SceneTransitionServices() = default;
    virtual void perform(SceneOperation) = 0;
    virtual void select_audio_environment(std::uint32_t environment) = 0; // 460AF0
    virtual void initialize_menu_viewport(std::uint32_t single_view) = 0; // 424C30
    virtual void reset_tick_clock() = 0;                                  // 442100: two distinct434820 reads
};

void request_scene_transition(SceneTransitionWorld &, SceneRaceResultWorld &);                            // 441E20
void unload_scene_resources(SceneTransitionServices &);                                                   // 4420E0
void initialize_scene(SceneTransitionWorld &, SceneLifecycleState &, SceneTransitionServices &);          // 442030
bool dispatch_scene_transition(SceneTransitionWorld &, SceneLifecycleState &, SceneTransitionServices &); // 441DC0
} // namespace sgn::retail
