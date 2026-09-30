#include "sgn/contact_particle_allocator.hpp"

namespace sgn::retail::contact::effects
{
std::optional<unsigned> allocate_effect_particle(EffectParticle &staging, EffectParticleBank &bank,
                                                 const std::array<bool, 16> &undefined_frame,
                                                 EffectParticleFrameAudit *audit)
{
    staging.scale = 0;
    staging.alpha = 0;
    staging.animation_frame = 0;
    for (unsigned i = 0; i < bank.particles.size(); ++i)
    {
        auto &slot = bank.particles[i];
        if (slot.active == 1)
            continue;
        const auto previous = slot;
        slot = staging;
        slot.active = 1;
        slot.age = 0;
        slot.angle = 0;
        for (unsigned j = 0; j < 16; ++j)
            if (undefined_frame[j])
                slot.frame_payload[j] = previous.frame_payload[j];
        if (audit)
        {
            audit->undefined[i] = undefined_frame;
            audit->position_sources[i] = nullptr;
        }
        if (staging.source_undefined[0])
            slot.velocity.opaque = previous.velocity.opaque;
        if (staging.source_undefined[1])
            slot.secondary_color = previous.secondary_color;
        if (staging.source_undefined[2])
            slot.field_94 = previous.field_94;
        if (staging.source_undefined[3])
            slot.angular_velocity = previous.angular_velocity;
        if (staging.source_undefined[4])
            slot.color_fade_in = previous.color_fade_in;
        if (staging.source_undefined[5])
            slot.color_fade_out = previous.color_fade_out;
        if (staging.source_undefined[6])
            slot.color = previous.color;
        bank.active_signal = 1;
        return i;
    }
    return std::nullopt;
}
} // namespace sgn::retail::contact::effects
