#pragma once
#include "sgn/vehicle.hpp"

namespace sgn::retail
{
// Full 00419B80..00419E2E. The matrix is the shared 004D5B70 value,
// last written by 0040FC90, not a matrix synthesized from replay steering.
void replay_tyre_response(const VehicleParameters &, VehicleState &, const VehicleMatrix &shared_steering);
void replay_contact_state(VehicleState &);                      // full 00419E30, including 0040FB30
void replay_braking_signals(VehicleState &);                    // full 00419E90 and both signal callees
void align_airborne_vehicle(VehicleState &, Vec3 unit_gravity); // full 004062B0..004064AC
} // namespace sgn::retail
