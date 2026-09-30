#include "sgn/source_race_lifecycle.hpp"
#include "sgn/source_score_factory.hpp"
#include "sgn/source_trial_start.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/retail_hud_lifetime.hpp"
#include "sgn/source_menu_sample_native.hpp"
#include "sgn/source_music_calls.hpp"
#include "sgn/race_session_lifecycle.hpp"
#include "sgn/race_session_scene.hpp"
#include "sgn/retail_pause_hud.hpp"
#include "sgn/retail_world.hpp"
#include "sgn/retail_time_trial_tick.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <algorithm>
#include <bit>
#include <cstring>

extern "C" float sgn_lifecycle_i32_float(std::uint32_t);
extern "C" std::uint32_t sgn_lifecycle_float_low(const float *);
extern "C" std::uint32_t sgn_lifecycle_rolling(const float *, const float *);
extern "C" std::uint32_t sgn_lifecycle_settle(std::uint32_t *);
extern "C" std::uint32_t sgn_lifecycle_spinner(std::uint32_t *);
extern "C" void sgn_lifecycle_finish_speed(const float *, std::uint32_t *);
extern "C" void sgn_lifecycle_all_overlay_rate(double *, float *);
extern "C" void sgn_lifecycle_results_hud(float *, std::uint32_t *, const std::uint32_t *);

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using Fields = std::array<W, 25>;
using sgn::bit_reinterpret::as_float;
[[noreturn]] void missing(W site, W child, const char *reason)
{
    throw SourceRaceLifecycleBoundary(site, child, reason);
}
class Lifecycle final
{
    FrontendState &frontend_;
    RaceSessionPlatform &platform_;
    RaceSessionState &w_;
    SourceMenuSampleNative &samples_;
    RaceTextureReplacement &textures_;
    W vehicle(W id, W site) const
    {
        if (id >= 6)
            missing(site, 0, "lifecycle reached vehicle outside actual six-slot source bank");
        return id;
    }
    W controller(W site) const
    {
        if (w_.controls.current >= 4)
            missing(site, 0, "lifecycle reached controller outside actual four-slot source bank");
        return w_.controls.current;
    }
    W current(W site) const
    {
        return vehicle(w_.current_slot, site);
    }
    W selected(W site) const
    {
        return vehicle(w_.controls.controllers[controller(site)].vehicle, site);
    }
    Fields &fields(W site)
    {
        return w_.vehicles.startup[current(site)].fields_768c;
    }
    RaceHudView hud()
    {
        return {w_.controls, w_.hud_countdown, w_.hud_layout, w_.selected_hud, w_.hud_margins};
    }
    void sound(W id, W volume, W frequency = 0, W force = 0)
    {
        samples_.start_461150(frontend_, id, 0, frequency, volume, 0, force, nullptr);
    }
    void music(W id, W loop)
    {
        start_source_music_45f8f0(platform_, id, loop);
    }
    void texture(W id)
    {
        if (w_.source_overlay_selection_61b634 != id)
            textures_.replace_source_texture(15, id);
    }
    void overlay(W hold, W duration, W flags, W value, W target, float rate)
    {
        set_overlay_transition(w_.overlays[controller(0x442ad0)], 2, hold, duration, flags, value, target, rate);
    }
    void hide_hud()
    {
        (void)controller(0x447ba0);
        hide_selected_pause_hud_447ba0(hud(), 0);
    }
    void set_mode(W mode)
    {
        auto &c = w_.controls.controllers[controller(0x4028d0)].camera;
        c.mode = mode;
        c.twist = 0;
        c.collision_bypass_ticks = 300;
        for (W i = 0; i < 6; ++i)
            if (w_.vehicles.race[i].active == 1)
                reset_camera_dependent_effects(w_.vehicles.physics[i]);
    }
    void notice(W source, GameTextId text, std::int32_t duration, W audience, W priority)
    {
        // 442CF0/442C80: no eager text dereference on phase3/no recipient/full bank.
        if (w_.clock.phase == 3)
            return;
        const auto add = [&](contact::player::Controller &c) {
            if (priority == 100)
                contact::player::clear_notices(c.notices);
            for (auto &n : c.notices)
                if (n.remaining == 0)
                {
                    n.remaining = duration;
                    n.initial = duration;
                    n.tick = w_.simulation_tick;
                    n.source_vehicle = source;
                    const auto bytes = w_.game_text.required(text); // source strlen starts at442CCA
                    const auto length = bytes.find('\0');
                    const auto text_bytes = bytes.substr(0, length);
                    if (text_bytes.size() >= n.text.size())
                        missing(0x442cca, 0, "notice text exceeds represented63-byte source field");
                    n.audience = static_cast<std::int32_t>(audience);
                    std::copy(text_bytes.begin(), text_bytes.end(), n.text.begin());
                    n.text[text_bytes.size()] = '\0';
                    return;
                }
        };
        if (audience == 2)
        {
            for (auto &c : w_.controls.controllers)
                if (c.active == 1 && c.vehicle == source)
                {
                    add(c);
                    return;
                }
            return;
        }
        if (audience > 1)
            return;
        for (auto &c : w_.controls.controllers)
            if (c.active == 1 && (audience == 0 || c.vehicle != source))
                add(c);
    }
    void disable()
    {
        for (W i = 0; i < 6; ++i)
            disable_vehicle_control(w_.vehicles.race[i], w_.vehicles.physics[i]);
    }
    void request_menu()
    {
        auto transition = scene_transition_world(w_);
        auto result = scene_race_result_world(w_);
        request_scene_transition(transition, result);
    }
    bool confirm()
    {
        auto &f = w_.normal_finish;
        if (f.confirm_consumed != 0 || f.confirm_pressed != 1)
            return false;
        f.confirm_consumed = 1;
        f.confirm_pressed = 1;
        return true;
    }
    void configure_drivers()
    { // 4058F0/405880, actual interleaved settings, no six-car copy
        for (W i = 0; i < 6; ++i)
            if (w_.vehicles.race[i].active == 1)
            {
                const auto skill = w_.cars[i].settings.ai_skill;
                w_.vehicles.ai[i].steering_ramp_ticks = 300;
                if (skill == 1)
                {
                    w_.vehicles.physics[i].upgrade_active = 0;
                    continue;
                }
                auto &d = w_.vehicles.drivers[i];
                d.skill = sgn_lifecycle_i32_float(skill);
                d.mode = sgn_lifecycle_i32_float(w_.cars[i].settings.ai_mode);
                w_.vehicles.physics[i].upgrade_active = 1;
                w_.vehicles.physics[i].torque_curve = d.skill > w_.gameplay.values[15]   ? 3
                                                      : d.skill > w_.gameplay.values[16] ? 2
                                                      : d.skill > w_.gameplay.values[17] ? 1
                                                                                         : 0;
            }
    }
    void start()
    { // 4439F0 including literal jump table443A20
        const auto type = w_.level.game_type;
        w_.trial_start.phase_ticks = 0;
        if (type == 4)
        {
            start_source_time_trial_443100(frontend_);
            return;
        }
        if (type != 2 && type != 3 && type != 5)
            return;
        arm_start_countdown(w_.vehicles.race);
        if (type == 3)
        {
            const auto id = w_.current_slot; // 443985 capture before subsequent mode stores
            w_.clock.game_mode = 3;
            w_.clock.phase = 1;
            w_.score_block = 0;
            notice(id, GameTextId::challenge_start, 300, 0, 0);
            const auto after = current(0x4439c7);
            w_.vehicles.audio[after].field_7720 = 0;
            w_.vehicles.audio[after].field_771c = 0;
            set_mode(7);
            auto &camera = w_.controls.controllers[controller(0x403316)].camera;
            camera_set_lens(camera, as_float(0x40233333), w_.shared_model_identity);
            w_.frame_projection = camera.projection;
            w_.inverse_depth_scale = camera.inverse_depth_scale;
            w_.depth_offset = camera.depth_offset;
            return;
        }
        const auto camera_mode = w_.level.camera_mode;
        w_.clock.game_mode = 2;
        w_.clock.phase = 1;
        if (camera_mode == 10)
        { // 4051E0, no camera reset/twist writes on this different path
            const auto c = controller(0x4051e0);
            auto &life = w_.camera_lifecycle[c];
            auto &camera = w_.controls.controllers[c].camera;
            life.next_mode = 10;
            camera.pending_follow_setup = 1;
            camera.mode = life.next_mode;
            camera_request_record(life, 0, 10);
        }
    }
    void completion()
    { // 443780
        if (w_.clock.phase != 2)
            return;
        const auto mode = w_.clock.game_mode;
        if (mode == 1 || mode == 2)
        {
            W upgraded = 0, other = 0;
            for (W i = 0; i < 6; ++i)
                if (w_.vehicles.race[i].active == 1 && w_.vehicles.physics[i].energy_exempt == 0)
                {
                    if (w_.vehicles.physics[i].upgrade_active == 1)
                        ++upgraded;
                    else
                        ++other;
                }
            if (w_.hud_block == 1)
                other = 1;
            if (other == 0 && upgraded == 0)
            {
                w_.clock.phase = 3;
                disable();
                return;
            }
            if (w_.clock.phase == 3)
                disable();
            return;
        }
        if (mode != 3 || w_.clock.remaining_ticks == 0)
            return;
        if (w_.clock.remaining_ticks != 1)
        {
            --w_.clock.remaining_ticks;
            return;
        }
        w_.vehicles.race[0].stunt_score = sgn_lifecycle_float_low(&w_.vehicles.physics[0].score);
        const auto id = w_.current_slot;
        w_.vehicles.race[0].best_lap_ticks = 0;
        const auto points = w_.vehicles.race[vehicle(id, 0x4437e9)].stunt_score;
        w_.vehicles.race[0].total_ticks = 0;
        create_source_score_popup_401220(frontend_, samples_, 5, 0, points); // 4437FF, complete factory on same owners
        const auto after = current(0x44380a);
        w_.vehicles.physics[after].score = sgn_lifecycle_i32_float(w_.vehicles.race[0].stunt_score);
        notice(after, GameTextId::challenge_finished, 300, 0, 100);
        w_.clock.remaining_ticks = 0;
        w_.score_block = 180;
        disable();
        if (w_.clock.phase == 3)
            disable();
    }
    void challenge_clock()
    { // 443890, wrapped DWORD arithmetic before unsigned division
        if (w_.score_block != 0 && --w_.score_block == 0)
        {
            w_.vehicles.physics[0].energy_exempt = 1;
            w_.clock.remaining_ticks = 0;
            w_.clock.phase = 3;
            return;
        }
        auto remaining = w_.clock.remaining_ticks;
        if (remaining == 0 || remaining > 600)
            return;
        if (remaining == 600)
        {
            notice(w_.current_slot, GameTextId::challenge_hurry, 180, 2, 0);
            remaining = w_.clock.remaining_ticks;
        }
        if (remaining % 60 == 0)
            sound(22, 255u - (remaining * 10u) / 60u, 27050u - (remaining * 500u) / 60u, 1);
    }
    void vehicle_notice()
    { // 443E20: fresh global selection after each actual native call
        if (w_.vehicles.race[current(0x443e20)].notice_active == 0)
            return;
        if (w_.vehicles.race[current(0x443e2f)].notice_ticks == 0)
            sound(47, 127);
        texture(20);
        auto &r = w_.vehicles.race[current(0x443e7b)];
        if (++r.notice_ticks > 120)
            r.notice_active = 0;
    }
    void countdown()
    { // 444C50
        w_.current_slot = selected(0x444c55);
        auto &r = w_.vehicles.race[current(0x444c61)];
        if (r.start_countdown == 0)
            return;
        if (--r.start_countdown > 60)
            hold_start_vehicles(w_.vehicles.physics, w_.vehicles.race, w_.clock);
        const auto count = w_.vehicles.race[current(0x444c86)].start_countdown;
        if (count > 180)
        {
            if (count == 240)
                sound(44, 255);
            texture(19);
            return;
        }
        if (count > 120)
        {
            if (count == 180)
                sound(44, 255);
            texture(18);
            return;
        }
        if (count > 60)
        {
            if (count == 120)
                sound(44, 255);
            texture(17);
            return;
        }
        if (count == 0)
            return;
        texture(16);
        if (w_.vehicles.race[current(0x444d6d)].start_countdown == 60)
        {
            sound(45, 255);
            enable_all_controls(w_.vehicles.race);
            configure_drivers();
            w_.clock.phase = 2;
            begin_lap_timers(w_.vehicles.race);
            w_.vehicles.race[selected(0x444d9f)].automatic_drive = 0;
        }
    }
    bool all_finished()
    { // 444670, zero active slots compares equal
        W active = 0, done = 0;
        for (W i = 0; i < 6; ++i)
            if (w_.vehicles.race[i].active == 1)
            {
                ++active;
                const auto &p = w_.vehicles.physics[i];
                const auto &f = w_.vehicles.startup[i].fields_768c;
                if ((p.energy_exempt == 1 && f[18] == 0 && f[11] == 0) || p.upgrade_active == 1 ||
                    w_.vehicles.race[i].stopped == 1)
                    ++done;
            }
        return active == done;
    }
    void begin_results()
    { // 443EA0, primary vehicle is reread after sample
        const auto first = vehicle(w_.controls.controllers[0].vehicle, 0x443ea5);
        if (w_.vehicles.startup[first].fields_768c[6] == 1)
            return;
        sound(47, 63);
        const auto after = vehicle(w_.controls.controllers[0].vehicle, 0x443ec4);
        auto &s = w_.normal_finish;
        s.row_ticks = 0;
        s.visible_rows = 0;
        s.field_60d728 = 0;
        w_.selected_hud = 0;
        w_.vehicles.startup[after].fields_768c[6] = 1;
        initialize_selected_hud_ranking(hud());
    }
    void begin_podium()
    {
        auto &f = fields(0x4443c0);
        f[11] = 1;
        f[12] = 0;
        f[13] = 0xc4160000;
        f[14] = 0;
        f[15] = 0xbf800000;
        sound(46, 63);
    }
    void begin_finish()
    { // 444590 includes split-screen branch
        if (w_.split_setting == 0)
        {
            w_.startup.field_60f2c0 = 1;
            music(w_.vehicles.race[current(0x4445a4)].finish_state <= 2 ? 21u : 22u, 0);
        }
        const auto id = current(0x4445c8);
        auto &f = w_.vehicles.startup[id].fields_768c;
        f[18] = 1;
        f[19] = 1;
        f[20] = 0;
        f[21] = 0;
        sgn_lifecycle_finish_speed(&w_.vehicles.physics[id].speed, &f[22]); // stores f23 between FSTP and clamp
        hide_hud();
        set_mode(10);
        bool complete = true;
        if (w_.split_setting != 0)
            for (W i = 0; i < 6; ++i)
                if (w_.vehicles.race[i].active == 1 && w_.vehicles.physics[i].upgrade_active == 0 &&
                    w_.vehicles.physics[i].energy_exempt != 1)
                {
                    complete = false;
                    break;
                }
        if (complete)
            w_.startup.field_60f2c0 = 1;
    }
    void finish()
    { // 4446D0; no startup references retained across native receivers
        auto id = current(0x4446d0);
        if (w_.vehicles.startup[id].fields_768c[6] == 1 || w_.vehicles.startup[id].fields_768c[11] == 1)
            return;
        if (w_.vehicles.startup[id].fields_768c[24] == 1)
        {
            auto &counter = w_.vehicles.startup[id].fields_768c[8];
            if (++counter >= 60)
                counter = 0;
            return;
        }
        if (w_.vehicles.physics[id].energy_exempt == 1 && w_.vehicles.startup[id].fields_768c[18] == 0)
        {
            begin_finish();
            id = current(0x44473b);
        }
        if (w_.vehicles.startup[id].fields_768c[18] == 0)
            return;
        if (w_.vehicles.startup[id].fields_768c[19] != 0)
        {
            if (w_.clock.game_mode != 3 && w_.vehicles.startup[id].fields_768c[19] == 60)
            {
                const auto rank = w_.vehicles.race[id].finish_state;
                if (rank <= 2)
                {
                    sound(21u - rank, 255);
                    id = current(0x4447b5);
                }
            }
            auto &f = w_.vehicles.startup[id].fields_768c;
            ++f[19];
            const auto emitted = sgn_lifecycle_spinner(&f[21]); // low byte sound, bit8 settled
            if ((emitted & 0x100) != 0 && f[19] > 360 && f[20] == 0)
            {
                f[20] = 1;
                overlay(240, 0, 0, 0, 60, as_float(0x3f75c28f));
                id = current(0x4448a6);
            }
            if ((emitted & 0xff) != 0)
            {
                sound(emitted & 0xff, 127);
                id = current(0x4448bf);
            }
            if (w_.vehicles.startup[id].fields_768c[19] > 30 && confirm())
            {
                sound(48, 255);
                id = current(0x4448fb);
                if (w_.vehicles.startup[id].fields_768c[20] == 0)
                {
                    overlay(240, 0, 0, 0, 60, as_float(0x3f75c28f));
                    id = current(0x444922);
                }
                w_.vehicles.startup[id].fields_768c[19] = 0;
                w_.vehicles.startup[id].fields_768c[20] = 480;
            }
            auto &f2 = w_.vehicles.startup[id].fields_768c;
            if (f2[20] != 0)
            {
                const auto age = ++f2[20];
                if ((age >= 120 && w_.audio_stream.playback_active == 0) || age >= 480)
                {
                    if (w_.level.game_type == 3)
                    {
                        request_menu();
                        return;
                    }
                    f2[18] = 0;
                    f2[19] = 0;
                    f2[20] = 0;
                    f2[6] = 0;
                    if (w_.split_setting == 0)
                    {
                        if (w_.vehicles.race[id].finish_state <= 2)
                            begin_podium();
                        else
                            begin_results();
                        music(23, 1);
                    }
                    else
                        f2[24] = 1;
                }
            }
        }
        if (w_.split_setting != 0 && all_finished())
            begin_results();
    }
    void begin_retirement()
    { // 4449E0
        if (fields(0x4449e0)[0] == 1)
            return;
        textures_.replace_source_texture(15, 25);
        auto &f = fields(0x444a06);
        f[1] = 1;
        f[0] = 1;
        f[5] = 0;
        f[2] = 0xc3800000;
        f[3] = 0;
        f[4] = 0xbf800000;
        hide_hud();
        overlay(60, 0, 0, 0, 60, as_float(0x3f75c28f));
        set_mode(10);
        if (w_.split_setting == 0)
        {
            music(20, 0);
            w_.audio_level.output_b_copy = 0;
        }
        sound(47, 63, 0x4000);
    }
    void retirement()
    { // 444AE0 retains ESI vehicle identity over all callbacks
        const auto saved = current(0x444ae0);
        if (w_.vehicles.race[saved].stopped == 1 && w_.vehicles.startup[saved].fields_768c[0] == 0)
            begin_retirement();
        bool all = false;
        if (w_.hud_block != 1)
        {
            W active = 0, done = 0;
            for (W i = 0; i < 6; ++i)
                if (w_.vehicles.race[i].active == 1)
                {
                    ++active;
                    if (w_.vehicles.physics[i].upgrade_active == 1 || w_.vehicles.startup[i].fields_768c[5] == 1)
                        ++done;
                }
            all = active == done;
        }
        if (all && w_.startup_counters.field_60d71c == 0)
        {
            w_.startup_counters.field_60d71c = 1;
            w_.normal_finish.field_612bf0 = 1;
            request_menu();
        }
        auto &f = w_.vehicles.startup[saved].fields_768c;
        if (f[0] == 0 || w_.startup_counters.field_60d71c == 1)
            return;
        const auto age = ++f[1];
        if ((age >= 540 && w_.audio_stream.playback_active == 0) || age >= 900)
        {
            f[5] = 1;
            return;
        }
        if (sgn_lifecycle_settle(&f[2]) != 0 && age == 420)
            overlay(120, 0, 300, 60, 100, 1);
    }
    void podium()
    { // 444410
        auto id = current(0x444410);
        if (w_.vehicles.startup[id].fields_768c[11] == 0)
            return;
        const auto texture_id = w_.vehicles.race[id].finish_state + 21u;
        if (w_.source_overlay_selection_61b634 != texture_id)
        {
            textures_.replace_source_texture(15, texture_id);
            id = current(0x444441);
        }
        if (w_.vehicles.startup[id].fields_768c[12] > 30 && confirm())
        {
            sound(48, 255);
            id = current(0x44447b);
            w_.vehicles.startup[id].fields_768c[11] = 0;
            w_.vehicles.startup[id].fields_768c[12] = 0;
            begin_results();
            id = current(0x444491);
        }
        auto &f = w_.vehicles.startup[id].fields_768c;
        if (sgn_lifecycle_settle(&f[13]) != 0 && f[12] >= 240)
        {
            f[11] = 0;
            f[12] = 0;
            begin_results();
            id = current(0x444506);
        }
        ++w_.vehicles.startup[id].fields_768c[12];
    }
    void result_row(W row, W id)
    { // 443F10, EDI/EBX identities stay captured across46280C.
        if (row >= 6)
            missing(0x444041, 0, "result row outside the six source pointer slots");
        id = vehicle(id, 0x443f19);
        auto &name = w_.cars[id].name;
        const auto digits = [](W ticks) { // 4482A0; first multiply wraps before division.
            std::array<W, 8> d{};
            d[0] = ((ticks * 10u) / 6u) % 10u;
            d[1] = (ticks / 6u) % 10u;
            const auto seconds = ticks / 60u, hours = seconds / 3600u, within = seconds % 3600u;
            d[6] = hours % 10u;
            d[7] = hours / 10u;
            const auto minutes = within / 60u;
            d[4] = minutes % 10u;
            d[5] = minutes / 10u;
            d[2] = (within % 60u) % 10u;
            d[3] = (within % 60u) / 10u;
            return d;
        };
        const auto app = platform_.source_application_literals();
        const auto format = [&](W address, std::span<const W> arguments, W offset, W site) {
            SourceScriptPathCall frame;
            auto output =
                std::span<std::uint8_t>(reinterpret_cast<std::uint8_t *>(name.data() + offset), name.size() - offset);
            (void)format_race_lifecycle_46280c(w_.boot_config, app, address, arguments, output, site, frame);
        };
        const auto time = [&](W ticks, bool six, W format_six, W format_five, W offset, W site_six, W site_five) {
            const auto d = digits(ticks);
            const std::array<W, 6> args{d[5], d[4], d[3], d[2], d[1], d[0]};
            format(six ? format_six : format_five, std::span<const W>(args).subspan(six ? 0u : 1u), offset,
                   six ? site_six : site_five);
        };
        const auto race = w_.vehicles.race[id].race_timer;
        time(std::bit_cast<W>(race), race >= 36000, 0x482b44, 0x482b54, 16, 0x443f88, 0x443fb8); // signed JL
        const auto best = w_.vehicles.race[id].best_lap_ticks;
        time(best, best >= 36000u, 0x482b64, 0x482b74, 32, 0x444002, 0x444012); // unsigned JC
        const std::array<W, 1> score{sgn_lifecycle_float_low(&w_.vehicles.physics[id].score)};
        format(0x482b84, score, 48, 0x444032);
        auto &pointers = w_.normal_finish.result_rows.words;
        const auto base = 0x60f050u + id * 100u;
        pointers[18 + row] = base + 48;
        pointers[row] = base;
        pointers[6 + row] = base + 16;
        pointers[12 + row] = base + 32;
    }
    void result_phase(W element, W phase)
    { // 447A90/447AA0 also reach the source notice alias.
        if (w_.selected_hud >= 4)
            missing(0x447a90, 0, "results selected HUD outside four source owners");
        if (element < 18)
        {
            auto e = hud_element(hud(), w_.selected_hud, element);
            e.active = 1;
            e.phase = phase;
            return;
        }
        if (element == 18)
        {
            auto &n = w_.controls.controllers[w_.selected_hud].notices[0];
            n.initial = 1;
            std::memcpy(n.text.data(), &phase, 4);
            return;
        } // HUD+298 aliases first notice.
        missing(0x44426c, 0x447a90, "results fade cursor exceeds the reached HUD/notice source field domain");
    }
    void result_hud(W identity, W element)
    {
        auto e = hud_element(hud(), identity, element);
        sgn_lifecycle_results_hud(&e.value, &e.active, &e.phase);
    }
    void all_overlay(W hold, W duration, W flags, W value, W target, float rate)
    {                               // 442A40, selected controller left at3.
        double carried_rate = rate; // 442A46 loads once; each active controller subtracts from the same ST0.
        for (W i = 0; i < 4; ++i)
        {
            w_.controls.current = i;
            if (w_.controls.controllers[i].active != 1)
                continue;
            auto &s = w_.overlays[i];
            s.active = 1;
            s.kind = 2;
            s.initial_value = value;
            s.value = value;
            s.target = target;
            s.age = 0;
            s.hold_ticks = hold;
            s.duration = duration;
            s.flags = flags;
            sgn_lifecycle_all_overlay_rate(&carried_rate, &s.rate);
        }
    }
    void result_updates()
    { // 444070; source global selections reload after every synchronous native call.
        const auto ticks = w_.normal_finish.row_ticks + 1u;
        w_.selected_hud = 0;
        w_.normal_finish.row_ticks = ticks;
        if (ticks > 60u && w_.normal_finish.visible_rows < 6u)
        {
            const auto row = w_.normal_finish.visible_rows;
            const auto pointer = w_.finish.order[row];
            if (pointer)
            {
                const auto id = vehicle(*pointer, 0x4440ad);
                if (w_.vehicles.race[id].active == 1)
                {
                    auto e = hud_element(hud(), 0, 12u + row);
                    e.active = 1;
                    result_row(row, id);
                    sound(47, 63);
                    const auto next = w_.normal_finish.visible_rows + 1u;
                    w_.normal_finish.row_ticks = 0;
                    w_.normal_finish.visible_rows = next;
                }
            }
        }
        W active = 0;
        for (const auto &r : w_.vehicles.race)
            if (r.active == 1)
                ++active;
        auto id = current(0x4440fa);
        if (w_.normal_finish.visible_rows >= active)
            ++w_.vehicles.startup[id].fields_768c[7];
        auto &blink = w_.vehicles.startup[id].fields_768c[8];
        if (++blink >= 60u)
            blink = 0;
        result_hud(w_.selected_hud, 9);
        result_hud(w_.selected_hud, 11);
        result_hud(w_.selected_hud, 10);
        const auto selected_hud = w_.selected_hud;
        for (W row = 0; row < 6; ++row)
            result_hud(selected_hud, 12 + row);
        if (w_.clock.phase == 3 && w_.normal_finish.confirm_consumed == 0 && w_.normal_finish.confirm_pressed == 1)
        {
            id = current(0x4441a0);
            if (w_.vehicles.startup[id].fields_768c[10] == 0)
            {
                (void)confirm();
                sound(48, 255);
                id = current(0x4441cb);
                w_.vehicles.startup[id].fields_768c[9] = 0;
                w_.vehicles.startup[id].fields_768c[10] = 1;
                all_overlay(120, 0, 0, 60, 0, as_float(0x3f7ae148));
                w_.audio_level.output_b_copy = 0;
                result_phase(9, 0x42c80000);
                result_phase(10, 0x42c80000);
                result_phase(11, 0x42c80000);
                sound(46, 63);
                id = current(0x44423e);
            }
        }
        else
            id = current(0x44423e);
        if (w_.vehicles.startup[id].fields_768c[10] == 0)
            return;
        if ((w_.vehicles.startup[id].fields_768c[10] & 7u) == 0)
        {
            result_phase(12u + w_.vehicles.startup[id].fields_768c[9], 0x42c80000);
            id = current(0x444271);
            auto &cursor = w_.vehicles.startup[id].fields_768c[9];
            if (cursor < 6u)
                ++cursor;
        }
        ++w_.vehicles.startup[id].fields_768c[10];
        if (w_.vehicles.startup[id].fields_768c[10] == 60)
        {
            all_overlay(60, 0, 240, 60, 100, as_float(0x3f7ae148));
            id = current(0x4442bc);
        }
        if (w_.vehicles.startup[id].fields_768c[10] == 120)
        {
            w_.audio_stream.field_e100 = 0;
            w_.audio_maintenance.music.gain_step = 0xfffffff6u;
            id = current(0x4442d6);
        }
        if (w_.vehicles.startup[id].fields_768c[10] == 180)
        {
            w_.normal_finish.field_612bf0 = 1;
            request_menu();
        }
    }
    void result_rotation()
    { // 444300, fresh current selection after each HUD child.
        auto id = current(0x444300);
        if (w_.vehicles.startup[id].fields_768c[10] != 0)
            return;
        if (w_.vehicles.startup[id].fields_768c[7] == 480)
        {
            result_phase(10, 0x42c80000);
            id = current(0x44432b);
            w_.vehicles.startup[id].fields_768c[16] = 240;
        }
        auto &countdown = w_.vehicles.startup[id].fields_768c[16];
        if (countdown == 0 || --countdown != 120)
            return;
        result_phase(10, 0);
        id = current(0x444362);
        auto &f = w_.vehicles.startup[id].fields_768c;
        f[17] = (f[17] + 1u) % 3u;
        f[7] = 0;
    }
    void results()
    { // 444390
        const auto primary = vehicle(w_.controls.controllers[0].vehicle, 0x444395);
        if (w_.vehicles.startup[primary].fields_768c[6] == 0)
            return;
        w_.selected_hud = 0;
        result_updates();
        result_rotation();
    }
    W replay_ticks(ReplayHandle handle) const
    {
        const auto at = ReplayBank::base(handle) + 0x1cu;
        W result;
        std::memcpy(&result, w_.replays.bytes().data() + at, sizeof(result));
        return result;
    }
    void reference_ghost(W id)
    { // 443360, EDI is the original wrapped signed/unsigned delta
        if (w_.trial_selection.availability[1] != 1 || w_.ghosts.reference_completed == 1)
            return;
        auto &reference = w_.ghosts.roles[0];
        const auto delta = std::bit_cast<W>(w_.vehicles.race[id].split_timer) - replay_ticks(reference.replay);
        if ((reference.active == 1 || std::bit_cast<std::int32_t>(delta) < 0) && w_.ghosts.reference_completed == 0 &&
            w_.vehicles.progression[id].record_improved == 1)
        {
            reference.active = 1;
            w_.ghosts.reference_completed = 1;
            w_.vehicles.progression[id].record_improved = 0;
            notice(id, GameTextId::you_have_beaten_the_goldenghost, 300, 0, 100);
        }
        if (reference.active == 0 && delta < (replay_ticks(reference.replay) * 10u) / 100u)
        {
            reference.active = 1;
            notice(id, GameTextId::beat_the_laprecord_ghost, 300, 0, 100);
            w_.trial_selection.availability[0] = 0;
        }
    }
    void personal_ghost(W id)
    { // 443430, unsigned split/header comparison
        if (w_.trial_selection.availability[0] != 1 || w_.trial_selection.availability[2] == 1)
            return;
        if (std::bit_cast<W>(w_.vehicles.race[id].split_timer) >= replay_ticks(w_.ghosts.roles[1].replay))
            return;
        w_.trial_selection.availability[2] = 1;
        notice(id, GameTextId::you_have_beaten_your_userghost, 300, 0, 90);
    }
    void retain_trial()
    { // 443480: fixed physical car0, independent of current controller
        const auto recording = w_.vehicles.race[0].recording;
        if (!recording || w_.replays.status(*recording) != 2)
            return;
        stop_replay(w_.replays, *recording);
        const auto role_index = select_replacement_ghost(w_.ghosts);
        if (!role_index)
            missing(0x4434a0, 0, "443300 produced null source replacement pointer");
        auto &role = w_.ghosts.roles[*role_index];
        const auto ticks = std::bit_cast<W>(w_.vehicles.race[0].split_timer);
        if (ticks >= role.lap_ticks)
            return;
        const auto source = ReplayBank::base(*recording), destination = ReplayBank::base(role.replay);
        // Represented source replay identities are whole fixed slots: disjoint or identical.
        // That domain makes this exact6740-DWORD forward REP copy also a native memcpy.
        if (destination != source)
            std::memcpy(w_.replays.bytes().data() + destination, w_.replays.bytes().data() + source,
                        ReplayBank::stride);
        role.lap_ticks = ticks;
        role.active = 1;
        std::memcpy(w_.replays.bytes().data() + destination + 0x1c, &ticks, sizeof(ticks));
        const auto ghost_vehicle = vehicle(role.vehicle, 0x4434ce);
        finalize_replay_header(w_.replays, role.replay, w_.cars[ghost_vehicle]);
        reference_ghost(0);
        personal_ghost(0);
    }
    void ghosts()
    { // 443630 captures selected vehicle before announcement
        const auto saved = selected(0x443637);
        if (w_.trial_start.announcement_ticks != 0 && --w_.trial_start.announcement_ticks == 0)
            notice(w_.current_slot, GameTextId::timetrial_start, 180, 2, 0);
        const bool crossing = w_.vehicles.race[saved].progress_crossing == 1;
        if (crossing)
        {
            retain_trial();
            const auto handle = w_.vehicles.race[saved].recording;
            if (!handle)
                missing(0x41ab4c, 0, "crossing reached null source recording pointer");
            begin_recording(w_.replays, *handle, w_.vehicles.race[saved]);
        }
        select_active_ghosts(w_.ghosts); // 443590 exact role0/role1 and unsigned7200 gates
        for (auto &role : w_.ghosts.roles)
        {
            if (role.active == 1 && (crossing || w_.replays.status(role.replay) == 0))
            {
                const auto id = vehicle(role.vehicle, crossing ? 0x443547 : 0x44350d);
                begin_playback(w_.replays, role.replay, w_.vehicles.race[id], w_.vehicles.physics[id]);
            }
            w_.vehicles.race[vehicle(role.vehicle, crossing ? 0x443555 : 0x44351b)].active =
                std::bit_cast<std::int32_t>(role.active);
        }
        if (crossing)
        {
            w_.vehicles.race[saved].race_index = 1;
            if (w_.trial_start.reference_at_start == 1 && w_.vehicles.race[saved].lap_index < 1)
            {
                notice(saved, GameTextId::beat_the_laprecord_ghost, 300, 0, 100);
                w_.trial_selection.availability[3] = 1;
            }
        }
        for (const auto &role : w_.ghosts.roles)
            if (role.active == 1)
                w_.vehicles.race[vehicle(role.vehicle, 0x443578)].collision_hold = 60;
    }
    void refill()
    {
        auto id = selected(0x443b4a);
        refill_vehicle(w_.vehicles.parameters[id], w_.vehicles.physics[id]);
        id = selected(0x443b5b);
        refill_impact_energy(w_.vehicles.physics[id], w_.vehicles.contact[id]);
    }

