#pragma once
#include "sgn/vehicle_restart.hpp"

namespace sgn::retail
{
// Fields first represented by the full 418830 integration. All other physical,
// contact, race, input and presentation fields retain their existing owners.
struct VehicleTickState
{
    float performance_gap{};                               // 2638
    std::uint32_t start_boost_ticks{};                     // 76FC
    std::uint32_t low_energy_phase{}, low_energy_notice{}; // 2578/7704
};
struct VehiclePerformanceSettings
{
    float route_length{};                       // descriptor1+18
    float upgraded_distance{}, upgraded_gain{}; // 60F318/328
    float normal_distance{}, normal_gain{};     // 60F32C/330
};
// Six physical slots; fallback is the literal original slot2, even if inactive.
unsigned performance_reference(std::span<const VehicleRaceState, 6>) noexcept; // 408180
void update_vehicle_performance(VehicleState &, const VehicleRestartState &, VehicleTickState &,
                                std::span<const VehicleRaceState, 6>, unsigned current,
                                const VehiclePerformanceSettings &);                                  // 4081D0
void reset_vehicle_com(const VehicleParameters &, VehicleState &, contact::solver::State &) noexcept; // 410AD0
void update_vehicle_com(const VehicleParameters &, VehicleState &, contact::solver::State &,
                        Vec3 unit_gravity); // 410B30
struct VehicleStartBoostContext
{
    std::int32_t race_mode{}, phase{};   // 60EFF0 /60F2B4 (notice guard, not game_mode)
    std::uint32_t tick{}, score_award{}; // 612CFC/60D6EC
    std::string_view text;               // 61287C
    contact::player::Controls &controls;
    unsigned current;
};
void update_vehicle_start_boost(const VehicleParameters &, VehicleState &, const VehicleRaceState &,
                                contact::solver::State &, VehicleTickState &,
                                const VehicleStartBoostContext &); // 412670
// Full 418830 is not exposed until every nonempty callee is implemented.
} // namespace sgn::retail
