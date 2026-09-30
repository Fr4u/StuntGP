#include "sgn/vehicle_score.hpp"
#include <algorithm>
#include <charconv>

namespace sgn::retail
{
void clear_score_popups(VehicleScoreState &state) noexcept
{
    state.popups = {};
}
void award_stunt_score(VehicleState &s, const contact::player::Controls &controls, std::uint32_t vehicle_id,
                       VehicleScoreState &state, const VehicleStuntContext &context, std::uint32_t category,
                       std::uint32_t variant, std::uint32_t points)
{
    if (context.game_mode != 2 || s.energy_exempt == 1 || context.race_mode == 4 ||
        (context.race_mode == 3 && context.score_block != 0))
        return;
    if (category > 5 || variant > 3)
        throw std::domain_error("score style outside recovered caller domain");
    s.score = vehicle_f32(double(points) + s.score);
    if (!contact::player::find_controller(controls, vehicle_id) || context.hud_block == 1)
        return;
    const auto found =
        std::find_if(state.popups.begin(), state.popups.end(), [](const auto &popup) { return popup.active != 1; });
    if (found == state.popups.end())
        return;
    auto &p = *found;
    p.offset = {0, 0, 0};
    p.points = points;
    // The retail field is eight bytes. Signed %d may temporarily spill into
    // following fields; every spilled byte is overwritten by valid variants.
    std::array<char, 12> formatted{};
    const auto result = std::to_chars(formatted.data(), formatted.data() + 11, std::bit_cast<std::int32_t>(points));
    if (result.ec != std::errc{})
        throw std::runtime_error("score formatting failed");
    const auto bytes =
        std::min<std::size_t>(p.text.size(), static_cast<std::size_t>(result.ptr - formatted.data()) + 1);
    std::copy_n(formatted.begin(), bytes, p.text.begin());
    state.sound_class = 6;
    state.sound_id = category <= 2 ? 47u : 46u;
    state.sound_volume = 192;
    context.audio.play(state.sound_id, state.sound_volume);
    // Descriptor table 00472030: exact six retail entries, texture ID 13.
    p.texture = 13;
    if (category <= 2)
    {
        p.dimensions = {128, 100};
        p.texture_rect = {4, 4, 512, 400};
    }
    else
    {
        p.dimensions = {213, 151};
        p.texture_rect = {4, 416, 852, 1020};
    }
    p.enabled = 1;
    p.motion_flags = 0;
    p.scale = 2;
    p.depth = 0;
    p.color_animation = {255, 15, 15};
    p.alignment = 9;
    p.reserved_94 = 0;
    p.overlay_enabled = 0;
    if (variant <= 1)
    {
        p.flags = variant == 0 ? 24u : 25u;
        p.lifetime = 60;
        p.offset[1] = -5;
        p.growth = -0.025f;
        p.motion = {variant == 0 ? 8.0f : -8.0f, 0, 15};
        p.angle = 60;
        p.font = 15;
        p.style = variant == 0 ? 63u : 85u;
    }
    else
    {
        p.flags = 57;
        p.lifetime = 120;
        p.growth = 0.025f;
        p.motion = {-11.75f, 60, 0};
        p.angle = 30;
        p.color_animation[1] = 0;
        p.font = 16;
        p.style = variant == 2 ? 63u : 127u;
        if (variant == 3)
        {
            p.text_size = {30, 15};
            p.colors = {0, 0xffffff, 0xc0c0c0};
            p.overlay_enabled = 1;
            p.overlay_age = 0;
            p.overlay_size = {30, 15, 15};
        }
    }
    p.flags |= 32;
    p.text_size = {6, 6};
    const auto color = category % 3 == 0 ? 0xffa000u : (category % 3 == 1 ? 0xe0f0ffu : 0xffdc00u);
    p.colors = {color, color, color};
    p.active = 1;
    p.vehicle = vehicle_id;
    p.variant = variant;
    p.category = category;
    p.age = 0;
    p.presentation_lifetime = 72;
    p.presentation_flags = p.flags;
    p.presentation_x = 0;
    p.presentation_y = 0;
    p.presentation_scale = {1, 1};
    const float half_width = vehicle_f32(double(p.dimensions[0]) * 0.5f);
    const double half_height = double(p.dimensions[1]) * 0.5f;
    const double width = category <= 2 ? p.dimensions[0] : half_width;
    p.presentation_size = {vehicle_f32(width), vehicle_f32(half_height)};
    p.position = {vehicle_f32(320 - width), vehicle_f32(240 - half_height)};
}
void update_inversion_state(VehicleState &s, contact::player::Controls &controls, std::uint32_t vehicle_id,
                            VehicleScoreState &state, const VehicleStuntContext &context, Vec3 gravity)
{
    const double facing = (double(gravity.z) * s.up.z + double(gravity.y) * s.up.y) + double(gravity.x) * s.up.x;
    if (s.contact_class == 0 && facing >= 0.9f && s.stunt.inversion_latch == 0)
    {
        s.stunt.inversion_latch = 1;
        if (contact::player::find_controller(controls, vehicle_id))
        {
            if (s.stunt.inversion_timer == 0)
            {
                award_stunt_score(s, controls, vehicle_id, state, context, 5, 0, context.inversion_points);
                contact::player::post_notice(controls, context.game_mode, context.tick, 180, context.inversion_text,
                                             vehicle_id, 2, 100);
            }
            s.stunt.inversion_timer = 300;
        }
    }
    if (s.stunt.inversion_timer != 0)
        --s.stunt.inversion_timer;
}
void award_stunt_height(VehicleState &s, const contact::player::Controls &controls, std::uint32_t vehicle_id,
                        VehicleScoreState &state, const VehicleStuntContext &context,
                        std::span<const std::uint32_t, 33> values, float height)
{
    s.stunt.height_level = 1;
    if (height < 250)
        return;
    // 00474354..004743A3. The final source row is a 50000 threshold with
    // score index/category zero. Preserve it; do not invent a maximum tier.
    constexpr std::array<float, 4> thresholds = {400, 800, 1250, 50000};
    constexpr std::array<unsigned, 5> indexes = {28, 29, 30, 31, 0};
    constexpr std::array<unsigned, 5> categories = {0, 1, 2, 2, 0};
    unsigned tier = 0;
    while (tier < thresholds.size() && height > thresholds[tier])
        ++tier;
    award_stunt_score(s, controls, vehicle_id, state, context, categories[tier], 0, values[indexes[tier]]);
}
void update_stunt_height(VehicleState &s, const contact::player::Controls &controls, std::uint32_t vehicle_id,
                         VehicleScoreState &state, const VehicleStuntContext &context,
                         std::span<const std::uint32_t, 33> values)
{
    if (context.race_mode == 4)
        return;
    if (s.stunt.maximum_height < s.position.y)
    {
        s.stunt.maximum_height = s.position.y;
        return;
    }
    if (s.stunt.height_level == 0 && s.position.y < double(s.stunt.maximum_height) - 2)
        award_stunt_height(s, controls, vehicle_id, state, context, values,
                           vehicle_f32(double(s.stunt.maximum_height) - s.stunt.launch_height));
}
} // namespace sgn::retail