  public:
    Lifecycle(FrontendState &f, SourceMenuSampleNative &a, RaceTextureReplacement &t)
        : frontend_(f), platform_(*f.shared_platform()), w_(platform_.state()), samples_(a), textures_(t)
    {
    }
    void run()
    {
        if (w_.clock.countdown_pending != 0)
        {
            --w_.clock.countdown_pending;
            if (std::bit_cast<std::int32_t>(w_.clock.countdown_pending) <= 0)
            {
                start();
                w_.clock.countdown_pending = 0;
            }
        }
        completion(); // mode is reread after443780, not a captured entry predicate
        switch (w_.clock.game_mode)
        {
        case 2:
        case 5:
            for (W i = 0; i < 4; ++i)
                if (w_.controls.controllers[i].active == 1)
                {
                    const auto selected_vehicle = w_.controls.controllers[i].vehicle;
                    w_.controls.current = i;
                    w_.current_slot = selected_vehicle;
                    w_.selected_hud = i;
                    vehicle_notice();
                    finish();
                    countdown();
                    retirement();
                    podium();
                    results();
                }
            w_.controls.current = 0;
            return;
        case 3: {
            const auto id = selected(0x443b21);
            w_.load_selection.template_vehicle = id;
            w_.current_slot = id;
            challenge_clock();
            countdown();
            retirement();
            finish();
            refill();
            return;
        }
        case 4: {
            const auto id = selected(0x443b6c);
            w_.load_selection.template_vehicle = id;
            w_.current_slot = id;
            ghosts();
            vehicle_notice();
            countdown();
            if (w_.clock.rolling_state != 480 &&
                sgn_lifecycle_rolling(&w_.clock.course_length, &w_.vehicles.race[current(0x443a49)].progress) != 0)
            {
                w_.clock.rolling_state = 480;
                arm_start_countdown(w_.vehicles.race);
            }
            refill();
            return;
        }
        default:
            return;
        }
    }
};
} // namespace
void advance_source_race_lifecycle_443a80(FrontendState &frontend, SourceMenuSampleNative &samples,
                                          RaceTextureReplacement &textures)
{
    const auto p = frontend.shared_platform();
    p->require_owner_thread();
    if (!p->source_construction_only_entry() || &frontend.world() != &p->state())
        throw FormatError("443A80 requires the same source platform/frontend world");
    Lifecycle calls(frontend, samples, textures);
    calls.run();
}
} // namespace sgn::retail
