#pragma once
#include "sgn/vehicle.hpp"
#include <optional>

namespace sgn::retail
{
// One owner per world, shared by all active physical and replay vehicles.
// 0040FC90 writes 004D5B70; 00419B80 reads its retained value.
struct VehicleDynamicsWorkspace
{
    VehicleMatrix steering{};
};
// Same qualified prefix as vehicle.hpp. This overload also exposes the actual
// shared write, at its original position in the instruction order.
void vehicle_dynamics_core(VehicleParameters &, VehicleState &, const VehicleSupport &, const VehicleDynamicsContext &,
                           VehicleRandom &, VehicleDynamicsWorkspace &);
// Full callers retain 405860's binary64 product until its actual FSTP. An
// absent value also preserves the source branch which performs no addition.
// The older overloads keep their already-stored float modifier contract.
void vehicle_dynamics_core(VehicleParameters &, VehicleState &, const VehicleSupport &, const VehicleDynamicsContext &,
                           VehicleRandom &, VehicleDynamicsWorkspace &, std::optional<double> source_drag_adjustment);
} // namespace sgn::retail
