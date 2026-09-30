#include "sgn/contact_surface_particles.hpp"
#include "sgn/retail_effect_template.hpp"
#include <algorithm>
#include <bit>
#include <cmath>

namespace sgn::retail::contact::effects
{
namespace
{
void initialize(EffectParticle &p, std::int32_t material, const VehicleState &car, const WorldGravity &gravity,
                const ModelTransform &template_pose)
{
    copy_effect_template(p, template_pose);
    p.flags = 3;
    p.render_flags = 0x60;
    p.lifetime_mode = 2;
    p.texture = 9;
    p.render_enabled = 1;
    p.animation_binding = 0;
    const float velocity_scale = material == 4 ? 0.75f : 0.5f;
    p.velocity.x = vehicle_f32(double(car.velocity.x) * velocity_scale);
    p.velocity.y = vehicle_f32(double(car.velocity.y) * velocity_scale);
    p.velocity.z = vehicle_f32(double(car.velocity.z) * velocity_scale);
    p.gravity_scale = vehicle_f32(double(gravity.inverse_factor) *
                                  std::bit_cast<float>(std::uint32_t{material == 2 ? 0xbf19999aU : 0xbe99999aU}));
    p.base_color = material == 2 ? 0x402000U : material == 3 ? 0x401000U : 0x101800U;
    p.lifetime = material == 2 ? 30U : 18U;
    p.field_68 = std::bit_cast<float>(std::uint32_t{material == 2   ? 0x3c23d70aU
                                                    : material == 3 ? 0x3dcccccdU
                                                                    : 0x3d4ccccdU});
    p.scale_fade_in = material == 2 ? 6.0f : 3.0f;
    p.scale_fade_out = material == 2 ? 6.0f : material == 3 ? 12.0f : 9.0f;
    if (car.speed < 10)
        p.base_scale = vehicle_f32(double(car.speed) *
                                   std::bit_cast<float>(std::uint32_t{material == 3 ? 0x3d99999aU : 0x3dcccccdU}));
    else
        p.base_scale = material == 4 ? 0.75f : 1.0f;
    p.alpha_fade_in = material == 2 ? 6.0f : 3.0f;
    p.alpha_fade_out = material == 2 ? 21.0f : 6.0f;
    p.base_alpha = 255;
    p.source_undefined.fill(true);
}
} // namespace
std::array<std::optional<unsigned>, 4> emit_surface_particles(const VehicleState &car, const solver::State &contact,
                                                              const VehiclePresentationState &presentation,
                                                              VehicleParticleState &state, const WorldGravity &gravity,
                                                              VehicleRandom &random, EffectParticleBank &bank,
                                                              const ModelTransform &template_pose,
                                                              EffectParticleFrameAudit *audit)
{
    std::array<std::optional<unsigned>, 4> result;
    if (car.speed < 2)
        return result;
    if (state.field_7620 != 0)
    {
        --state.field_7620;
        return result;
    }
    if (!std::isfinite(car.speed))
        throw FormatError("surface particle speed outside finite domain");
    std::int32_t last = 0;
    EffectParticle staging;
    Vec3 lift{};
    for (unsigned j = 0; j < 4; ++j)
    {
        const auto material = contact.wheels[j].type;
        if (material < 2 || material > 4 || car.wheels[j].contact_countdown <= 4)
            continue;
        if (material != last)
        {
            initialize(staging, material, car, gravity, template_pose);
            last = material;
            lift = {vehicle_f32(double(car.up.x) * 20), vehicle_f32(double(car.up.y) * 20),
                    vehicle_f32(double(car.up.z) * 20)};
        }
        const auto &center = presentation.wheel_centers[j];
        staging.position.x = vehicle_f32(double(center.x) - lift.x);
        staging.position.y = vehicle_f32(double(center.y) - lift.y);
        staging.position.z = vehicle_f32(double(center.z) - lift.z);
        const float amplitude = material == 2   ? std::bit_cast<float>(std::uint32_t{0x40f00001})
                                : material == 3 ? 1.0f
                                                : 2.5f;
        const float x = vehicle_f32(random.signed_value(amplitude));
        float y = 0;
        if (material == 2)
            y = vehicle_f32(std::abs(random.signed_value(2.5f)));
        const double z = random.signed_value(amplitude);
        staging.velocity.x = vehicle_f32(double(staging.velocity.x) + x);
        if (material == 2)
            staging.velocity.y = vehicle_f32(double(staging.velocity.y) + y);
        staging.velocity.z = vehicle_f32(double(staging.velocity.z) + z);
        result[j] = allocate_effect_particle(staging, bank, {}, audit);
    }
    if (last == 2 || last == 3)
        state.field_7620 = std::max(static_cast<std::uint32_t>(100.0 / car.speed), 4U);
    else if (last == 4)
        state.field_7620 = std::max(static_cast<std::uint32_t>(25.0 / car.speed), 2U);
    return result;
}
} // namespace sgn::retail::contact::effects
