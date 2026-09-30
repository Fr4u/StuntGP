#include "sgn/retail_race_hud_submission.hpp"
#include "sgn/retail_quad_submission.hpp"
#include "sgn/retail_hud_fraction.hpp"
#include "sgn/retail_font_selection.hpp"
#include "sgn/race_session_lifecycle.hpp"
#include "retail_race_hud_tables.hpp"
#include "sgn/host_wide_race_anchors.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/format_require.hpp"
#include <algorithm>
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_float;
std::uint32_t word(double v)
{
    return std::bit_cast<std::uint32_t>(static_cast<float>(v));
}
using sgn::bit_reinterpret::as_signed;
double k(unsigned address)
{
    return hud_source::scalar(address);
}
using sgn::format_check::require;
std::uint32_t ftol(double value)
{
    // 462860 FISTP qword returns integer-indefinite on invalid conversion;
    // all HUD callers consume its low EAX word, which is zero in that case.
    if (!std::isfinite(value) || value < -9223372036854775808.0 || value >= 9223372036854775808.0)
        return 0;
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
}
double uv(std::uint32_t v)
{
    return (double(as_signed(v)) * k(0x46e75c)) * k(0x46e6e4);
}
unsigned digits_448150(std::uint32_t value, std::span<std::uint32_t> out, bool forced)
{
    require(out.size() == 3 || out.size() == 7, "HUD decimal output extent");
    std::uint32_t divisor = 1, seen = forced ? 1u : 0u;
    unsigned leading = 0;
    for (std::size_t i = 1; i < out.size(); ++i)
        divisor *= 10;
    for (std::size_t i = 0; i < out.size(); ++i)
    {
        const auto digit = value / divisor;
        seen |= digit;
        out[i] = digit;
        if (seen == 0 && i + 1 != out.size())
        {
            out[i] = 0xffffffffu;
            ++leading;
        }
        value -= digit * divisor;
        divisor /= 10;
    }
    return leading;
}
std::array<std::uint32_t, 8> digits_4482a0(std::uint32_t ticks)
{
    const auto seconds = ticks / 60u, hours = seconds / 3600u, within_hour = seconds % 3600u,
               minutes = within_hour / 60u, within_minute = within_hour % 60u;
    return {((ticks * 10u) / 6u) % 10u,
            (ticks / 6u) % 10u,
            within_minute % 10u,
            within_minute / 10u,
            minutes % 10u,
            minutes / 10u,
            hours % 10u,
            hours / 10u};
}
// NEW HOST (wide_hud R20): view edge of each finite leaf group, from the
// 4478C0 base coordinates cited in host_wide_race_anchors.hpp.
host_wide_overlay::Anchor leaf_anchor(RaceHudFiniteLeaf leaf)
{
    namespace a = host_wide_overlay::race;
    switch (leaf)
    {
    case RaceHudFiniteLeaf::position:
        return a::position;
    case RaceHudFiniteLeaf::timers:
        return a::timers;
    case RaceHudFiniteLeaf::laps:
        return a::timers;
    case RaceHudFiniteLeaf::gauge:
        return a::gauge;
    case RaceHudFiniteLeaf::score:
        return a::score;
    case RaceHudFiniteLeaf::progress:
        return a::progress;
    case RaceHudFiniteLeaf::gear:
        return a::gear;
    case RaceHudFiniteLeaf::split_gauge:
        return a::gauge;
    case RaceHudFiniteLeaf::remaining:
        return a::position;
    }
    return host_wide_overlay::Anchor::original(); // unknown leaf: run_leaf throws before drawing
}
using AnchorScope = host_wide_overlay::AnchorScope;
class Hud
{
    RaceHudLeafInputs f;
    AudioDriver *audio_{};
    const RaceTextureReplacement *overlay_{};
    RaceHudSourceEnergy *source_energy_{};
    PolygonSubmissionSink &sink;
    std::span<std::uint32_t, 35> d;
    unsigned vehicle(unsigned controller) const
    {
        require(controller < 4, "HUD controller identity outside four owners");
        const auto id = f.hud.controls.controllers[controller].vehicle;
        require(id < 6, "HUD vehicle identity outside six owners");
        return id;
    }
    unsigned current() const
    {
        return vehicle(f.hud.controls.current);
    }
    const VehicleState &physics() const
    {
        return f.vehicles.physics[current()];
    }
    const VehicleRaceState &race() const
    {
        return f.vehicles.race[current()];
    }
    std::uint32_t &h(unsigned offset)
    {
        require(f.hud.selected_hud < 4 && offset >= 0x2c, "HUD computed field identity");
        const auto relative = offset - 0x2c, index = relative / 0x24, field = relative % 0x24;
        require(index < 18 && (field == 0 || field == 4), "HUD computed field outside canonical words");
        auto &e = f.hud.layouts[f.hud.selected_hud].elements[index];
        return field ? e.word_20 : e.word_1c;
    }
    double hr(unsigned offset)
    {
        return as_float(h(offset));
    }
    void put(unsigned offset, std::uint32_t value)
    {
        require(offset % 4 == 0 && offset < 140, "HUD descriptor offset");
        d[offset / 4] = value;
    }
    void fp(unsigned offset, double value)
    {
        put(offset, word(value));
    }
    double dr(unsigned offset) const
    {
        return as_float(d[offset / 4]);
    }
    void set(std::initializer_list<std::pair<unsigned, std::uint32_t>> values)
    {
        for (const auto &[offset, value] : values)
            put(offset, value);
    }
    void quad()
    {
        auto &r = f.rendering;
        submit_quad_432e00({d, f.vertices, r.selected_material, r.materials, r.viewport}, sink);
    }
    std::uint32_t startup(unsigned slot, unsigned offset) const
    {
        require(slot < 6 && offset >= 0x768c && offset <= 0x76ec && offset % 4 == 0, "HUD startup field access");
        return f.vehicles.startup[slot].fields_768c[(offset - 0x768c) / 4];
    }
    std::uint32_t overlay() const
    {
        require(overlay_ != nullptr, "HUD finite leaf reached an unbound overlay child");
        return overlay_->selected_overlay_texture();
    }
    void select_font(unsigned requested)
    {
        select_retail_font_428f70(f.rendering.fonts, requested);
    }
    std::span<const std::uint8_t> text(unsigned id) const
    {
        require(id < f.game_text.offsets.size() && f.game_text.offsets[id].has_value(), "HUD text pointer absent");
        const auto offset = *f.game_text.offsets[id];
        require(offset < f.game_text.arena.size(), "HUD text outside source arena");
        return {reinterpret_cast<const std::uint8_t *>(f.game_text.arena.data() + offset),
                f.game_text.arena.size() - offset};
    }
    void text_call(std::span<const std::uint8_t> bytes, std::uint32_t alignment, TextGlyphParameters p)
    {
        submit_text_4291f0(f.rendering, bytes, alignment, p, sink);
    }
    void animated_score_448ab0();
    void timer_448e00(const std::array<std::uint32_t, 8> &, std::uint32_t, float, float, std::uint32_t, std::uint32_t);
    void timers_4490e0();
    void laps_44a4a0(std::uint32_t, std::uint32_t, float, float);
    void lap_display_44a5c0();
    void progress_44a800();
    void remaining_44aad0();

