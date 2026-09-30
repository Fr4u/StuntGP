#include "sgn/retail_normal_race_lifecycle.hpp"
#include "sgn/retail_challenge_lifecycle.hpp"
#include "sgn/retail_hud_lifetime.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_word;
using Fields = std::array<std::uint32_t, 25>;
Fields &fields(NormalRaceLifecycleWorld &w)
{
    return w.startup[w.race.current_slot].fields_768c;
}
void validate(const NormalRaceLifecycleWorld &w)
{
    if (w.game_type != 2 || (w.race.clock.game_mode != 1 && w.race.clock.game_mode != 2) || w.split != 0)
        throw FormatError("normal race requires local game_type2 and mode1/2");
    if (w.race.controls.current >= 4 || w.race.current_slot >= 6 || w.selected_hud >= 4)
        throw FormatError("normal race world identity");
    for (const auto &c : w.race.controls.controllers)
        if (c.active == 1 && c.vehicle >= 6)
            throw FormatError("normal race active controller vehicle");
}
NormalRacePresentation &services(NormalRaceLifecycleWorld &w)
{
    if (!w.services)
        throw FormatError("normal race presentation services missing");
    return *w.services;
}
bool confirm(NormalRaceLifecycleWorld &w)
{
    auto &s = w.presentation_state;
    if (s.confirm_consumed != 0 || s.confirm_pressed != 1)
        return false;
    s.confirm_consumed = s.confirm_pressed = 1;
    return true; // 443D80
}
void hide_hud(NormalRaceLifecycleWorld &w)
{ // 447BA0 ECX0
    w.selected_hud = w.race.controls.current;
    auto &h = w.hud.countdown[w.selected_hud];
    h.ticks = 0;
    h.enabled = 1;
    h.value = 0;
    h.phase = as_word(100);
}
void overlay(NormalRaceLifecycleWorld &w, std::uint32_t hold, std::uint32_t delay, std::uint32_t flags,
             std::uint32_t from, std::uint32_t to, float rate)
{
    set_overlay_transition(w.overlays[w.race.controls.current], 2, hold, delay, flags, from, to, rate);
}
void all_overlay(NormalRaceLifecycleWorld &w, std::uint32_t hold, std::uint32_t delay, std::uint32_t flags,
                 std::uint32_t from, std::uint32_t to, float rate)
{
    for (unsigned i = 0; i < 4; ++i)
    {
        w.race.controls.current = i;
        if (w.race.controls.controllers[i].active == 1)
            overlay(w, hold, delay, flags, from, to, rate);
    }
}
void hud_phase(NormalRaceLifecycleWorld &w, unsigned element, std::uint32_t phase)
{
    if (element < 18)
    {
        auto h = hud_element(w.hud, w.selected_hud, element);
        h.active = 1;
        h.phase = phase;
        return;
    }
    // 44426C also visits row6: its element18 aliases controller0 notice0.
    // Preserve those exact writes in the existing owner, without an out-of-bounds array access.
    if (element != 18)
        throw FormatError("normal result HUD row outside source range");
    auto &n = w.race.controls.controllers[w.selected_hud].notices[0];
    n.initial = 1;
    std::memcpy(n.text.data(), &phase, 4);
}
void begin_results(NormalRaceLifecycleWorld &w)
{ // 443EA0
    const auto primary = w.race.controls.controllers[0].vehicle;
    auto &f = w.startup[primary].fields_768c;
    if (f[6] == 1)
        return;
    w.race.presentation.sound(47, 63);
    auto &p = w.presentation_state;
    p.row_ticks = p.visible_rows = p.field_60d728 = 0;
    w.selected_hud = 0;
    f[6] = 1;
    initialize_selected_hud_ranking(w.hud);
}
void begin_podium(NormalRaceLifecycleWorld &w)
{ // 4443C0
    auto &f = fields(w);
    f[11] = 1;
    f[12] = 0;
    f[13] = as_word(-600);
    f[14] = 0;
    f[15] = as_word(-1);
    w.race.presentation.sound(46, 63);
}
void begin_finish(NormalRaceLifecycleWorld &w)
{ // 444590, single-player branch
    auto &f = fields(w);
    const auto &race = w.race.race[w.race.current_slot];
    w.finish_music = 1;
    services(w).music(race.finish_state <= 2 ? 21 : 22, 0);
    f[18] = 1;
    f[19] = 1;
    f[20] = 0;
    f[21] = 0;
    f[23] = 0;
    float speed = std::abs(w.race.physics[w.race.current_slot].speed);
    if (speed < as_float(0x42055555))
        speed = as_float(0x42055555);
    else if (speed > 100)
        speed = 100;
    f[22] = as_word(speed);
    hide_hud(w);
    services(w).finish_camera();
    w.finish_music = 1;
}
bool tick_finish(NormalRaceLifecycleWorld &w)
{ // 4446D0; true is the game_type3 call441E20
    auto &f = fields(w);
    auto &r = w.race.race[w.race.current_slot];
    if (f[6] == 1 || f[11] == 1)
        return false;
    if (f[24] == 1)
    {
        if (++f[8] >= 60)
            f[8] = 0;
        return false;
    }
    if (w.race.physics[w.race.current_slot].energy_exempt == 1 && f[18] == 0)
        begin_finish(w);
    if (f[18] == 0 || f[19] == 0)
        return false;
    if (w.race.clock.game_mode != 3 && f[19] == 60 && r.finish_state <= 2)
        w.race.presentation.sound(19, 255);
    ++f[19];
    unsigned sound = 0;
    double unrounded_angle = double(as_float(f[22])) * 0.5 + as_float(f[21]);
    if (unrounded_angle >= 360)
    {
        sound = 46;
        unrounded_angle -= 360;
    }
    const float angle = vehicle_f32(unrounded_angle);
    f[21] = as_word(angle);
    float speed = vehicle_f32(double(as_float(f[22])) * double(0.98f));
    if (speed < 4)
        speed = 4;
    f[22] = as_word(speed);
    if (speed <= 4 && angle <= 10)
    {
        if (f[23] == 0)
        {
            f[23] = 1;
            sound = 50;
        }
        f[21] = f[22] = 0;
        if (f[19] > 360 && f[20] == 0)
        {
            f[20] = 1;
            overlay(w, 240, 0, 0, 0, 60, as_float(0x3f75c28f));
        }
    }
    if (sound)
        w.race.presentation.sound(sound, 127);
    if (f[19] > 30 && confirm(w))
    {
        w.race.presentation.sound(48, 255);
        if (f[20] == 0)
            overlay(w, 240, 0, 0, 0, 60, as_float(0x3f75c28f));
        f[19] = 0;
        f[20] = 480;
    }
    if (f[20] != 0)
    {
        ++f[20];
        if ((f[20] > 119 && w.music_active == 0) || f[20] > 479)
        {
            if (w.game_type == 3)
                return true;
            f[18] = f[19] = f[20] = f[6] = 0;
            if (r.finish_state < 3)
                begin_podium(w);
            else
                begin_results(w);
            services(w).music(23, 1);
        }
    }
    return false;
}
bool settle(Fields &f, unsigned p, unsigned v, unsigned a)
{
    if (std::abs(as_float(f[a])) >= 0.25f || std::abs(as_float(f[v])) >= 0.25f || std::abs(as_float(f[p])) >= 1)
    {
        const double acceleration = double(as_float(f[p])) * double(-0.01f);
        f[a] = as_word(vehicle_f32(acceleration));
        const double velocity = (acceleration + as_float(f[v])) * double(0.95f);
        f[v] = as_word(vehicle_f32(velocity));
        f[p] = as_word(vehicle_f32(velocity + as_float(f[p])));
        return false;
    }
    f[a] = f[v] = f[p] = 0;
    return true;
}
void begin_retirement(NormalRaceLifecycleWorld &w)
{ // 4449E0
    auto &f = fields(w);
    if (f[0] == 1)
        return;
    w.race.presentation.texture(15, 25);
    f[1] = 1;
    f[0] = 1;
    f[5] = 0;
    f[2] = as_word(-256);
    f[3] = 0;
    f[4] = as_word(-1);
    hide_hud(w);
    overlay(w, 60, 0, 0, 0, 60, as_float(0x3f75c28f));
    services(w).finish_camera();
    services(w).music(20, 0);
    w.audio.output_b_copy = 0;
    services(w).frequency_sound(47, 0x4000, 63);
}
bool tick_retirement(NormalRaceLifecycleWorld &w)
{ // 444AE0 /444A90; true requests441E20
    auto &f = fields(w);
    if (w.race.race[w.race.current_slot].stopped == 1 && f[0] == 0)
        begin_retirement(w);
    bool all = w.hud_block != 1;
    for (unsigned i = 0; i < 6; ++i)
        if (w.race.race[i].active == 1 && w.race.physics[i].special_mode != 1 && w.startup[i].fields_768c[5] != 1)
            all = false;
    if (all && w.retire_exit == 0)
    {
        w.retire_exit = 1;
        w.presentation_state.field_612bf0 = 1;
        return true;
    }
    if (f[0] == 0 || w.retire_exit == 1)
        return false;
    ++f[1];
    if ((f[1] > 539 && w.music_active == 0) || f[1] > 899)
    {
        f[5] = 1;
        return false;
    }
    if (settle(f, 2, 3, 4) && f[1] == 420)
        overlay(w, 120, 0, 300, 60, 100, 1);
    return false;
}
void tick_podium(NormalRaceLifecycleWorld &w)
{ // 444410
    auto &f = fields(w);
    if (f[11] == 0)
        return;
    const auto texture = w.race.race[w.race.current_slot].finish_state + 21;
    if (w.race.presentation.selected_texture() != texture)
        w.race.presentation.texture(15, texture);
    if (f[12] > 30 && confirm(w))
    {
        w.race.presentation.sound(48, 255);
        f[11] = f[12] = 0;
        begin_results(w);
    }
    if (settle(f, 13, 14, 15) && f[12] > 239)
    {
        f[11] = f[12] = 0;
        begin_results(w);
        ++f[12];
        return;
    }
    ++f[12];
}
void format_time(std::span<char, 16> output, std::uint32_t ticks)
{ // 4482A0 +443F10
    std::array<unsigned, 6> d{((ticks * 10u) / 6u) % 10u,          (ticks / 6u) % 10u,
                              (ticks / 60u) % 60u % 10u,           (ticks / 60u) % 60u / 10u,
                              ((ticks / 60u) % 3600u) / 60u % 10u, ((ticks / 60u) % 3600u) / 600u};
    const auto written =
        ticks < 36000
            ? std::snprintf(output.data(), output.size(), "%u:%u%u:%u%u", d[4], d[3], d[2], d[1], d[0])
            : std::snprintf(output.data(), output.size(), "%u%u:%u%u:%u%u", d[5], d[4], d[3], d[2], d[1], d[0]);
    if (written < 0 || written >= 16)
        throw FormatError("normal race result time overflow");
}
void result_row(NormalRaceLifecycleWorld &w, unsigned row, unsigned slot)
{ // 443F10
    auto &name = w.cars[slot].name;
    auto &r = w.race.race[slot];
    format_time(std::span<char, 16>(name.data() + 16, 16), std::bit_cast<std::uint32_t>(r.race_timer));
    format_time(std::span<char, 16>(name.data() + 32, 16), r.best_lap_ticks);
    const auto score = w.race.physics[slot].score;
    if (!std::isfinite(score) || score < -9223372036854775808.0 || score >= 9223372036854775808.0)
        throw FormatError("normal result score conversion");
    const auto integer = static_cast<std::uint32_t>(static_cast<std::int64_t>(score));
    std::snprintf(name.data() + 48, 16, "%d", std::bit_cast<std::int32_t>(integer));
    w.presentation_state.result_rows[row] = slot;
}
void tick_results(NormalRaceLifecycleWorld &w)
{ // 444390 /444070 /444300
    const auto primary = w.race.controls.controllers[0].vehicle;
    if (w.startup[primary].fields_768c[6] == 0)
        return;
    auto &f = fields(w);
    auto &p = w.presentation_state;
    w.selected_hud = 0;
    ++p.row_ticks;
    if (p.row_ticks > 60 && p.visible_rows < 6 && w.finish.order[p.visible_rows])
    {
        const auto slot = *w.finish.order[p.visible_rows];
        if (slot >= 6)
            throw FormatError("normal result vehicle identity");
        if (w.race.race[slot].active == 1)
        {
            auto h = hud_element(w.hud, 0, 12 + p.visible_rows);
            h.active = 1;
            result_row(w, p.visible_rows, slot);
            w.race.presentation.sound(47, 63);
            p.row_ticks = 0;
            ++p.visible_rows;
        }
    }
    if (p.visible_rows >= count_active_vehicles(w.race.race))
        ++f[7];
    if (++f[8] >= 60)
        f[8] = 0;
    for (unsigned e : {9u, 11u, 10u, 12u, 13u, 14u, 15u, 16u, 17u})
        update_hud_element(hud_element(w.hud, 0, e));
    if (w.race.clock.phase == 3 && f[10] == 0 && confirm(w))
    {
        w.race.presentation.sound(48, 255);
        f[9] = 0;
        f[10] = 1;
        all_overlay(w, 120, 0, 0, 60, 0, as_float(0x3f7ae148));
        w.audio.output_b_copy = 0;
        for (unsigned e : {9u, 10u, 11u})
            hud_phase(w, e, as_word(100));
        w.race.presentation.sound(46, 63);
    }
    if (f[10] != 0)
    {
        if ((f[10] & 7u) == 0)
        {
            hud_phase(w, 12 + f[9], as_word(100));
            if (f[9] < 6)
                ++f[9];
        }
        ++f[10];
        if (f[10] == 60)
            all_overlay(w, 60, 0, 240, 60, 100, as_float(0x3f7ae148));
        if (f[10] == 120)
            services(w).fade_music();
        if (f[10] == 180)
        {
            p.field_612bf0 = 1;
            p.menu_requested = true;
        }
    }
    if (f[10] == 0)
    {
        if (f[7] == 480)
        {
            hud_phase(w, 10, as_word(100));
            f[16] = 240;
        }
        if (f[16] != 0 && --f[16] == 120)
        {
            hud_phase(w, 10, 0);
            f[17] = (f[17] + 1u) % 3u;
            f[7] = 0;
        }
    }
}
} // namespace
void start_normal_race(NormalRaceLifecycleWorld &w)
{
    validate(w);
    if (w.camera_mode == 10)
        throw FormatError("normal authored camera10 activation requires its source adapter");
    w.phase_ticks = 0;
    arm_start_countdown(w.race.race);
    w.race.clock.game_mode = 2;
    w.race.clock.phase = 1;
}
void tick_normal_race_lifecycle(NormalRaceLifecycleWorld &w)
{
    validate(w);
    auto &race = w.race;
    auto &clock = race.clock;
    if (clock.countdown_pending != 0)
    {
        --clock.countdown_pending;
        if (std::bit_cast<std::int32_t>(clock.countdown_pending) <= 0)
        {
            start_normal_race(w);
            clock.countdown_pending = 0;
        }
    }
    if (clock.phase == 2)
    {
        const auto left = count_unfinished_vehicles(race.physics, race.race, w.hud_block);
        if (left.other == 0 && left.upgraded == 0)
            clock.phase = 3;
    }
    if (clock.phase == 3)
        for (unsigned i = 0; i < 6; ++i)
            disable_vehicle_control(race.race[i], race.physics[i]);
    if (clock.game_mode == 1)
        return;
    for (unsigned i = 0; i < 4; ++i)
    {
        auto &controller = race.controls.controllers[i];
        if (controller.active != 1)
            continue;
        race.current_slot = controller.vehicle;
        w.selected_hud = i;
        race.controls.current = i;
        tick_vehicle_notice(race);
        tick_finish(w);
        tick_race_countdown(race);
        if (tick_retirement(w))
            w.presentation_state.menu_requested = 1;
        tick_podium(w);
        tick_results(w);
    }
    race.controls.current = 0;
}
namespace
{
void validate_challenge(const ChallengeRaceLifecycleWorld &w)
{
    const auto &shared = w.shared;
    const auto &race = shared.race;
    if (shared.game_type != 3 || shared.split != 0 || (race.clock.game_mode != 1 && race.clock.game_mode != 3) ||
        race.clock.lap_count != 100)
        throw FormatError("challenge lifecycle requires local game_type3, mode1/3 and lap_count100");
    if (race.current_slot >= 6 || race.controls.current >= 4 ||
        race.controls.controllers[race.controls.current].vehicle >= 6)
        throw FormatError("challenge lifecycle identity outside source slots");
    if (&w.countdown.clock != &race.clock || &w.countdown.controls != &race.controls ||
        w.countdown.physical.data() != race.physics.data() || w.countdown.race.data() != race.race.data())
        throw FormatError("challenge lifecycle requires shared source owners");
}
} // namespace
void start_challenge_race(ChallengeRaceLifecycleWorld &w)
{
    validate_challenge(w);
    auto &race = w.shared.race;
    arm_start_countdown(race.race);
    race.clock.game_mode = 3;
    race.clock.phase = 1;
    w.countdown.score_block = 0;
    contact::player::post_notice(race.controls, 1, w.countdown.tick, 300, w.start_text, race.current_slot, 0, 0);
    auto &selectors = w.selectors[race.current_slot];
    selectors.field_7720 = selectors.field_771c = 0;
    w.services.start_camera();
}
void tick_challenge_race_lifecycle(ChallengeRaceLifecycleWorld &w)
{
    validate_challenge(w);
    auto &shared = w.shared;
    auto &race = shared.race;
    auto &clock = race.clock;
    if (clock.countdown_pending != 0)
    {
        --clock.countdown_pending;
        if (std::bit_cast<std::int32_t>(clock.countdown_pending) <= 0)
        {
            shared.phase_ticks = 0;
            start_challenge_race(w);
            clock.countdown_pending = 0;
        }
    }
    // 443780 is before the controller selects its vehicle. Its mode1 arm is
    // still reachable in the three real prestart world groups before443980.
    if (clock.game_mode == 1)
    {
        if (clock.phase == 2)
        {
            const auto remaining = count_unfinished_vehicles(race.physics, race.race, shared.hud_block);
            if (remaining.other == 0 && remaining.upgraded == 0)
            {
                clock.phase = 3;
                for (unsigned i = 0; i < 6; ++i)
                    disable_vehicle_control(race.race[i], race.physics[i]);
            }
        }
        return;
    }
    w.countdown.current = race.current_slot;
    tick_challenge_clock(w.countdown);
    race.current_slot = race.controls.controllers[race.controls.current].vehicle;
    w.load_selection.template_vehicle = race.current_slot;
    w.countdown.current = race.current_slot;
    tick_challenge_end_countdown(w.countdown);
    tick_race_countdown(race);
    if (tick_retirement(shared))
        w.services.request_menu();
    if (tick_finish(shared))
        w.services.request_menu();
    auto slot = race.controls.controllers[race.controls.current].vehicle;
    refill_vehicle(w.parameters[slot], race.physics[slot]);
    slot = race.controls.controllers[race.controls.current].vehicle;
    refill_impact_energy(race.physics[slot], w.contact[slot]);
}
} // namespace sgn::retail
