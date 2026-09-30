#pragma once
#include "sgn/retail_vehicle_bootstrap.hpp"
namespace sgn::retail
{
// Complete4130D0: cache/find spline1 before dispatch, mode1 original grid,
// mode2 full412E80 using spline2, other unsigned modes no-op. Missing spline1
// returns false; a missing/invalid spline2 is rejected instead of guest memory
// access. No extra spline owner or opaque car byte array is introduced.
bool initialize_race_grid(VehicleStorage &, VehicleResetWorld &, const SplineSet &, VehicleGridState &,
                          std::uint32_t mode, VehicleGridPayloadAudit *audit = nullptr);
} // namespace sgn::retail
