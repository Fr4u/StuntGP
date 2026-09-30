#pragma once
#include "sgn/retail_level_selection.hpp"
#include "sgn/vehicle_reset.hpp"
#include "sgn/vehicle_ai_automatic.hpp"
#include "sgn/vehicle_score.hpp"

namespace sgn::retail
{
// Separate controller-owned blocks. Neither is CameraState or Notice storage.
struct ControllerOverlayTransition
{
    std::uint32_t active{}, age{}, initial_value{}, kind{}, hold_ticks{}, duration{}, flags{}, value{}, target{};
    float rate{}; // controller 1E8..20C in the listed order
};
struct ControllerHudCountdown
{
    std::uint32_t active{}, ticks{}, enabled{}; // controller 210,214,228
    float value{};                              // 22C
    std::uint32_t phase{};                      // 230
};
struct TimeTrialStartState
{
    std::uint32_t reference_at_start{}, announcement_ticks{}, phase_ticks{}; // 60D678,60D67C,60F2C4
};
void set_overlay_transition(ControllerOverlayTransition &, std::uint32_t kind, std::uint32_t hold,
                            std::uint32_t duration, std::uint32_t flags, std::uint32_t value, std::uint32_t target,
                            float rate); // 442AD0
void set_hud_countdown(std::span<ControllerHudCountdown, 4>, unsigned controller, unsigned &selected_hud,
                       std::uint32_t ticks);                                   // 447B70
void set_level_audio_output(LevelAudioState &, std::uint32_t output) noexcept; // 460CB0
// Complete 412B10, including real reset. The result is diagnostic: placement ran.
// Source-unwritten heading W retains its previous native value.
bool place_vehicle_at_distance(VehicleResetWorld &, unsigned target, float distance);
struct TimeTrialStartWorld
{
    std::span<ResetVehicle, 6> vehicles;
    std::span<AiAutomaticMemory, 6> ai;
    ReplayBank &replays;
    TimeTrialGhosts &ghosts;
    TimeTrialSelectionState &selection;
    RaceClockState &clock;
    contact::player::Controls &controls;
    VehicleResetGlobals &reset_globals;
    const VehicleResetParameters &reset_parameters;
    VehicleScoreState &score;
    std::span<ControllerOverlayTransition, 4> overlays;
    std::span<ControllerHudCountdown, 4> hud;
    unsigned &current_slot;
    unsigned &selected_hud;
    LevelAudioState &audio;
    TimeTrialStartState &state;
};
// Full 443100 candidate. Source world mode four is assigned at the end, after
// the real placement/reset uses the preceding world mode and phase.
bool start_time_trial(TimeTrialStartWorld &, std::uint32_t rolling_start_offset);
} // namespace sgn::retail
