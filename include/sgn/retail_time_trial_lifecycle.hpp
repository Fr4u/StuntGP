#pragma once
#include "sgn/retail_time_trial_tick.hpp"
#include "sgn/retail_race_start.hpp"
#include "sgn/retail_vehicle_load_selection.hpp"
#include "sgn/vehicle_prestart_progression.hpp"

namespace sgn::retail
{
// Call views reference the same world. There are no lifecycle copies of the
// countdown, race clock, controllers, cars, ghosts, replay arena or current ID.
struct TimeTrialLifecycleWorld
{
    TimeTrialStartWorld &start;
    RaceStartWorld &race;
    TimeTrialGhostWorld &ghosts;
    VehicleLoadSelection &load_selection;
    std::span<const VehicleParameters, 6> parameters;
    std::span<contact::solver::State, 6> contact;
    const std::uint32_t &game_type;            // 60DF98, must be4 in this recovered domain
    const std::uint32_t &rolling_start_offset; // 60F014
    const std::uint32_t &hud_block;            // 61299C
};
void validate_time_trial_lifecycle_bindings(const TimeTrialLifecycleWorld &);
void disable_all_vehicle_controls(std::span<VehicleState, 6>, std::span<VehicleRaceState, 6>) noexcept; // 443730
// 443780 in the complete mode1/mode4 TimeTrial lifecycle domain.
void check_time_trial_completion(TimeTrialLifecycleWorld &);
bool dispatch_time_trial_start(TimeTrialLifecycleWorld &); // 4439F0 game_type4
// 443A80 mode1/prestart or mode4. Returns only diagnostic placement information.
bool tick_time_trial_lifecycle(TimeTrialLifecycleWorld &);
} // namespace sgn::retail
