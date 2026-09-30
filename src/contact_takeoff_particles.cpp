#include "sgn/contact_takeoff_particles.hpp"
#include "sgn/retail_effect_template.hpp"
#include "sgn/vehicle.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail::contact::effects
{
namespace
{
float f32(double value)
{
    return static_cast<float>(value);
}
using sgn::bit_reinterpret::as_float;
EffectParticle source_staging(const ModelTransform &template_pose)
{
    EffectParticle p;
    copy_effect_template(p, template_pose); // 4171DB: live 80-byte source copy
    // Full 00417130 writes. Loop later replaces base_scale.
    p.flags = 3;
    p.gravity_scale = 0.25f;
    p.base_scale = 0.25f;
    p.render_flags = 0x60;
    p.lifetime_mode = 2;
    p.texture = 0;
    p.lifetime = 30;
    p.field_68 = as_float(0x3c23d70a);
    p.render_enabled = 1;
    p.animation_binding = 0;
    p.scale_fade_in = 6;
    p.scale_fade_out = 12;
    p.base_alpha = 192;
    p.alpha_fade_in = as_float(0x3f199999);
    p.alpha_fade_out = 12;
    p.base_color = 0x182010;
    p.source_undefined.fill(true);
    return p;
}
void allocate(EffectParticle &staging, EffectParticleBank &bank)
{
    // 0045E110 resets these staging words even when no slot can be allocated.
    staging.scale = 0;
    staging.alpha = 0;
    staging.animation_frame = 0;
    for (auto &slot : bank.particles)
    {
        if (slot.active == 1)
            continue;
        const auto old = slot;
        slot = staging;
        slot.active = 1;
        slot.age = 0;
        slot.angle = 0;
        // Explicit native opaque-word policy for the seven unwritten source
        // stack slots, with definedness kept separately for comparison/readers.
        slot.velocity.opaque = old.velocity.opaque;
        slot.secondary_color = old.secondary_color;
        slot.field_94 = old.field_94;
        slot.angular_velocity = old.angular_velocity;
        slot.color_fade_in = old.color_fade_in;
        slot.color_fade_out = old.color_fade_out;
        slot.color = old.color;
        bank.active_signal = 1;
        return;
    }
}
} // namespace
void emit_takeoff_particles(const VehicleState &vehicle, VehicleRandom &random, EffectParticleBank &bank,
                            const ModelTransform &template_pose)
{
    const auto finite = [](float a, float b, float c) {
        return std::isfinite(a) && std::isfinite(b) && std::isfinite(c);
    };
    if (!finite(vehicle.position.x, vehicle.position.y, vehicle.position.z) ||
        !finite(vehicle.velocity.x, vehicle.velocity.y, vehicle.velocity.z) ||
        !finite(vehicle.up.x, vehicle.up.y, vehicle.up.z))
        throw FormatError("takeoff particle source vehicle must be finite");
    auto staging = source_staging(template_pose);
    const Vec3 base_velocity{f32(double(vehicle.velocity.x) * 0.5), f32(double(vehicle.velocity.y) * 0.5),
                             f32(double(vehicle.velocity.z) * 0.5)};
    const float scale = as_float(0x42700001);
    const Vec3 lift{f32(double(vehicle.up.x) * scale), f32(double(vehicle.up.y) * scale),
                    f32(double(vehicle.up.z) * scale)};
    for (unsigned i = 0; i < 32; ++i)
    {
        staging.position.x = f32(double(vehicle.position.x) - lift.x);
        staging.position.y = f32(double(vehicle.position.y) - lift.y);
        staging.position.z = f32(double(vehicle.position.z) - lift.z);
        const float vx = f32(random.signed_value(5)), vy = f32(std::abs(random.signed_value(1)));
        const double vz = random.signed_value(5);
        staging.velocity.x = f32(double(vx) + base_velocity.x);
        staging.velocity.y = f32(double(vy) + base_velocity.y);
        staging.velocity.z = f32(vz + base_velocity.z);
        staging.base_scale = f32(std::abs(random.signed_value(0.25f)) + 0.5);
        const float x = f32(random.signed_value(150)), y = f32(std::abs(random.signed_value(10)));
        const double z = random.signed_value(150);
        staging.position.x = f32(double(staging.position.x) + x);
        staging.position.y = f32(double(staging.position.y) + y);
        staging.position.z = f32(double(staging.position.z) + z);
        allocate(staging, bank);
    }
}
} // namespace sgn::retail::contact::effects
