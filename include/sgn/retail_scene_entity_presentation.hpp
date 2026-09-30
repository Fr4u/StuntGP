#pragma once
#include "sgn/retail_vehicle_presentation_frame.hpp"
#include "sgn/effect_particle_sprites.hpp"

namespace sgn::retail
{
// Borrowed442500 invocation. The vehicle frame carries all shared geometry,
// camera and staging owners; the two particle banks are the existing world
// owners. Animation/sample views come from the actual resource/source owners.
struct SceneEntityPresentation
{
    VehiclePresentationFrame vehicles;
    const contact::effects::EffectParticleBank &effects;
    const contact::effects::ImpactParticles &impacts;
    const contact::effects::ImpactParticleDrawList &impact_draws;
    std::span<const contact::effects::EffectAnimationTexture> animations;
    std::span<const contact::effects::EffectAngleSample> adjacent_angles;
    contact::effects::EffectSpriteColorAudit *effect_color_audit{};
};

// Complete442500 composition. Qualification is separate from the component
// gates and the following42E870 world-consumer stage. No resets or simulation.
VehiclePresentationDraws prepare_scene_entity_presentation(SceneEntityPresentation);
} // namespace sgn::retail