  public:
    Hud(RaceHudSubmission frame, PolygonSubmissionSink &target)
        : f{frame.hud,        frame.vehicles,  frame.clock,         frame.records,     frame.ranking,
            frame.cars,       frame.game_text, frame.split_setting, frame.frame_pause, frame.stars,
            frame.descriptor, frame.vertices,  frame.rendering},
          audio_(&frame.audio), overlay_(&frame.overlay_textures), sink(target), d(frame.descriptor)
    {
    }
    Hud(RaceHudLeafInputs frame, PolygonSubmissionSink &target) : f(frame), sink(target), d(frame.descriptor) {}
    Hud(RaceHudLeafInputs frame, RaceHudSourceEnergy &energy, PolygonSubmissionSink &target)
        : f(frame), source_energy_(&energy), sink(target), d(frame.descriptor)
    {
    }
    void run_leaf(RaceHudFiniteLeaf leaf)
    {
        AnchorScope anchor(leaf_anchor(leaf)); // NEW HOST (wide_hud R20)
        switch (leaf)
        {
        case RaceHudFiniteLeaf::position:
            position_448a70();
            return;
        case RaceHudFiniteLeaf::timers:
            timers_4490e0();
            return;
        case RaceHudFiniteLeaf::laps:
            lap_display_44a5c0();
            return;
        case RaceHudFiniteLeaf::gauge:
            gauge_44add0();
            return;
        case RaceHudFiniteLeaf::score:
            score_44adc0();
            return;
        case RaceHudFiniteLeaf::progress:
            progress_44a800();
            return;
        case RaceHudFiniteLeaf::gear:
            gear_4485e0();
            return;
        case RaceHudFiniteLeaf::split_gauge:
            gauge_44ae10();
            return;
        case RaceHudFiniteLeaf::remaining:
            remaining_44aad0();
            return;
        }
        throw FormatError("unknown HUD finite leaf");
    }
    void run();
    void run_countdown()
    {
        countdown_449b50();
    } // 449B50 opens its own anchor scope
    void run_position(std::uint32_t index, float x, float y, std::uint32_t depth)
    {
        position_4487d0(index, x, y, depth);
    }

