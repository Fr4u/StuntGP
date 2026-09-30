#pragma once
#include "sgn/contact_effects.hpp"

namespace sgn::retail::contact::effects
{
// Full 00415C00/00415D20. Their legacy stack scalar is never read and has no
// native parameter. Return the allocated slot for independent provenance tracking.
// Both borrow the existing 32-slot impact pool and its shared throttle/cursor.
std::optional<unsigned> emit_refuel_particle(ImpactParticles &, VehicleRandom &, const Vector4 &position,
                                             const Vector4 &velocity, Vec3 vehicle_right);
std::optional<unsigned> emit_refuel_anchor_particle(ImpactParticles &, const Vector4 &position,
                                                    const Vector4 &velocity);
} // namespace sgn::retail::contact::effects
