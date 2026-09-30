#pragma once
#include "sgn/contact_takeoff_particles.hpp"
#include "sgn/retail_light_effects.hpp"

namespace sgn::retail::contact::effects
{
struct EffectAnimationTexture
{
    std::uint32_t identity{}, first_texture{}; // borrowed descriptor+0 resolved texture-row identity
};
struct EffectAngleSample
{
    std::uint32_t index{};
    const float &value; // source-owned adjacent word outside the pinned trig table
};
// Caller-owned provenance for the 2048 packet attribute colors. A true flag
// means source-unwritten color, not a known retail value. Retained native
// opaque color is a storage policy, not the original stack/pool contents.
struct EffectSpriteColorAudit
{
    std::array<bool, 2048> source_undefined{};
};
// Full45E4F0 +45E4B0 and actual434590 packet builder. No bank mutation, second
// particle pool, buffer reset or GPU draw. The caller owns the current packet
// storage, view, source animation resources and any adjacent angle samples.
// Without an audit, selected source-unwritten CC still rejects. With an
// audit, append the same packets and tag only colors actually written;
// defined writes clear their tags. All other tags and the bank are retained.
// The caller must update tags for other producers' writes. A cursor reset
// does not clear retained attribute payload or its tags.
void append_effect_particle_sprites(const EffectParticleBank &, SourceSpritePacketStorage &, LightCameraView,
                                    std::span<const EffectAnimationTexture> animations = {},
                                    std::span<const EffectAngleSample> adjacent = {},
                                    EffectSpriteColorAudit *audit = nullptr);
} // namespace sgn::retail::contact::effects
