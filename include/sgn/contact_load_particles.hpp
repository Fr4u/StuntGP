#pragma once
#include "sgn/contact_solver.hpp"
#include "sgn/contact_vehicle_particle_state.hpp"
#include "sgn/vehicle_audio_state.hpp"

namespace sgn::retail::contact::effects
{
unsigned emit_load_particle(ImpactParticles &, VehicleRandom &, const VehicleState &, const Vector4 &position,
                            const Vector4 &velocity, float strength); // 417B50
std::array<std::optional<unsigned>, 2> emit_load_particles(ImpactParticles &, VehicleRandom &, const VehicleState &,
                                                           const solver::State &, VehicleParticleState &,
                                                           VehicleAudioState &, float gravity_magnitude,
                                                           solver::PayloadAudit *audit = nullptr); // 417CA0
} // namespace sgn::retail::contact::effects
