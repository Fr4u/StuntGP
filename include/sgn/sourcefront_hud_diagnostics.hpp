#pragma once
// NEW, read-only host diagnostics. Never calls source update/preparation code.
#include "sgn/retail_race_hud.hpp"
#include "sgn/retail_viewport.hpp"
#include <bit>
#include <optional>
#include <ostream>

namespace sgn::diagnostics
{
struct HudControllerObservation
{
    std::int32_t active{};
    std::uint32_t vehicle{};
    bool vehicle_valid{};
    std::int32_t lap_timer{}, race_timer{}, lap_index{}, race_index{}, energy_exempt{}, special_mode{};
    std::array<std::uint32_t, 3> auxiliary_bits{};
    std::uint32_t respawn_active{}, respawn_phase{};
    retail::ControllerHudCountdown countdown{};
    retail::RaceHudLayout layout{};
};
struct HudObservation
{
    std::uint64_t source_tick{};
    std::uint32_t frame_pause{}, game_mode{}, split_setting{};
    unsigned current_controller{}, selected_hud{};
    retail::RaceHudMargins margins{};
    retail::ViewportState viewport{};
    std::array<HudControllerObservation, 4> controllers{};
};
inline HudObservation observe_hud(std::uint64_t source_tick, std::uint32_t frame_pause, std::uint32_t game_mode,
                                  std::uint32_t split_setting, const retail::contact::player::Controls &controls,
                                  std::span<const retail::ControllerHudCountdown, 4> countdown,
                                  std::span<const retail::RaceHudLayout, 4> layouts, unsigned selected_hud,
                                  retail::RaceHudMargins margins, std::span<const retail::VehicleState, 6> physics,
                                  std::span<const retail::VehicleRaceState, 6> race,
                                  const retail::ViewportState &viewport) noexcept
{
    HudObservation result;
    result.source_tick = source_tick;
    result.frame_pause = frame_pause;
    result.game_mode = game_mode;
    result.split_setting = split_setting;
    result.current_controller = controls.current;
    result.selected_hud = selected_hud;
    result.margins = margins;
    result.viewport = viewport;
    for (unsigned i = 0; i < 4; ++i)
    {
        const auto &c = controls.controllers[i];
        auto &d = result.controllers[i];
        d.active = c.active;
        d.vehicle = c.vehicle;
        d.countdown = countdown[i];
        d.layout = layouts[i];
        d.respawn_active = c.respawn_transition.state;
        d.respawn_phase = c.respawn_transition.phase;
        d.vehicle_valid = c.vehicle < physics.size();
        if (d.vehicle_valid)
        {
            const auto &p = physics[c.vehicle];
            const auto &r = race[c.vehicle];
            d.lap_timer = r.lap_timer;
            d.race_timer = r.race_timer;
            d.lap_index = r.lap_index;
            d.race_index = r.race_index;
            d.energy_exempt = p.energy_exempt;
            d.special_mode = p.special_mode;
            d.auxiliary_bits = {std::bit_cast<std::uint32_t>(p.auxiliary_displacement.x),
                                std::bit_cast<std::uint32_t>(p.auxiliary_displacement.y),
                                std::bit_cast<std::uint32_t>(p.auxiliary_displacement.z)};
        }
    }
    return result;
}
struct HudCaptureObservation
{
    std::uint64_t accepted_outer_serial{}, submission_source_tick{};
    HudObservation before, after;
};
struct HudTrace
{
    std::array<HudCaptureObservation, 2> last{};
    unsigned count{};
    void record(std::uint64_t serial, std::uint64_t source_tick, const HudObservation &before,
                const HudObservation &after) noexcept
    {
        if (count == 2)
            last[0] = last[1];
        else
            ++count;
        last[count - 1] = {serial, source_tick, before, after};
    }
};
namespace detail
{
inline std::uint32_t bits(float value) noexcept
{
    return std::bit_cast<std::uint32_t>(value);
}
template <class T, std::size_t N> void integers(std::ostream &out, const std::array<T, N> &values)
{
    out << '[';
    for (std::size_t i = 0; i < N; ++i)
    {
        if (i)
            out << ',';
        out << values[i];
    }
    out << ']';
}
template <std::size_t N> void float_bits(std::ostream &out, const std::array<float, N> &values)
{
    out << '[';
    for (std::size_t i = 0; i < N; ++i)
    {
        if (i)
            out << ',';
        out << bits(values[i]);
    }
    out << ']';
}
inline void snapshot(std::ostream &out, const HudObservation &s)
{
    const auto &v = s.viewport;
    out << "{\"source_tick\":" << s.source_tick << ",\"frame_pause\":" << s.frame_pause
        << ",\"game_mode\":" << s.game_mode << ",\"split_setting\":" << s.split_setting
        << ",\"current_controller\":" << s.current_controller << ",\"selected_hud\":" << s.selected_hud
        << ",\"margins\":[" << s.margins.x << ',' << s.margins.y << "],\"viewport\":{\"width\":" << v.width
        << ",\"height\":" << v.height << ",\"full_width\":" << v.full_width << ",\"full_height\":" << v.full_height
        << ",\"selector\":" << v.selector << ",\"applied_selector\":" << v.applied_selector
        << ",\"requested_selector\":" << v.requested_selector << ",\"cropped\":" << v.cropped << ",\"origin_bits\":["
        << bits(v.origin_x) << ',' << bits(v.origin_y) << "],\"center_bits\":[" << bits(v.center_x) << ','
        << bits(v.center_y) << "],\"scale_bits\":[" << bits(v.scale_x) << ',' << bits(v.scale_y)
        << "],\"ui_scale_bits\":[" << bits(v.ui_scale_x) << ',' << bits(v.ui_scale_y) << "],\"clip_virtual_bits\":";
    float_bits(out, v.clip_virtual);
    out << ",\"clip_screen_bits\":";
    float_bits(out, v.clip_screen);
    out << "},\"controllers\":[";
    for (unsigned i = 0; i < 4; ++i)
    {
        if (i)
            out << ',';
        const auto &c = s.controllers[i];
        out << "{\"slot\":" << i << ",\"active\":" << c.active << ",\"vehicle\":" << c.vehicle
            << ",\"vehicle_valid\":" << (c.vehicle_valid ? "true" : "false") << ",\"lap_timer\":" << c.lap_timer
            << ",\"race_timer\":" << c.race_timer << ",\"lap_index\":" << c.lap_index
            << ",\"race_index\":" << c.race_index << ",\"energy_exempt\":" << c.energy_exempt
            << ",\"special_mode\":" << c.special_mode << ",\"auxiliary_bits\":";
        integers(out, c.auxiliary_bits);
        out << ",\"respawn_active\":" << c.respawn_active << ",\"respawn_phase_bits\":" << c.respawn_phase
            << ",\"countdown\":{\"active\":" << c.countdown.active << ",\"ticks\":" << c.countdown.ticks
            << ",\"enabled\":" << c.countdown.enabled << ",\"value_bits\":" << bits(c.countdown.value)
            << ",\"phase_bits\":" << c.countdown.phase << "},\"layout\":{\"value_bits\":";
        float_bits(out, c.layout.value);
        out << ",\"active\":";
        integers(out, c.layout.active);
        out << ",\"phase_bits\":";
        integers(out, c.layout.phase);
        out << ",\"elements\":[";
        for (unsigned j = 0; j < c.layout.elements.size(); ++j)
        {
            if (j)
                out << ',';
            const auto &e = c.layout.elements[j];
            out << '[' << bits(e.x) << ',' << bits(e.y) << ',' << e.word_14 << ',' << e.word_18 << ',' << e.word_1c
                << ',' << e.word_20 << ']';
        }
        out << "]}}";
    }
    out << "]}";
}
} // namespace detail
inline void write_hud_trace(std::ostream &out, const HudTrace &trace,
                            std::optional<std::uint64_t> displayed_source_tick)
{
    unsigned matches{};
    for (unsigned i = 0; i < trace.count; ++i)
        if (displayed_source_tick && trace.last[i].submission_source_tick == *displayed_source_tick)
            ++matches;
    // clang-format off: keep the JSON key literals unsplit
    out<<"{\"scope\":\"read_only_actual_source_capture_state\",\"float_encoding\":\"raw_binary32_uint\","
        <<"\"layout_element_order\":\"base_x_bits,base_y_bits,signed_offset_x_word,signed_offset_y_word,computed_x_bits,computed_y_bits\","
        <<"\"displayed_source_tick\":";
    // clang-format on
    if (displayed_source_tick)
        out << *displayed_source_tick;
    else
        out << "null";
    out << ",\"displayed_matching_records\":" << matches;
    out << ",\"last_captures\":[";
    for (unsigned i = 0; i < trace.count; ++i)
    {
        if (i)
            out << ',';
        const auto &r = trace.last[i];
        out << "{\"accepted_outer_serial\":" << r.accepted_outer_serial
            << ",\"submission_source_tick\":" << r.submission_source_tick << ",\"before\":";
        detail::snapshot(out, r.before);
        out << ",\"after\":";
        detail::snapshot(out, r.after);
        out << '}';
    }
    out << "]}";
}
} // namespace sgn::diagnostics