  private:
    void score_phase_448360()
    {
        auto &phase = h(0x294);
        if (phase != 0)
        {
            phase = (phase + 1) & 15u;
            set({{0, 10},
                 {12, 0x3f7ae148},
                 {64, 0x3fc00000},
                 {68, 0x3fc00000},
                 {28, 0x44000000},
                 {36, 0x440f0000},
                 {16, 0x410ccccd},
                 {20, 0x4139999a},
                 {40, 11},
                 {44, 0x20}});
            fp(4, hr(0x50) + k(0x46eafc));
            fp(8, hr(0x54) + k(0x46eaf8));
            fp(24, uv(static_cast<std::uint32_t>(int(hud_source::animation[phase >> 1]) + 122)));
            fp(32, dr(24) + k(0x46eaf4));
            quad();
        }
        else if (as_signed(physics().stunt.score_hold) % 10 == 1)
            phase = 1;
    }
    void stars_448480()
    {
        const float x = static_cast<float>(hr(0x50) + k(0x46e200)), y = static_cast<float>(hr(0x54) + k(0x46eb00));
        auto count = physics().stunt.displayed_chain_count;
        std::uint32_t i = 0;
        while (i < count)
        {
            require(i < 10, "HUD star count outside ten source records");
            auto &phase = f.stars.phase[i];
            set({{0, 10}, {12, 0x3f7ae148}, {64, 0x3fc00000}, {68, 0x3fc00000}});
            fp(4, double(hud_source::star_x[i]) + x);
            fp(8, double(y));
            if (i < count)
            {
                if (phase < 8)
                    ++phase;
            }
            else if (phase != 0)
                phase = (phase + 1) & 15u;
            const auto index = phase >> 1;
            require(index < 8, "HUD star phase beyond signed source sequence");
            set({{28, 0x44110000}, {36, 0x441c0000}, {16, 0x410ccccd}, {20, 0x410ccccd}, {40, 11}, {44, 0x20}});
            fp(24, uv(static_cast<std::uint32_t>(int(hud_source::animation[8 + index]) + 123)));
            fp(32, dr(24) + k(0x46eaf4));
            quad();
            ++i;
            count = physics().stunt.displayed_chain_count;
        }
    }
    void gear_4485e0()
    {
        const auto index = static_cast<std::uint32_t>(physics().gear);
        require(index < 9, "HUD gear atlas index outside source rows");
        const auto row = index * 6;
        const auto &table = hud_source::gear;
        set({{0, 2}, {12, 0x3f733333}, {40, 14}, {44, 0x20}});
        fp(16, double(as_signed(table[row + 4])) * k(0x46e31c));
        put(8, h(0x108));
        fp(4, hr(0x104) - dr(16));
        fp(24, uv(table[row]));
        fp(28, uv(table[row + 1]));
        fp(32, uv(table[row] + table[row + 4]));
        fp(36, uv(table[row + 5] + table[row + 1]));
        fp(20, double(as_signed(table[row + 5])) * k(0x46e31c));
        quad();
        put(12, 0x3f733333);
        fp(4, hr(0x104) - k(0x46e1f8));
        fp(8, hr(0x108) + k(0x46eb04));
        fp(24, uv(table[48]));
        fp(28, uv(table[49]));
        fp(32, uv(table[52] + table[48]));
        fp(36, uv(table[53] + table[49]));
        fp(16, double(as_signed(table[52])) * k(0x46e31c));
        fp(20, double(as_signed(table[53])) * k(0x46e31c));
        quad();
    }
    void position_4487d0(std::uint32_t index, float x, float y, std::uint32_t depth)
    {
        require(index < 12, "HUD position atlas outside source rows");
        const auto row = index * 6;
        const auto &t = hud_source::place;
        set({{0, 2}, {12, depth}, {40, 14}, {44, 0x20}});
        fp(4, double(as_signed(t[row + 2])) + x);
        fp(8, double(as_signed(t[row + 3])) + y);
        const auto atlas = [&](unsigned at) {
            fp(24, uv(t[at]));
            fp(28, uv(t[at + 1]));
            fp(32, uv(t[at + 4]) + dr(24));
            fp(36, uv(t[at + 5]) + dr(28));
            fp(16, double(as_signed(t[at + 4])) * k(0x46e31c));
            fp(20, double(static_cast<float>(as_signed(t[at + 5]))) * k(0x46e31c));
        };
        atlas(row);
        quad();
        if (index < 3)
        {
            const auto next = (index + 9) * 6;
            put(12, depth);
            fp(4, (double(as_signed(t[next + 2])) + as_signed(t[row + 2])) + x);
            fp(8, (double(as_signed(t[next + 3])) + as_signed(t[row + 3])) + y);
            atlas(next);
        }
        else
        {
            set({{40, 14}, {12, depth}});
            fp(4, double(x) + k(0x46eaf4));
            fp(8, double(y) + k(0x46e220));
            atlas(8 * 6);
        }
        put(44, 0x20);
        quad();
    }
    void position_448a70()
    {
        const auto slot = current();
        const auto index = f.vehicles.physics[slot].energy_exempt == 1 ? f.vehicles.race[slot].finish_state
                                                                       : f.vehicles.race[slot].place;
        const float x = as_float(h(0xe0)), y = as_float(h(0xe4));
        position_4487d0(index, x, y, 0x3f733333);
    }
    void speed_4492d0(float speed)
    {
        std::array<std::uint32_t, 3> digits{};
        const auto leading = digits_448150(ftol(speed), digits, false);
        set({{0, 2},
             {12, 0x3f733333},
             {16, 0x4199999a},
             {20, 0x41c66667},
             {40, 11},
             {44, 0x20},
             {28, 0x44600000},
             {36, 0x447f0000}});
        double x = (double(leading) * k(0x46e49c) + hr(0x2c)) + k(0x46eb40);
        float stored_x = static_cast<float>(x);
        fp(8, hr(0x30) + k(0x46eb3c));
        for (const auto digit : digits)
            if (digit != 0xffffffffu)
            {
                fp(4, x);
                const double u = uv(digit * 25u + 1u);
                fp(24, u);
                fp(32, u + k(0x46e624));
                quad();
                x = double(stored_x) + k(0x46e49c);
                stored_x = static_cast<float>(x);
            }
    }
    void energy_4493e0()
    {
        if (source_energy_)
        {
            source_energy_->energy_4493e0();
            return;
        }
        const auto slot = current();
        const auto &car = f.vehicles.physics[slot];
        const auto &parameters = f.vehicles.parameters[slot];
        if (car.energy == 0 || std::isnan(car.energy))
            return;
        const double ratio = double(car.energy) / car.energy_capacity;
        const float stored_ratio = static_cast<float>(ratio);
        const auto selected = f.hud.selected_hud;
        require(selected < 4, "energy HUD identity");
        auto &phase = f.hud.layouts[selected].elements[17].word_1c;
        if (ratio < parameters.turbo_energy_threshold || std::isnan(ratio))
        {
            --phase;
            if (as_signed(phase) <= 0)
            {
                phase = ftol(double(stored_ratio) * k(0x46e378));
                if (as_signed(phase) < 24)
                    phase = 24;
                return;
            }
            if (car.energy_exempt == 0 && as_signed(phase) < 16)
            {
                if (phase != 1 || f.frame_pause != 0)
                    return;
                const auto volume =
                    ftol((double(parameters.turbo_energy_threshold) - stored_ratio) * k(0x46e2e0) + k(0x46e5fc));
                require(audio_ != nullptr, "HUD energy has no audio receiver");
                audio_start(*audio_, 22, 0, 0, volume, 0, 1, nullptr);
                return;
            }
        }
        const double right = double(stored_ratio) * k(0x46eb4c), outer = right + k(0x46eb48);
        set({{0, 6},
             {76, 0},
             {80, 0},
             {84, 0},
             {88, 0x442a0000},
             {96, 0},
             {104, 0x442a0000},
             {112, 0x41666667},
             {120, 0x44390000},
             {124, 0},
             {128, 0x41666667},
             {132, 0},
             {136, 0x44390000},
             {12, 0x3f51eb85},
             {16, 0x42bccccd},
             {20, 0x41666667},
             {40, 11},
             {44, 0x20}});
        fp(92, outer);
        fp(100, outer * k(0x46e20c));
        fp(108, right);
        fp(116, right * k(0x46e20c));
        // ESI keeps the selected HUD address through this source helper.
        const auto &layout = f.hud.layouts[selected].elements[0];
        fp(4, double(as_float(layout.word_1c)) + k(0x46eb44));
        fp(8, (double(as_float(layout.word_20)) + k(0x46eb40)) - k(0x46e580));
        quad();
    }
    void needle_4495e0()
    {
        set({{0, 5},
             {76, 0x40e3d70a},
             {80, 0},
             {84, 0x44540000},
             {88, 0x440b0000},
             {92, 0x40d9999a},
             {96, 0},
             {100, 0x44540000},
             {104, 0x440b0000},
             {108, 0x41966667},
             {112, 0x4281999a},
             {116, 0x44640000},
             {120, 0x445c0000},
             {124, 0x3ecccccd},
             {128, 0x4281999a},
             {132, 0x444d0000},
             {136, 0x445c0000},
             {12, 0x3f733333},
             {40, 11},
             {44, 0x20},
             {56, 0x40f33333},
             {60, 0x42600000}});
        fp(4, hr(0x2c) + k(0x46eb58));
        fp(8, hr(0x30) - k(0x46e528));
        const auto slot = current();
        fp(52,
           (double(f.vehicles.physics[slot].rpm) / f.vehicles.parameters[slot].rpm_max) * k(0x46eb54) - k(0x46eb50));
        quad();
    }
    void gauge_449720()
    {
        set({{0, 2},
             {12, 0x3f70a3d7},
             {24, 0},
             {28, 0},
             {32, 0x44480000},
             {36, 0x43fa0000},
             {16, 0x43200000},
             {20, 0x42c80000},
             {40, 11},
             {44, 0x20}});
        put(4, h(0x2c));
        put(8, h(0x30));
        quad();
    }
    void gauge_4497b0()
    {
        set({{0, 2},
             {12, 0x3f70a3d7},
             {24, 0x432c0000},
             {28, 0x43c60000},
             {32, 0x44480000},
             {36, 0x43fa0000},
             {16, 0x42fb3333},
             {20, 0x41a66667},
             {40, 11},
             {44, 0x20}});
        fp(4, hr(0x2c) + k(0x46eb60));
        fp(8, hr(0x30) + k(0x46eb5c));
        quad();
    }
    void score_44adc0()
    {
        animated_score_448ab0();
        score_phase_448360();
        stars_448480();
    }
    void gauge_44add0()
    {
        gauge_449720();
        const float speed = static_cast<float>(std::abs(double(physics().speed)) * k(0x46eb98));
        speed_4492d0(speed);
        energy_4493e0();
        needle_4495e0();
    }
    void gauge_44ae10()
    {
        gauge_4497b0();
        const float speed = static_cast<float>(std::abs(double(physics().speed)) * k(0x46eb98));
        speed_4492d0(speed);
        energy_4493e0();
    }
    void wrong_way_449850();
    void notice_4499a0();
    void countdown_449b50();
    void waiting_449ce0();
    void finish_449d70();
    void start_44a3d0();
    void notice_text_442e30();
};
void Hud::animated_score_448ab0()
{
    set({{0, 2},
         {40, 11},
         {44, 0x20},
         {12, 0x3f70a3d7},
         {24, 0x41000000},
         {28, 0x44000000},
         {32, 0x43f00000},
         {36, 0x44260000},
         {16, 0x430d999a},
         {20, 0x42366667}});
    fp(4, hr(0x50) - k(0x46eb28));
    fp(8, hr(0x54) - k(0x46eb24));
    quad();
    float score = physics().score;
    if (score >= k(0x46e520))
        score = as_float(0x4b18967f);
    require(std::isfinite(score), "HUD score outside finite original fraction domain");
    std::array<std::uint32_t, 7> digits{};
    digits_448150(ftol(score), digits, true);
    float scale = 1, inverse = 1;
    double x = hr(0x50) + k(0x46eb20);
    float stored_x = static_cast<float>(x);
    for (int i = 6; i >= 0; --i)
    {
        fp(4, x);
        put(8, h(0x54));
        set({{12, 0x3f733333}, {16, 0x4179999a}, {20, 0x41accccd}, {24, i == 6 ? 0x44650000u : 0x44720000u}});
        fp(28, uv(digits[static_cast<unsigned>(i)] * 18u));
        double fraction = hud_fraction_46291a(double(scale) * score);
        if (fraction <= scale)
            fraction = fraction * inverse - k(0x46e264);
        else
            fraction = 0;
        const auto movement = ftol(fraction * k(0x46ead8));
        const double v = uv(movement) + dr(28);
        fp(28, v);
        if (v < 0)
            fp(28, dr(28) + k(0x46eb1c));
        fp(32, dr(24) + k(0x46eb18));
        fp(36, dr(28) + k(0x46eb14));
        quad();
        x = double(stored_x) - k(0x46eb10);
        stored_x = static_cast<float>(x);
        inverse = static_cast<float>(double(inverse) * k(0x46e220));
        scale = static_cast<float>(double(scale) * k(0x46e230));
    }
    set({{12, 0x3f75c28f},
         {24, 0x41a00000},
         {28, 0x44030000},
         {32, 0x43c00000},
         {36, 0x44150000},
         {16, 0x42da6667},
         {20, 0x41accccd},
         {44, 0x48},
         {48, 0x80}});
    put(4, h(0x50));
    put(8, h(0x54));
    quad();
    set({{0, 2},
         {12, 0x3f70a3d7},
         {24, 0x43c00000},
         {28, 0x445b0000},
         {32, 0x441c0000},
         {36, 0x447c0000},
         {16, 0x42400000},
         {20, 0x41d33333},
         {40, 12},
         {44, 0x20}});
    fp(4, hr(0x50) + k(0x46eb0c));
    fp(8, hr(0x54) - k(0x46eb08));
    quad();
}
void Hud::timer_448e00(const std::array<std::uint32_t, 8> &digits, std::uint32_t ticks, float x, float y,
                       std::uint32_t animate, std::uint32_t hide)
{
    set({{0, 2},
         {12, 0x3f70a3d7},
         {24, 0},
         {28, 0x443a0000},
         {32, 0x43a40000},
         {36, 0x44550000},
         {16, 0x42833333},
         {20, 0x41accccd},
         {40, 11},
         {44, 0x20}});
    fp(4, x);
    fp(8, y);
    quad();
    if (hide == 0)
    {
        set({{40, 11}, {0, 2}, {44, 0x20}, {12, 0x3f733333}});
        const float origin_x = static_cast<float>(double(x) + k(0x46eb30)),
                    origin_y = static_cast<float>(double(y) + k(0x46eb24));
        for (unsigned i = 0; i < 5; ++i)
        {
            const auto *p = hud_source::timer.data() + i * 5;
            set({{16, 0x41266667}, {20, 0x41666667}});
            fp(8, origin_y);
            fp(4, double(as_signed(hud_source::positions[i])) * k(0x46e31c) + origin_x);
            fp(24, (double(as_float(p[4])) * k(0x46e75c)) * k(0x46e6e4));
            fp(28, uv(digits[i] * 18u));
            fp(32, dr(24) + k(0x46eb18));
            fp(36, dr(28) + k(0x46eb14));
            if (animate == 1)
            {
                double fraction = hud_fraction_46291a(double(as_signed(ticks)) * as_float(p[0]));
                if (fraction <= as_float(p[2]))
                    fraction = fraction * as_float(p[1]) - k(0x46e264);
                else
                    fraction = 0;
                const double offset = as_signed(ftol((fraction * k(0x46eb2c)) * k(0x46e6e4)));
                fp(28, offset + dr(28));
                fp(36, offset + dr(36));
                if (dr(28) < 0)
                {
                    const double wrap = as_signed(ftol((double(as_float(p[3])) * k(0x46e75c)) * k(0x46e6e4)));
                    fp(28, wrap + dr(28));
                    fp(36, wrap + dr(36));
                }
            }
            quad();
        }
    }
    set({{0, 2},
         {12, 0x3f75c28f},
         {24, 0x44030000},
         {28, 0x44280000},
         {32, 0x444c0000},
         {36, 0x443a0000},
         {16, 0x4269999a},
         {20, 0x41666667},
         {40, 11},
         {44, 0x48},
         {48, 0x80}});
    fp(4, double(x) + k(0x46eb24));
    fp(8, double(y) + k(0x46eb24));
    quad();
}
void Hud::timers_4490e0()
{
    auto slot = current();
    if (f.vehicles.race[slot].lap_timer != -1)
    {
        const auto ticks = static_cast<std::uint32_t>(f.vehicles.race[slot].lap_timer),
                   hide = static_cast<std::uint32_t>(f.vehicles.physics[slot].energy_exempt);
        const auto digits = digits_4482a0(ticks);
        const float x = as_float(h(0x74)), y = as_float(h(0x78));
        timer_448e00(digits, ticks, x, y, 1, hide);
        set({{0, 2},
             {12, 0x3f75c28f},
             {24, 0x44120000},
             {28, 0x443c0000},
             {32, 0x444c0000},
             {36, 0x445d0000},
             {16, 0x4239999a},
             {20, 0x41d33333},
             {40, 11},
             {44, 0x20}});
        fp(4, (hr(0x74) - k(0x46eb38)) + k(0x46e31c));
        put(8, h(0x78));
        quad();
    }
    if (f.records.lap_ticks != 0xffffffffu && race().lap_timer != -1)
    {
        const auto ticks = f.records.lap_ticks;
        const auto digits = digits_4482a0(ticks);
        const float x = as_float(h(0x98)), y = static_cast<float>(hr(0x9c) + k(0x46e5fc));
        timer_448e00(digits, ticks, x, y, 0, 0);
        set({{0, 2},
             {12, 0x3f75c28f},
             {24, 0x43a60000},
             {28, 0x443b0000},
             {32, 0x44110000},
             {36, 0x445e0000},
             {16, 0x42466667},
             {20, 0x41e00000},
             {40, 11},
             {44, 0x20}});
        fp(4, (hr(0x98) - k(0x46eb34)) + k(0x46e31c));
        fp(8, hr(0x9c) + k(0x46e5fc));
        quad();
    }
}
void Hud::laps_44a4a0(std::uint32_t number, std::uint32_t variant, float x, float y)
{
    const std::array<std::uint32_t, 2> digits = {number % 10u, (number / 10u) % 10u};
    set({{12, 0x3f733333}, {16, 0x41266667}, {20, 0x41666667}, {40, 11}});
    for (unsigned i = 0; i < 2; ++i)
    {
        fp(4, double(as_signed(hud_source::positions[i])) * k(0x46e31c) + x);
        const auto row = i + (variant ? 2u : 0u);
        fp(24, (double(as_float(hud_source::timer[row * 5 + 4])) * k(0x46e75c)) * k(0x46e6e4));
        fp(28, (double(digits[i] * 18u) * k(0x46e75c)) * k(0x46e6e4));
        fp(32, dr(24) + k(0x46eb18));
        fp(36, dr(28) + k(0x46eb14));
        fp(8, double(y) + k(0x46eb24));
        quad();
    }
}
void Hud::lap_display_44a5c0()
{
    if (race().lap_timer == -1)
        return;
    set({{0, 2},
         {12, 0x3f70a3d7},
         {24, 0x442b0000},
         {28, 0x42f00000},
         {32, 0x44650000},
         {36, 0x43820000},
         {16, 0x4239999a},
         {20, 0x41e00000},
         {40, 12},
         {44, 0x20}});
    fp(4, (hr(0xbc) - k(0x46eb38)) + k(0x46e31c));
    fp(8, (hr(0xc0) + k(0x46ead4)) - k(0x46e1d8));
    quad();
    set({{12, 0x3f70a3d7},
         {24, 0x442b0000},
         {28, 0},
         {32, 0x447e0000},
         {36, 0x42d80000},
         {16, 0x4284cccd},
         {20, 0x41accccd}});
    put(4, h(0xbc));
    fp(8, hr(0xc0) + k(0x46ead4));
    quad();
    float x = static_cast<float>(hr(0xbc) + k(0x46eb30)), y = static_cast<float>(hr(0xc0) + k(0x46ead4));
    laps_44a4a0(f.clock.lap_count, 0, x, y);
    x = static_cast<float>((hr(0xbc) + k(0x46eb30)) - k(0x46eb88));
    y = static_cast<float>(hr(0xc0) + k(0x46ead4));
    laps_44a4a0(static_cast<std::uint32_t>(race().race_index), 1, x, y);
    set({{12, 0x3f75c28f},
         {24, 0x44030000},
         {28, 0x44280000},
         {32, 0x444c0000},
         {36, 0x443a0000},
         {16, 0x4269999a},
         {20, 0x41666667},
         {40, 11},
         {44, 0x48},
         {48, 0x80}});
    fp(4, hr(0xbc) + k(0x46eb24));
    fp(8, hr(0xc0) + k(0x46eb84));
    quad();
}
void Hud::progress_44a800()
{
    const auto own = current();
    const float progress = f.vehicles.race[own].progress;
    const double first_y = hr(0x54) + k(0x46e1f8);
    const float saved_y = static_cast<float>(first_y);
    set({{0, 2},
         {4, 0x41800000},
         {12, 0x3f6e147b},
         {24, 0x42800000},
         {28, 0x442a0000},
         {32, 0x43e80000},
         {36, 0x44340000},
         {16, 0x43600000},
         {20, 0x40800000},
         {40, 11},
         {44, 0x20}});
    fp(8, first_y);
    quad();
    set({{12, 0x3f6b851f}, {24, 0x44440000}, {28, 0x44330000}, {32, 0x44480000}, {36, 0x44370000}});
    fp(4, dr(4) + k(0x46e460));
    fp(8, dr(8) + k(0x46e460));
    quad();
    const float y = static_cast<float>(double(saved_y) - k(0x46e460));
    const auto count = f.ranking.count;
    set({{16, 0x41400000}, {20, 0x41300000}, {40, 14}, {48, 0xc0}, {44, 0x48}});
    fp(8, y);
    if (as_signed(count - 1) < 0)
        return;
    require(count <= 6, "HUD ranking traversal outside six slots");
    for (unsigned n = count; n != 0; --n)
    {
        const auto current_vehicle = current();
        const auto id = f.ranking.slots[n - 1];
        require(id.has_value() && *id < 6, "HUD ranking vehicle pointer outside live world");
        const auto slot = *id;
        if (slot == current_vehicle)
        {
            set({{16, 0x41800000}, {20, 0x41700000}, {12, 0x3f733333}});
            fp(8, double(y) - k(0x46e200));
        }
        else
        {
            set({{16, 0x41400000}, {20, 0x41300000}, {12, 0x3f70a3d7}});
            fp(8, y);
        }
        double delta = double(f.vehicles.race[slot].progress) - progress;
        if (!(delta >= k(0x46e6d0)))
            delta = k(0x46e6d0);
        else if (delta > k(0x46e2f0))
            delta = k(0x46e2f0);
        fp(4, delta * k(0x46eb90) + k(0x46eb8c));
        const auto livery = f.cars[slot].settings.livery;
        require(livery < 9, "HUD livery icon outside pinned source atlas");
        for (unsigned i = 0; i < 4; ++i)
            put(24 + i * 4, hud_source::icons[livery * 4 + i]);
        const auto &car = f.vehicles.physics[slot];
        if (car.energy_exempt != 0 && slot != current_vehicle)
            continue;
        if (double(car.energy_capacity) * f.vehicles.parameters[slot].turbo_energy_threshold > car.energy &&
            f.vehicles.tick[slot].low_energy_phase % 15 >= 7)
            continue;
        quad();
    }
}
void Hud::remaining_44aad0()
{
    auto remaining = f.clock.remaining_ticks;
    if (remaining == 0 || remaining > 5400)
        return;
    set({{0, 0x22}, {12, 0x3f7ae148}, {44, 0x48}, {48, 0xc0}, {72, 0xffff64}});
    if (remaining < 600 && ((remaining / 30) & 1))
        put(72, 0xff0000);
    const double elapsed = std::min(double(5400u - remaining), k(0x46e3ec));
    float x = static_cast<float>((elapsed - k(0x46e5c0)) + hr(0xe0) + f.hud.margins.x);
    const float y = static_cast<float>((hr(0xe4) + k(0x46e20c)) + f.hud.margins.y);
    unsigned divisor = 100;
    bool seen = false;
    for (unsigned i = 0; i < 3; ++i)
    {
        const auto digit = (remaining / (divisor * 60u)) % 10u;
        if (digit != 0 || seen || i == 2)
        {
            seen = true;
            fp(4, x);
            fp(8, y);
            const auto row = digit * 7;
            for (unsigned j = 0; j < 4; ++j)
                put(24 + j * 4, hud_source::digits[row + 1 + j]);
            put(40, hud_source::digits[row]);
            put(16, hud_source::digits[row + 5]);
            put(20, hud_source::digits[row + 6]);
            quad();
            remaining = f.clock.remaining_ticks;
        }
        x = static_cast<float>(double(x) + k(0x46e76c));
        divisor /= 10;
    }
}
void Hud::wrong_way_449850()
{
    AnchorScope anchor(host_wide_overlay::race::center); // NEW HOST (wide_hud R20)
    if (f.frame_pause == 1)
        return;
    const auto ticks = race().event7678;
    if (ticks < 30)
        return;
    const auto distance = static_cast<unsigned>(std::abs(static_cast<int>(ticks % 60u) - 30));
    const auto alpha = std::min((distance * 510u) / 60u, 255u);
    set({{0, 2},
         {4, 0x43873333},
         {8, 0x432c0000},
         {12, 0x3f7ae148},
         {24, 0x44030000},
         {28, 0x41800000},
         {32, 0x447f0000},
         {36, 0x43b00000},
         {16, 0x42c66667},
         {20, 0x42866667},
         {40, 13},
         {44, 0x48},
         {48, alpha}});
    quad();
    select_font(1);
    const auto color = ((0u - static_cast<unsigned>(alpha < 127)) & 0xff404100u) - 0x100u;
    text_call(text(7), 1, {320, static_cast<float>(dr(8) + k(0x46e46c)), as_float(0x3f7d70a4), color, 2, 6});
    select_font(0);
}
void Hud::notice_4499a0()
{
    AnchorScope anchor(host_wide_overlay::race::center); // NEW HOST (wide_hud R20)
    if (race().notice_active != 1 || overlay() != 20)
        return;
    const auto ticks = race().notice_ticks;
    const auto alpha = ticks < 30 ? (ticks * 224u) / 30u : ticks < 91 ? 224u : (ticks * (0u - 224u) + 0x6900u) / 30u;
    set({{0, 2},
         {8, 0x42933333},
         {12, 0x3f7ae148},
         {24, 0},
         {28, 0},
         {32, 0x44250000},
         {36, 0x43c00000},
         {16, 0x43040000},
         {20, 0x4299999a},
         {40, 15},
         {44, 0x48},
         {48, alpha}});
    const double motion = double(ticks) * k(0x46eb6c);
    fp(4, motion - k(0x46eb68));
    quad();
    set({{0, 2},
         {8, 0x43166666},
         {12, 0x3f7ae148},
         {24, 0},
         {28, 0x43c20000},
         {32, 0x44500000},
         {36, 0x44210000},
         {16, 0x43266667},
         {20, 0x424ccccd},
         {40, 15},
         {44, 0x48},
         {48, alpha}});
    // 449A8D stores a float while retaining the first motion value in ST0.
    // The second panel reloads that stored float at449AC1.
    fp(4, k(0x46eb64) - static_cast<float>(motion));
    quad();
}
void Hud::countdown_449b50()
{
    AnchorScope anchor(host_wide_overlay::race::center); // NEW HOST (wide_hud R20)
    if (f.frame_pause == 1)
        return;
    const auto ticks = f.vehicles.race[vehicle(0)].start_countdown;
    if (ticks == 0 || ticks >= 240)
        return;
    const auto selected = overlay();
    if (selected < 16 || selected > 19)
        return;
    std::uint32_t alpha = 192;
    if ((ticks < 31 || ticks > 119) && ticks % 60 < 30)
        alpha = ((ticks % 60) * 192u) / 30u;
    set({{48, alpha},
         {0, 2},
         {4, 0x4359999a},
         {8, 0x42933333},
         {12, 0x3f7ae148},
         {24, 0},
         {28, 0},
         {32, 0x44800000},
         {36, 0x44800000},
         {16, 0x434ccccd},
         {20, 0x434ccccd},
         {40, 15},
         {72, 0},
         {44, 0x48}});
    quad();
}
void Hud::waiting_449ce0()
{
    AnchorScope anchor(host_wide_overlay::race::center); // NEW HOST (wide_hud R20)
    if (startup(current(), 0x76ec) != 1 || startup(vehicle(0), 0x76a4) != 0)
        return;
    const auto phase = startup(current(), 0x76ac);
    select_font(1);
    const auto color = ((0u - static_cast<unsigned>(phase > 30)) & 0xff404100u) - 0x100u;
    text_call(text(16), 1, {320, 240, as_float(0x3f733333), color, 2, 0});
    select_font(0);
}
void Hud::finish_449d70()
{
    AnchorScope anchor(host_wide_overlay::race::center); // NEW HOST (wide_hud R20)
    const auto slot = current();
    if (startup(slot, 0x76b8) == 0)
        return;
    const auto selected = overlay();
    if (selected < 21 || selected > 23)
        return;
    const auto ticks = startup(slot, 0x76bc);
    const auto numerator = ticks < 180 ? ticks * 255u : ticks * (0u - 255u) + 0xef10u;
    const auto alpha = std::min(numerator / 60u, 255u);
    set({{48, alpha},
         {0, 2},
         {4, 0x43266666},
         {12, 0x3f7d70a4},
         {24, 0},
         {28, 0},
         {32, 0x44800000},
         {36, 0x44800000},
         {16, 0x4399999a},
         {20, 0x4399999a},
         {40, 15},
         {44, 0x48}});
    fp(8, k(0x46eb70) - as_float(startup(slot, 0x76c0)));
    quad();
}
void Hud::start_44a3d0()
{
    AnchorScope anchor(host_wide_overlay::race::center); // NEW HOST (wide_hud R20)
    const auto slot = current();
    if (startup(slot, 0x768c) == 0)
        return;
    const auto alpha = std::min((startup(slot, 0x7690) * 240u) / 60u, 240u);
    set({{48, alpha},
         {0, 2},
         {4, 0x43266666},
         {12, 0x3f7d70a4},
         {24, 0},
         {28, 0},
         {32, 0x44800000},
         {36, 0x44800000},
         {16, 0x4399999a},
         {20, 0x4319999a},
         {40, 15},
         {44, 0x48}});
    fp(8, double(as_float(startup(slot, 0x7694))) + k(0x46eb80));
    quad();
}
void Hud::notice_text_442e30()
{
    AnchorScope anchor(host_wide_overlay::race::bottom_center); // NEW HOST (wide_hud R20)
    if (f.frame_pause == 1)
        return;
    const auto controller = f.hud.controls.current;
    require(controller < 4, "HUD notice controller identity");
    const auto limit = static_cast<std::uint32_t>(f.hud.margins.y) + 40u;
    const auto &notices = f.hud.controls.controllers[controller].notices;
    for (const auto &notice : notices)
    {
        if (notice.remaining == 0 || notice.remaining == notice.initial)
            continue;
        const auto remaining = static_cast<std::uint32_t>(notice.remaining),
                   initial = static_cast<std::uint32_t>(notice.initial);
        auto animation = remaining < limit ? remaining : std::min(initial - remaining, limit);
        if (f.split_setting != 0)
            animation = limit;
        const auto color = ((0u - static_cast<unsigned>(30u < remaining % 60u)) & 0xffff4140u) + 0xffff00u;
        const float y = static_cast<float>(k(0x46e754) - double(animation));
        const std::span<const std::uint8_t> bytes{reinterpret_cast<const std::uint8_t *>(notice.text.data()),
                                                  notice.text.size()};
        text_call(bytes, 1, {320, y, as_float(0x3f70a3d7), color, 1.75f, 6});
        return;
    }
}
void Hud::run()
{
    require(f.hud.controls.current < 4, "HUD parent current controller outside four owners");
    f.hud.selected_hud = f.hud.controls.current;
    std::fill(d.begin(), d.end(), 0u);
    prepare_hud_layout_447d80(f.hud);
    if (f.hud.countdown[f.hud.selected_hud].active == 0)
    {
        countdown_449b50();
        return;
    }
    apply_hud_displacement_44ac90(f.hud, f.vehicles.physics);
    // NEW HOST (wide_hud R20): same leaf order through run_leaf, which only
    // adds the host anchor scope around the identical leaf call.
    using L = RaceHudFiniteLeaf;
    switch (f.clock.game_mode)
    {
    case 1:
        run_leaf(L::position);
        run_leaf(L::timers);
        run_leaf(L::laps);
        run_leaf(L::gauge);
        run_leaf(L::score);
        run_leaf(L::progress);
        run_leaf(L::gear);
        break;
    case 2:
        run_leaf(L::position);
        if (f.split_setting == 0)
        {
            run_leaf(L::timers);
            run_leaf(L::laps);
            run_leaf(L::gauge);
            run_leaf(L::score);
            run_leaf(L::progress);
            run_leaf(L::gear);
            wrong_way_449850();
        }
        else
        {
            run_leaf(L::split_gauge);
            run_leaf(L::gear);
            wrong_way_449850();
        }
        waiting_449ce0();
        break;
    case 3:
        run_leaf(L::remaining);
        run_leaf(L::gauge);
        run_leaf(L::score);
        run_leaf(L::gear);
        waiting_449ce0();
        break;
    case 4:
        run_leaf(L::timers);
        run_leaf(L::gauge);
        run_leaf(L::gear);
        wrong_way_449850();
        break;
    default:
        break;
    }
    notice_4499a0();
    if (f.split_setting == 0)
        finish_449d70();
    start_44a3d0();
    notice_text_442e30();
}
} // namespace
void submit_race_hud_44ae40(RaceHudSubmission frame, PolygonSubmissionSink &sink)
{
    Hud(frame, sink).run();
}
void submit_race_countdown_449b50(RaceHudSubmission frame, PolygonSubmissionSink &sink)
{
    Hud(frame, sink).run_countdown();
}
void submit_race_position_4487d0(RaceHudSubmission frame, PolygonSubmissionSink &sink, std::uint32_t index, float x,
                                 float y, std::uint32_t depth)
{
    Hud(frame, sink).run_position(index, x, y, depth);
}
void submit_race_hud_finite_leaf(RaceHudLeafInputs frame, RaceHudFiniteLeaf leaf, RaceHudSourceEnergy &energy,
                                 PolygonSubmissionSink &sink)
{
    Hud(frame, energy, sink).run_leaf(leaf);
}
void submit_race_position_4487d0(RaceHudLeafInputs f, PolygonSubmissionSink &sink, std::uint32_t index, float x,
                                 float y, std::uint32_t depth)
{
    Hud(f, sink).run_position(index, x, y, depth);
}

} // namespace sgn::retail
