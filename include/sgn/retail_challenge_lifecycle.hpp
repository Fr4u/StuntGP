#pragma once
#include "sgn/retail_normal_race_lifecycle.hpp"
#include "sgn/retail_vehicle_load_selection.hpp"
#include "sgn/vehicle_audio_state.hpp"
#include "sgn/vehicle_challenge_progression.hpp"

namespace sgn::retail
{
struct ChallengeRaceServices
{
    virtual ~ChallengeRaceServices() = default;
    virtual void start_camera() = 0; // 4028D0(7),403310(2.55), actual shared projection
    virtual void request_menu() = 0; // 441E20, including445530 save and source transition request
};
struct ChallengeRaceLifecycleWorld
{
    NormalRaceLifecycleWorld &shared;
    ChallengeClockWorld &countdown;
    std::span<VehicleAudioState, 6> selectors;
    VehicleLoadSelection &load_selection;
    std::span<const VehicleParameters, 6> parameters;
    std::span<contact::solver::State, 6> contact;
    std::string_view start_text;
    ChallengeRaceServices &services;
};
void start_challenge_race(ChallengeRaceLifecycleWorld &);          // 443980, phase_ticks belongs to4439F0
void tick_challenge_race_lifecycle(ChallengeRaceLifecycleWorld &); // 443A80, game_type3 local mode1/3
} // namespace sgn::retail
