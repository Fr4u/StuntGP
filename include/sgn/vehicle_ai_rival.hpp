#pragma once
#include "sgn/contact_ai_stunts.hpp"
#include "sgn/contact_rail_state.hpp"
#include "sgn/retail_race_start.hpp"
#include "sgn/retail_world.hpp"
#include "sgn/vehicle_tick.hpp"

namespace sgn::retail
{
struct AiRivalSettings
{
    float boost_distance{}, boost_gain{}, leader_distance{}, leader_penalty{}; // 60F318/31C/320/324
    float turbo_factor{};                                                      // 60F300
    float spline_length{}; // [571328]+18, distinct from clock.course_length
};
// Borrowed owners, not a duplicate vehicle simulation or configuration.
struct AiRivalWorld
{
    std::span<AiAutomaticVehicle, 6> cars;
    std::span<contact::solver::State, 6> contacts;
    std::span<const VehicleRaceDriverSettings, 6> drivers;
    std::span<const VehicleRestartState, 6> restart;
    std::span<const VehicleTickState, 6> tick_state;
    std::span<AiStuntHelperState, 6> stunts;
    const RaceRanking &ranking;
    const RaceClockState &clock;
    const RailState &rail;
    const AiStuntKnots &knots;
    AiAutomaticGlobals &globals;
    const AiAutomaticEnvironment &environment;
    const AiRivalSettings &settings;
    VehicleRandom &random;
    std::uint32_t tick{};
};
void adjust_ai_skill(AiRivalWorld &, unsigned);    // 405960..405A78
void adjust_ai_lateral(AiRivalWorld &, unsigned);  // 4064F0..406604
bool ai_turbo_for_speed(AiRivalWorld &, unsigned); // 407860..4078E0
bool ai_turbo_for_stunt(AiRivalWorld &, unsigned); // 4077B0..407855
void update_ai_turbo(AiRivalWorld &, unsigned);    // 4078F0..407A62
void choose_ai_pit(AiRivalWorld &, unsigned);      // 406D30..406E76, actual4064C0
// 40808D..408106, after the common init/guards and407F60 in ai_automatic_tick.
void update_rival_ai(AiRivalWorld &, unsigned);
} // namespace sgn::retail
