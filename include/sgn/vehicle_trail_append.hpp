#pragma once
#include "sgn/vehicle_trails.hpp"
namespace sgn::retail
{
// Additive entry to the already qualified shared41E510 implementation.
// Caller owns the ring, cursor, count and original profile; model/projection
// and shared basis remain borrowed through the existing bindings.
void append_vehicle_trail_point(VehicleTrailBindings, VehicleVector, std::span<VehicleVector>, std::uint32_t &cursor,
                                std::uint32_t &count, VehicleTrailParameters);
// Full shared41E410. False preserves every output lane when behind near clip;
// successful projection writes XYZ and preserves the caller's existing W.
bool project_vehicle_trail_point(VehicleVector, const VehicleTrailProjection &, VehicleVector &);
} // namespace sgn::retail
