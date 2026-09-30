#pragma once
#include "sgn/vehicle_reset.hpp"

namespace sgn::retail
{
struct VehicleResetBounds
{
    std::array<float, 256> left{}, right{}; // 60E7F0 / 60EBF0
};
void clear_accumulated_impact_loss(contact::solver::State &) noexcept; // 00408D80
void check_stuck_vehicle(VehicleState &, VehicleRaceState &) noexcept; // 004187C0
bool finish_vehicle_bounds(VehicleResetWorld &,
                           const VehicleResetBounds &); // full 00418320; returns placement diagnostic
} // namespace sgn::retail
