#pragma once
#include "sgn/contact_landing_particles.hpp"
#include "sgn/contact_vehicle_particle_state.hpp"

namespace sgn::retail::contact::effects
{
std::array<std::optional<unsigned>, 4> emit_tire_particles(const VehicleState &, const VehiclePresentationState &,
                                                           VehicleParticleState &, const WorldGravity &,
                                                           VehicleRandom &, EffectParticleBank &,
                                                           const ModelTransform &template_pose,
                                                           EffectParticleFrameAudit *audit = nullptr); // 415880/4157F0
}
