#pragma once
#include "sgn/contact_solver.hpp"
#include "sgn/retail_level.hpp"
#include "sgn/vehicle_ai_automatic.hpp"

namespace sgn::retail
{
// Source per-vehicle driver settings; no copy of physical or automatic AI state.
struct VehicleRaceDriverSettings
{
    float skill{}, mode{};
}; // 1B8C,1B90
struct RaceStartPresentation
{
    virtual ~RaceStartPresentation() = default;
    virtual void sound(std::uint32_t id, std::uint32_t volume) = 0; // 461150 device boundary
    virtual std::uint32_t selected_texture() const = 0;             // 61B634
    virtual void texture(std::uint32_t slot, std::uint32_t id) = 0; // 430D40 resource boundary
};
struct RaceStartWorld
{
    std::span<VehicleState, 6> physics;
    std::span<VehicleRaceState, 6> race;
    std::span<VehicleRaceDriverSettings, 6> drivers;
    std::span<AiAutomaticMemory, 6> ai;
    std::span<const LevelCarSettings, 6> cars;
    RaceClockState &clock;
    contact::player::Controls &controls;
    unsigned &current_slot;
    std::array<float, 3> torque_thresholds; // 60F30C/310/314, source order
    RaceStartPresentation &presentation;
};
void choose_race_torque_curve(const VehicleRaceDriverSettings &, VehicleState &,
                              const std::array<float, 3> &); // 405880
void configure_race_drivers(RaceStartWorld &);               // full 4058F0 with 405880
void begin_race_timers(RaceStartWorld &);                    // full 444C20
void hold_start_vehicles(std::span<VehicleState, 6>, std::span<const VehicleRaceState, 6>,
                         const RaceClockState &) noexcept;               // full 412780
void tick_race_countdown(RaceStartWorld &);                              // 444C50, explicit device/resource boundaries
void tick_vehicle_notice(RaceStartWorld &);                              // 443E20, same boundaries
void refill_vehicle(const VehicleParameters &, VehicleState &) noexcept; // 40E360
void refill_impact_energy(VehicleState &, contact::solver::State &) noexcept; // 40E380
} // namespace sgn::retail
