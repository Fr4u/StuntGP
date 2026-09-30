#include "sgn/contact_vehicle_particles.hpp"
#include <bit>

namespace sgn::retail::contact::effects
{
std::array<std::optional<unsigned>, 2> emit_launch_wheel_particles(VehicleState &car,
                                                                   const VehiclePresentationState &presentation,
                                                                   std::uint32_t rate_selector, VehicleRandom &random,
                                                                   EffectParticleBank &bank,
                                                                   EffectParticleFrameAudit *audit)
{
    std::array<std::optional<unsigned>, 2> result;
    if (rate_selector < 80 || car.contact_counter != 0 || car.stunt.launch_state > 60)
        return result;
    ++car.stunt.launch_state;
    if (car.stunt.launch_state & 1)
        return result;
    EffectParticle staging;
    staging.flags = 3;
    staging.texture = 0;
    staging.animation_binding = 0;
    staging.render_flags = 0x60;
    staging.lifetime_mode = 2;
    staging.lifetime = 41;
    staging.field_68 = std::bit_cast<float>(std::uint32_t{0x3c23d70a});
    staging.gravity_scale = 0.25f;
    staging.render_enabled = 1;
    staging.base_scale = 1;
    staging.scale_fade_in = std::bit_cast<float>(std::uint32_t{0x3f199999});
    staging.scale_fade_out = 24;
    staging.base_alpha = 224;
    staging.alpha_fade_in = staging.scale_fade_in;
    staging.alpha_fade_out = 18;
    staging.base_color = 0x182010;
    staging.source_undefined.fill(true);
    std::array<bool, 16> undefined_frame;
    undefined_frame.fill(true);
    const Vec3 velocity{vehicle_f32(double(car.velocity.x) * 0.5), vehicle_f32(double(car.velocity.y) * 0.5),
                        vehicle_f32(double(car.velocity.z) * 0.5)};
    for (unsigned i = 0; i < 2; ++i)
    {
        const auto &center = presentation.wheel_centers[i];
        staging.position = {center.x, center.y, center.z, std::bit_cast<std::uint32_t>(center.w)};
        staging.velocity.x = vehicle_f32(random.signed_value(5) + velocity.x);
        staging.velocity.y = velocity.y;
        staging.velocity.z = velocity.z;
        result[i] = allocate_effect_particle(staging, bank, undefined_frame, audit);
        if (audit && result[i])
            audit->position_sources[*result[i]] = &center.w;
    }
    return result;
}
} // namespace sgn::retail::contact::effects
