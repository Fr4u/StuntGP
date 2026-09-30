#include "sgn/effect_particle_lifetime.hpp"
#include "sgn/vehicle.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail::contact::effects
{
namespace
{
std::uint32_t frames(std::span<const EffectAnimationFrames> animations, std::uint32_t identity)
{
    const EffectAnimationFrames *result = nullptr;
    for (const auto &entry : animations)
        if (entry.identity == identity)
        {
            if (result)
                throw FormatError("duplicate effect animation identity");
            result = &entry;
        }
    if (!result)
        throw FormatError("unresolved effect animation identity");
    return result->frame_count;
}
float envelope(std::uint32_t age, std::uint32_t lifetime, float base, float fade_in, float fade_out)
{
    if (double(age) < fade_in)
        return vehicle_f32((double(age) * base) / fade_in);
    if (double(age) > double(lifetime) - fade_out)
        return vehicle_f32((double(lifetime - age) * base) / fade_out);
    return base;
}
std::uint32_t integer(double value)
{
    // 00462860 converts to signed64 and returns its low32 bits.
    if (!std::isfinite(value) || value < -9223372036854775808.0 || value >= 9223372036854775808.0)
        throw FormatError("effect color conversion outside signed64 domain");
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
}
std::uint32_t blend(std::uint32_t first, std::uint32_t second, std::uint32_t amount)
{
    const auto r = (first >> 16) & 255u, g = (first >> 8) & 255u, b = first & 255u;
    const auto dr = (((second >> 16) & 255u) - r) * amount;
    const auto dg = (((second >> 8) & 255u) - g) * amount;
    const auto db = ((second & 255u) - b) * amount;
    // Literal422620 shifts, masks and ORs; no clamp or invented alpha blend.
    return (((dr & 0xffffff00u) + (r << 8)) << 8) | ((dg & 0xffffff00u) + (g << 8)) |
           (static_cast<std::uint32_t>(std::bit_cast<std::int32_t>(db) >> 8) + b);
}
std::uint32_t color(const EffectParticle &p)
{
    if (!(p.flags & 8))
        return p.base_color;
    if (double(p.age) < p.color_fade_in)
        return blend(p.base_color, p.secondary_color, integer((double(p.age) * 255) / p.color_fade_in));
    if (double(p.age) > double(p.lifetime) - p.color_fade_out)
        return blend(p.secondary_color, p.field_94,
                     integer(255 - (double(p.lifetime - p.age) * 255) / p.color_fade_out));
    return p.secondary_color;
}
void validate(const EffectParticle &p, std::span<const EffectAnimationFrames> animations)
{
    if ((p.flags & 4) && p.source_undefined[3])
        throw FormatError("undefined effect angular velocity");
    if (p.flags & 8)
    {
        if (p.source_undefined[1] || p.source_undefined[4])
            throw FormatError("undefined effect color input");
        if (!(double(p.age) < p.color_fade_in))
        {
            if (p.source_undefined[5])
                throw FormatError("undefined effect color fade out");
            if (double(p.age) > double(p.lifetime) - p.color_fade_out && p.source_undefined[2])
                throw FormatError("undefined effect final color");
        }
    }
    // The full conversion is checked here so failed input never half-updates a bank.
    (void)color(p);
    if (p.animation_binding)
        (void)frames(animations, p.animation_binding);
}
} // namespace
void update_effect_particles(EffectParticleBank &bank, Vec3 gravity, std::span<const EffectAnimationFrames> animations)
{
    if (bank.active_signal == 0)
        return;
    for (const auto &p : bank.particles)
        if (p.active == 1)
            validate(p, animations);
    bank.active_signal = 0;
    for (auto &p : bank.particles)
    {
        if (p.active != 1)
            continue;
        bank.active_signal = 1;
        p.position.x = vehicle_f32(double(p.velocity.x) + p.position.x);
        p.position.y = vehicle_f32(double(p.velocity.y) + p.position.y);
        p.position.z = vehicle_f32(double(p.velocity.z) + p.position.z);
        if (p.velocity.y < std::bit_cast<float>(std::uint32_t{0x4326aaab}))
        {
            Vec3 velocity{p.velocity.x, p.velocity.y, p.velocity.z};
            apply_gravity(velocity, gravity, p.gravity_scale);
            p.velocity.x = velocity.x;
            p.velocity.y = velocity.y;
            p.velocity.z = velocity.z;
        }
        p.alpha =
            (p.flags & 2) ? envelope(p.age, p.lifetime, p.base_alpha, p.alpha_fade_in, p.alpha_fade_out) : p.base_alpha;
        p.scale =
            (p.flags & 1) ? envelope(p.age, p.lifetime, p.base_scale, p.scale_fade_in, p.scale_fade_out) : p.base_scale;
        if (p.flags & 4)
        {
            p.angle = vehicle_f32(double(p.angular_velocity) + p.angle);
            if (p.angle >= 360)
                p.angle = vehicle_f32(double(p.angle) - 360);
            else if (p.angle < 0)
                p.angle = vehicle_f32(double(p.angle) + 360);
        }
        p.color = color(p);
        p.source_undefined[6] = false;
        if (p.lifetime_mode == 2 && ++p.age >= p.lifetime)
            p.active = 2;
        if (p.animation_binding)
        {
            const auto old = std::bit_cast<std::int32_t>(p.animation_frame) / 4096;
            p.animation_frame += 0x6aa;
            if (static_cast<std::uint32_t>(old) >= frames(animations, p.animation_binding) - 1)
            {
                p.animation_frame = 0;
                if (p.lifetime_mode == 1)
                    p.active = 2;
            }
        }
    }
}
} // namespace sgn::retail::contact::effects
