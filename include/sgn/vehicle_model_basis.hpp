#pragma once
#include "sgn/vehicle_trails.hpp"

namespace sgn::retail
{
// Full413B60, shared source4D5B30 owner. X/Y stack W retain destination
// payload; forward W is authored. No per-car cache or model mutation.
void prepare_vehicle_model_basis(const VehicleTrailModel &, VehicleMatrix &);
} // namespace sgn::retail
