#pragma once
#include "sgn/contact_takeoff_particles.hpp"
#include <optional>

namespace sgn::retail::contact::effects
{
struct EffectParticleFrameAudit
{
    std::array<std::array<bool, 16>, 64> undefined{};
    // Read-only provenance of4173E0's copied wheel-center payload. A later
    // allocation clears this identity even when it copies the same numeric W.
    std::array<const float *, 64> position_sources{};
};
// Full0045E110, using the existing single world-owned 64-slot bank.
// Source staging's scale/alpha/animation_frame are reset even when full.
// source_undefined marks the seven explicitly modeled opaque staging fields;
// those destination words are retained, all defined fields are copied exactly.
// A caller with additional undefined source fields needs a reviewed contract.
// 4173E0 additionally leaves the sixteen frame words unwritten. Their explicit
// native policy is to retain destination payload, with optional reader diagnostics.
std::optional<unsigned> allocate_effect_particle(EffectParticle &staging, EffectParticleBank &,
                                                 const std::array<bool, 16> &undefined_frame = {},
                                                 EffectParticleFrameAudit *audit = nullptr);
} // namespace sgn::retail::contact::effects
