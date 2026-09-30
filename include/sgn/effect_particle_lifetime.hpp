#pragma once
#include "sgn/contact_takeoff_particles.hpp"
#include <span>

namespace sgn::retail::contact::effects
{
// Borrowed descriptor+8 values. Identities are owner-issued resource handles,
// never host pointers. The lifetime stage does not read texture bindings.
struct EffectAnimationFrames
{
    std::uint32_t identity{}, frame_count{};
};
// Full0045E1D0 +0045E170/0045E180/00422620. One call is one source tick.
// The existing64-slot bank remains the sole owner. Inactive becomes2; signal
// remains1 for the final active tick. Animation also advances on that tick.
// Rejects undefined source inputs and unresolved animation handles before any
// mutation. Metadata for CC is cleared only when its source writer executes.
void update_effect_particles(EffectParticleBank &, Vec3 gravity,
                             std::span<const EffectAnimationFrames> animations = {});
} // namespace sgn::retail::contact::effects
