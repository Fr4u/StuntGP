#include "sgn/score_popup_lifetime.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
using Popup = VehicleScorePopup;
static_assert(sizeof(Popup) == 0x138);
static_assert(offsetof(Popup, offset) == 0x14 && offsetof(Popup, age) == 0x28);
static_assert(offsetof(Popup, reserved_50) == 0x50 && offsetof(Popup, reserved_60) == 0x60);
static_assert(offsetof(Popup, motion_flags) == 0x70 && offsetof(Popup, overlay_age) == 0x9c);
static_assert(offsetof(Popup, position) == 0xb0 && offsetof(Popup, presentation_x) == 0xdc);
static_assert(offsetof(Popup, presentation_scale) == 0xec && offsetof(Popup, reserved_f4) == 0xf4);
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_word;
std::uint32_t integer(double value)
{ // full462860 low32, including x87 indefinite
    if (!std::isfinite(value) || value < -9223372036854775808.0 || value >= 9223372036854775808.0)
        return 0;
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
}
float envelope(std::uint32_t age, std::uint32_t lifetime, float base, float fade_in, float fade_out)
{
    if (!(double(age) >= fade_in))
        return vehicle_f32((double(age) * base) / fade_in);
    if (double(age) > double(lifetime) - fade_out)
        return vehicle_f32((double(lifetime - age) * base) / fade_out);
    return base;
}
std::uint32_t blend(std::uint32_t first, std::uint32_t second, std::uint32_t amount)
{ // 422620
    const auto r = (first >> 16) & 255u, g = (first >> 8) & 255u, b = first & 255u;
    const auto dr = (((second >> 16) & 255u) - r) * amount, dg = (((second >> 8) & 255u) - g) * amount,
               db = ((second & 255u) - b) * amount;
    return (((dr & 0xffffff00u) + (r << 8)) << 8) | ((dg & 0xffffff00u) + (g << 8)) |
           (static_cast<std::uint32_t>(std::bit_cast<std::int32_t>(db) >> 8) + b);
}
} // namespace
void update_score_audio_delay(VehicleScoreState &state, AudioDriver &audio)
{
    if (state.sound_class != 0 && --state.sound_class == 0)
        audio_start(audio, state.sound_id, 0, 0, state.sound_volume, -255, 0, nullptr);
}
void spawn_score_sparkle(VehicleScoreState &state, VehicleRandom &random, float x, float y)
{
    for (auto &p : state.popups)
    {
        if (p.active == 1)
            continue;
        p.texture = 2;
        p.dimensions = {256, 256};
        p.flags = 0x18;
        p.lifetime = 30;
        p.growth = -0.5f;
        p.offset[0] = vehicle_f32(random.signed_value(15));
        p.offset[1] = vehicle_f32(random.signed_value(7.5f) - 7.5);
        p.motion_flags = 0;
        p.motion = {8, 0, 15};
        p.reserved_50 = 0;
        p.scale = 0.1f;
        p.angle = 6;
        p.depth = 15;
        p.color_animation = {255, 9, 15};
        p.alignment = 0xffffffffu;
        p.active = 1;
        p.variant = 0;
        p.age = 0;
        p.enabled = 0;
        p.overlay_enabled = 0;
        p.points = 0;
        p.presentation_lifetime = 88;
        p.presentation_flags = p.flags;
        p.presentation_x = 0;
        p.presentation_y = 0;
        p.presentation_scale = {1, 1};
        p.presentation_size = {vehicle_f32(double(p.dimensions[0]) * 0.5), vehicle_f32(double(p.dimensions[1]) * 0.5)};
        p.position = {vehicle_f32(320 - double(p.presentation_size[0])),
                      vehicle_f32(240 - double(p.presentation_size[1]))};
        p.reserved_b8 = as_word(1);
        p.position[0] = vehicle_f32(double(x) - p.presentation_size[0]);
        p.position[1] = vehicle_f32(double(y) - p.presentation_size[1]);
        return;
    }
}
void update_score_popups(VehicleScoreState &state, VehicleRandom &random, Vec3 gravity, AudioDriver &audio)
{
    update_score_audio_delay(state, audio);
    for (auto &p : state.popups)
    {
        if (p.active != 1)
            continue;
        p.position[0] = vehicle_f32(double(p.position[0]) + p.offset[0]);
        p.position[1] = vehicle_f32(double(p.position[1]) + p.offset[1]);
        if (!(p.offset[1] >= as_float(0x4326aaabu)))
        {
            Vec3 velocity{p.offset[0], p.offset[1], p.offset[2]};
            apply_gravity(velocity, gravity, p.growth);
            p.offset = {velocity.x, velocity.y, velocity.z};
        }
        p.reserved_60 = as_word((p.flags & 0x10) ? envelope(p.age, p.lifetime, p.color_animation[0],
                                                            p.color_animation[1], p.color_animation[2])
                                                 : p.color_animation[0]);
        p.reserved_50 = as_word((p.flags & 8) ? envelope(p.age, p.lifetime, p.scale, p.angle, p.depth) : p.scale);
        if (p.flags & 1)
        {
            double speed;
            if (!(double(p.age) >= p.motion[1]))
                speed = ((double(p.motion[1]) - p.age) * p.motion[0]) / p.motion[1];
            else if (double(p.age) > double(p.lifetime) - p.motion[2])
                speed = ((double(p.age) - (double(p.lifetime) - p.motion[2])) * p.motion[0]) / p.motion[2];
            else
            {
                p.motion_flags = 0;
                speed = 0;
            }
            float angle = vehicle_f32(speed + as_float(p.motion_flags));
            p.motion_flags = as_word(angle);
            if (angle >= 360)
                p.motion_flags = as_word(vehicle_f32(double(angle) - 360));
            else if (!(angle >= 0))
                p.motion_flags = as_word(vehicle_f32(double(angle) + 360));
        }
        if (p.flags & 0x20)
        {
            if (!(double(p.age) >= p.text_size[0]))
            {
                const std::uint32_t product = (p.age << 8) - p.age;
                p.reserved_f4[0] = blend(p.colors[0], p.colors[1], integer(double(product) / p.text_size[0]));
            }
            else if (double(p.age) > double(p.lifetime) - p.text_size[1])
            {
                const std::uint32_t product = ((p.lifetime << 8) - p.lifetime) - ((p.age << 8) - p.age);
                p.reserved_f4[0] = blend(p.colors[1], p.colors[2], integer(255 - double(product) / p.text_size[1]));
            }
            else
                p.reserved_f4[0] = p.colors[1];
        }
        else
            p.reserved_f4[0] = p.colors[0];
        if (p.age == p.alignment)
            audio_start(audio, p.font, 0, p.reserved_94, p.style, 0, 1, nullptr);
        if (p.overlay_enabled == 1 && double(p.age) >= p.overlay_size[1] &&
            double(p.age) < double(p.lifetime) - p.overlay_size[2])
        {
            // 401A54 retains the x87 result through the comparison after FST.
            const double age = double(p.overlay_size[0]) + as_float(p.overlay_age);
            p.overlay_age = as_word(vehicle_f32(age));
            if (age > 60)
            {
                p.overlay_age = 0;
                const float x = vehicle_f32(double(p.presentation_size[0]) + p.position[0]);
                const float y = p.position[1];
                spawn_score_sparkle(state, random, x, y);
            }
        }
        if (++p.age >= p.lifetime)
            p.active = 0;
        p.presentation_x = integer(as_float(p.reserved_60));
        // 401AB4..401AC9 tests x87 C3 only: unordered also selects0.1.
        if (as_float(p.reserved_50) == 0 || std::isnan(as_float(p.reserved_50)))
            p.reserved_50 = as_word(0.1f);
        p.presentation_scale = {as_float(p.reserved_50), as_float(p.reserved_50)};
        p.presentation_y = p.motion_flags;
    }
}
} // namespace sgn::retail
