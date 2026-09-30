#include "sgn/contact_landing_particles.hpp"
#include "sgn/retail_effect_template.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail::contact::effects
{
std::array<std::optional<unsigned>, 4> emit_landing_wheel_particles(const VehicleState &car,
                                                                    const VehiclePresentationState &presentation,
                                                                    const WorldGravity &gravity, VehicleRandom &random,
                                                                    EffectParticleBank &bank,
                                                                    const ModelTransform &template_pose,
                                                                    EffectParticleFrameAudit *audit)
{
    std::array<std::optional<unsigned>, 4> result;
    if (std::bit_cast<std::int32_t>(car.stunt.landing_hold) < 30)
        return result;
    const float threshold = vehicle_f32((double(gravity.magnitude) * car.effective_mass) * 0.5);
    bool initialized = false;
    EffectParticle staging;
    Vec3 base_velocity{}, lift{};
    for (unsigned j = 0; j < 4; ++j)
    {
        if (!(car.wheels[j].load > threshold))
            continue;
        if (!std::isfinite(threshold) || threshold == 0)
            throw FormatError("landing particle load divisor outside finite nonzero domain");
        if (!initialized)
        {
            copy_effect_template(staging, template_pose);
            staging.flags = 3;
            staging.texture = 0;
            staging.animation_binding = 0;
            staging.render_flags = 0x60;
            staging.gravity_scale =
                vehicle_f32(double(gravity.inverse_factor) * std::bit_cast<float>(std::uint32_t{0xbe19999a}));
            staging.lifetime_mode = 2;
            staging.lifetime = 30;
            staging.field_68 = std::bit_cast<float>(std::uint32_t{0x3c23d70a});
            staging.render_enabled = 1;
            staging.base_scale = 0.5f;
            staging.scale_fade_in = 6;
            staging.scale_fade_out = 12;
            staging.base_alpha = 128;
            staging.alpha_fade_in = std::bit_cast<float>(std::uint32_t{0x3f199999});
            staging.alpha_fade_out = 12;
            staging.base_color = 0x182010;
            staging.source_undefined.fill(true);
            const float vy = vehicle_f32(double(car.velocity.y) * 0.5), vz = vehicle_f32(double(car.velocity.z) * 0.5);
            const float uy = vehicle_f32(double(car.up.y) * 1.5), uz = vehicle_f32(double(car.up.z) * 1.5);
            base_velocity = {vehicle_f32(double(car.up.x) * 1.5 + double(car.velocity.x) * 0.5),
                             vehicle_f32(double(uy) + vy), vehicle_f32(double(uz) + vz)};
            const float factor = std::bit_cast<float>(std::uint32_t{0x42700001});
            lift = {vehicle_f32(double(car.up.x) * factor), vehicle_f32(double(car.up.y) * factor),
                    vehicle_f32(double(car.up.z) * factor)};
            initialized = true;
        }
        const auto &center = presentation.wheel_centers[j];
        staging.position.x = vehicle_f32(double(center.x) - lift.x);
        staging.position.y = vehicle_f32(double(center.y) - lift.y);
        staging.position.z = vehicle_f32(double(center.z) - lift.z);
        const float ratio = vehicle_f32(double(car.wheels[j].load) / threshold);
        double side = double(ratio) * 2 + std::abs(random.signed_value(2));
        if (j & 1)
            side = -side;
        const float x = vehicle_f32(side * car.basis_x.x), y = vehicle_f32(side * car.basis_x.y);
        staging.velocity.x = vehicle_f32(double(x) + base_velocity.x);
        staging.velocity.y = vehicle_f32(double(y) + base_velocity.y);
        staging.velocity.z = vehicle_f32(side * car.basis_x.z + base_velocity.z);
        result[j] = allocate_effect_particle(staging, bank, {}, audit);
    }
    return result;
}
} // namespace sgn::retail::contact::effects
