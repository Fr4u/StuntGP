#pragma once
#include "sgn/vehicle_progression.hpp"

namespace sgn::retail
{
// 445120 fixes the playable challenge domain to mode3/lap_count100. A lap can
// still cross the spline boundary, but record notices and checkpoints return.
void complete_challenge_lap(const VehicleState &, VehicleRaceState &, VehicleProgressionState &, AiAutomaticMemory &,
                            const VehicleProgressionContext &); // 41DD60 mode3
VehicleProgressionObservation tick_challenge_progression(const VehicleState &, VehicleRaceState &,
                                                         VehicleProgressionState &, AiAutomaticMemory &,
                                                         const VehicleProgressionContext &); // 41E210 mode3

struct ChallengeClockAudio : VehicleScoreAudio
{
    // 461150 sample22, EDX0, frequency,volume,pan0,force1,null.
    virtual void countdown_beep(std::uint32_t frequency, std::uint32_t volume) = 0;
};
struct ChallengeClockWorld
{
    std::span<VehicleState, 6> physical;
    std::span<VehicleRaceState, 6> race;
    RaceClockState &clock;
    std::uint32_t &score_block; // sole60D680, also the180-tick ending countdown
    contact::player::Controls &controls;
    unsigned current{};
    std::uint32_t tick{};
    VehicleScoreState &score_popups;
    std::uint32_t hud_block{};
    std::string_view time_up_text, ten_seconds_text; // 612854,612924
    ChallengeClockAudio &audio;
};
// No new persistent state: source absolute49C804/49C5C8/49C6D8 refer to slot0.
void tick_challenge_clock(ChallengeClockWorld &);         // 443780..44388A, mode3 arm
void tick_challenge_end_countdown(ChallengeClockWorld &); // 443890..44394E
} // namespace sgn::retail
