#pragma once
#include "sgn/race_session_lifecycle.hpp"
#include "sgn/race_session_vehicle_world.hpp"
#include "sgn/effect_particle_lifetime.hpp"

namespace sgn::retail
{
struct SessionWorldTickAudit
{
    VehicleWorldTickAudit *vehicle{};
    contact::solver::PayloadAudit *contact_payload{};
    contact::effects::ImpactParticleLifetimeAudit *impact{};
    void *context{};
    void (*before_stage)(void *, std::uint32_t source_entry) noexcept {};
    void (*after_stage)(void *, std::uint32_t source_entry) noexcept {};
    AudioRequestObserver *audio{}; // read-only actual461150 argument observation
};
struct SessionWorldDevices
{
    RaceTextureReplacement &textures;
    AudioDevice &audio;
    std::span<const contact::effects::EffectAnimationFrames> effect_animations;
};
// The seven real calls at442841..44285F, in source order, once each. Invoke
// only after the outer4423D0 gate accepted an unpaused step. This function does
// not choose a rate, advance612CFC, execute the outer tail or render a frame.
// Animation descriptors and the real texture registry are mandatory inputs;
// unresolved effect identities remain errors, never an empty fallback stage.
void tick_session_world(RaceSession &, RaceTextureReplacement &,
                        std::span<const contact::effects::EffectAnimationFrames>,
                        SessionWorldTickAudit *audit = nullptr);
// The same composition with an explicit platform boundary. Qualification may
// pass a recording decorator that forwards every request to the actual device.
void tick_session_world(RaceSession &, SessionWorldDevices, SessionWorldTickAudit *audit = nullptr);
} // namespace sgn::retail
