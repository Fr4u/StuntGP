#pragma once
#include "sgn/vehicle_air_tick.hpp"
#include "sgn/vehicle_dynamics_shared.hpp"
#include "sgn/vehicle_wake.hpp"

namespace sgn::retail
{
struct VehicleDynamicsVehicle
{
    VehicleParameters &parameters;
    VehicleState &physics;
    const VehicleSupport &support;
    VehicleAirTickState &air;
    VehiclePositionHistory &history;
};
struct VehicleDynamicsTickContext
{
    const VehicleDynamicsContext &dynamics;
    VehicleDynamicsWorkspace &workspace;
    std::span<VehicleWakeVehicle, 6> world;
    unsigned current;
    const VehicleAirTickContext &air;
    float upgrade_drag_multiplier{}; // original world global 0060F308
};
// Full 0040FC90..00410772, including all four tail calls starting at 0041075A.
// The stock gate and expanded modifier gate remain separately recorded.
// No tick is scaled by display frame time. Legacy dynamics.special_drag must
// be zero here; this full caller reads the actual world modifier above.
VehicleAirTickObservation vehicle_dynamics_tick(VehicleDynamicsVehicle, const VehicleDynamicsTickContext &);
} // namespace sgn::retail
