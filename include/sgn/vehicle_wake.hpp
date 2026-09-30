#pragma once
#include "sgn/retail_race.hpp"

namespace sgn::retail
{
struct VehiclePositionSample
{
    VehicleVector position; // 1F2C + i*14, including the authored W lane
    float radius{};         // 1F3C + i*14
};
struct VehiclePositionHistory
{
    std::array<VehiclePositionSample, 8> samples;
    std::uint32_t ticks{}; // 1FCC
};
struct VehicleWakeVehicle
{
    VehicleState &physics;
    const VehicleRaceState &race;
};
// Complete 0040E080..0040E28A. Writes other vehicles' sole air_factor owner.
void apply_vehicle_wake(std::span<VehicleWakeVehicle, 6>, unsigned current, const VehiclePositionHistory &);
// Complete 0040E290..0040E33B, including the non-Time-Trial wake branch.
void update_vehicle_position_history(std::span<VehicleWakeVehicle, 6>, unsigned current, VehiclePositionHistory &,
                                     std::int32_t game_mode);
} // namespace sgn::retail
