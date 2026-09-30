#include "sgn/contact_tire_particles.hpp"
#include "sgn/retail_effect_template.hpp"
#include <algorithm>
#include <bit>
#include <cmath>

namespace sgn::retail::contact::effects
{
std::array<std::optional<unsigned>, 4> emit_tire_particles(const VehicleState &car,
                                                           const VehiclePresentationState &presentation,
                                                           VehicleParticleState &state, const WorldGravity &gravity,
                                                           VehicleRandom &random, EffectParticleBank &bank,
                                                           const ModelTransform &template_pose,
                                                           EffectParticleFrameAudit *audit)
{
    std::array<std::optional<unsigned>, 4> result;
    const float shift = std::bit_cast<float>(std::uint32_t{0xc1f00001});
    const Vec3 offset{vehicle_f32(double(car.up.x) * shift), vehicle_f32(double(car.up.y) * shift),
                      vehicle_f32(double(car.up.z) * shift)};
    bool initialized = false;
    EffectParticle staging;
    for (unsigned j = 0; j < 4; ++j)
    {
        if (!(car.speed < 100))
            continue;
        const auto &wheel = car.wheels[j];
        auto &accumulated = state.field_1ff8[j];
        if (wheel.grip_usage < 3.5f)
        {
            const double decreased = double(accumulated) - 2;
            accumulated = vehicle_f32(decreased);
            if (decreased < 20)
                accumulated = 20;
        }
        else
        {
            if (!std::isfinite(wheel.surface_rolling) || wheel.surface_rolling == 0)
                throw FormatError("tire particle rolling divisor outside finite nonzero domain");
            const double usage = std::min(std::abs(double(wheel.grip_usage) * 0.25), 1.0);
            accumulated = vehicle_f32(((usage * wheel.surface_grip) * 20) / wheel.surface_rolling + accumulated);
        }
        if (!(accumulated > 200))
            continue;
        const double speed = double(car.speed) * std::bit_cast<float>(std::uint32_t{0x3a83126f});
        float amount = vehicle_f32(speed);
        if (speed < std::bit_cast<float>(std::uint32_t{0x3c23d70a}))
            amount = std::bit_cast<float>(std::uint32_t{0x3c23d70a});
        accumulated = 175;
        if (!initialized)
        {
            staging.flags = 7;
            staging.texture = 7;
            staging.render_flags = 0x60;
            staging.lifetime_mode = 2;
            staging.gravity_scale =
                vehicle_f32(double(gravity.inverse_factor) * std::bit_cast<float>(std::uint32_t{0xbe99999a}));
            staging.base_color = 0x406090;
            staging.lifetime = 30;
            staging.field_68 = std::bit_cast<float>(std::uint32_t{0x3c23d70a});
            staging.render_enabled = 1;
            staging.animation_binding = 0;
            staging.scale_fade_in = 1.5f;
            staging.scale_fade_out = 6;
            staging.base_scale = 1;
            staging.alpha_fade_in = 0;
            staging.alpha_fade_out = 21;
            staging.base_alpha = 255;
            copy_effect_template(staging, template_pose);
            staging.source_undefined.fill(true);
            staging.source_undefined[3] = false;
            initialized = true;
        }
        const auto &center = presentation.wheel_centers[j];
        staging.position.x = vehicle_f32(double(offset.x) + center.x);
        staging.position.y = vehicle_f32(double(offset.y) + center.y);
        staging.position.z = vehicle_f32(double(offset.z) + center.z);
        staging.velocity.x = vehicle_f32(double(amount) * car.velocity.x);
        staging.velocity.y = vehicle_f32(double(amount) * car.velocity.y);
        staging.velocity.z = vehicle_f32(double(amount) * car.velocity.z);
        const float x = vehicle_f32(random.signed_value(5));
        const double z = random.signed_value(5);
        staging.velocity.x = vehicle_f32(double(x) + staging.velocity.x);
        staging.velocity.z = vehicle_f32(z + staging.velocity.z);
        if (j >= 2)
            staging.base_scale = std::bit_cast<float>(std::uint32_t{0x3f333333});
        staging.angle = vehicle_f32(random.signed_value(360));
        staging.angular_velocity = vehicle_f32(random.signed_value(45) - 22.5);
        result[j] = allocate_effect_particle(staging, bank, {}, audit);
    }
    return result;
}
} // namespace sgn::retail::contact::effects
