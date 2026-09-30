#pragma once
#include "sgn/retail_time_trial.hpp"
#include "sgn/vehicle_progression.hpp"

namespace sgn::retail
{
// Borrowed live owners. The source's fixed car0 in443480 is deliberately
// distinct from both the selected controller vehicle and global current_slot.
struct TimeTrialGhostWorld
{
    std::span<VehicleState, 6> physical;
    std::span<VehicleRaceState, 6> race;
    std::span<VehicleProgressionState, 6> progression;
    std::span<const LevelCarSlot, 6> cars;
    ReplayBank &replays;
    TimeTrialGhosts &ghosts;
    TimeTrialSelectionState &selection;
    TimeTrialStartState &start;
    const RaceClockState &clock;
    contact::player::Controls &controls;
    const unsigned &current_slot;
    const std::uint32_t &tick;
    std::string_view countdown_text, reference_text, reference_beaten_text,
        personal_beaten_text; // 612928/6127E8/7F0/7EC
};
std::uint8_t replay_checksum(std::span<const std::uint8_t>) noexcept;          // 4199D0
std::uint32_t update_replay_length(ReplayBank &, ReplayHandle);                // 419A40
void finalize_replay_header(ReplayBank &, ReplayHandle, const LevelCarSlot &); // 419A60
void update_reference_ghost(TimeTrialGhostWorld &, unsigned vehicle);          // 443360
void update_personal_ghost(TimeTrialGhostWorld &, unsigned vehicle);           // 443430
void retain_completed_trial(TimeTrialGhostWorld &);                            // 443480
void ensure_active_ghosts(TimeTrialGhostWorld &);                              // 4434F0
void restart_active_ghosts(TimeTrialGhostWorld &);                             // 443530
void tick_time_trial_ghosts(TimeTrialGhostWorld &);                            // 443630
} // namespace sgn::retail
