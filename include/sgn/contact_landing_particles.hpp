#pragma once
#include "sgn/contact_particle_allocator.hpp"
#include "sgn/vehicle_presentation.hpp"
#include "sgn/retail_world_globals.hpp"

namespace sgn::retail::contact::effects
{
std::array<std::optional<unsigned>, 4> emit_landing_wheel_particles(
    const VehicleState &, const VehiclePresentationState &, const WorldGravity &, VehicleRandom &, EffectParticleBank &,
    const ModelTransform &template_pose, EffectParticleFrameAudit *audit = nullptr); // 416F30/416EA0
}
