#pragma once
#include "sgn/contact_particle_allocator.hpp"
#include "sgn/vehicle_presentation.hpp"

namespace sgn::retail::contact::effects
{
// 4173E0 +417350: borrows existing launch_state7244 and physical wheel centers.
// rate_selector is the one global61297C already stored in ImpactParticles.
std::array<std::optional<unsigned>, 2> emit_launch_wheel_particles(VehicleState &, const VehiclePresentationState &,
                                                                   std::uint32_t rate_selector, VehicleRandom &,
                                                                   EffectParticleBank &,
                                                                   EffectParticleFrameAudit *audit = nullptr);
} // namespace sgn::retail::contact::effects
