#include "sgn/source_trial_start.hpp"
#include "sgn/source_race_lifecycle.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_time_trial.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
extern "C" float sgn_lifecycle_i32_float(std::uint32_t);
extern "C" float sgn_trial_distance(std::uint32_t, const float *);
extern "C" std::uint32_t sgn_trial_quotient(std::uint32_t, const float *, const float *, float *);
extern "C" std::uint32_t sgn_trial_special_offset(std::uint32_t, const float *);
extern "C" void sgn_trial_normalize(float *);
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_float;
float f32(double x)
{
    return static_cast<float>(x);
}
bool c0(double a, double b)
{
    return a < b || std::isnan(a) || std::isnan(b);
}
bool c3(double a, double b)
{
    return a == b || std::isnan(a) || std::isnan(b);
}
[[noreturn]] void gap(W site, W child, const char *reason)
{
    throw SourceRaceLifecycleBoundary(site, child, reason);
}
W vehicle(const RaceSessionState &w, W site)
{
    if (w.current_slot >= 6)
        gap(site, 0, "trial current vehicle outside source six-slot bank");
    return w.current_slot;
}
W controller(const RaceSessionState &w, W site)
{
    if (w.controls.current >= 4)
        gap(site, 0, "trial current controller outside source four-slot bank");
    return w.controls.current;
}
void nonzero(W divisor, W site)
{
    if (divisor == 0)
        gap(site, 0, "trial source unsigned DIV by zero");
}
void normalize(VehicleVector &v)
{
    sgn_trial_normalize(&v.x);
}
class TrialStart final
{
    std::weak_ptr<RaceSessionPlatform> borrow_;
    RaceSessionPlatform &platform_;
    RaceSessionState &w_;
    SourceCameraSplineRef descriptor(W site)
    {
        const auto &t = platform_.source_scene_resources().models().source_loading_state().track;
        (void)site;
        return SourceCameraSplineRef(borrow_, t.splines.allocation, t.splines.offset);
    }
    static float point(const SourceCameraSplineRef &p, W index, W lane, W site)
    {
        return as_float(p.read_word(std::uint64_t(index) * 16u + lane, site));
    }
    void named_notice(W id, W language, W format, W return_site)
    { // 418660/419180 ->46280C ->442CF0
        const auto &names = w_.cars[id].name;
        const auto offset = w_.game_text.offsets[language];
        std::optional<std::span<const std::uint8_t>> text;
        if (offset)
        {
            const auto n = std::min<std::size_t>(*offset, w_.game_text.arena.size());
            text = std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(w_.game_text.arena.data() + n),
                                                 w_.game_text.arena.size() - n);
        }
        std::array<std::uint8_t, 64> output;
        SourceScriptPathCall frame;
        format_respawn_notice_46280c(w_.boot_config, platform_.source_application_literals(), format,
                                     {reinterpret_cast<const std::uint8_t *>(names.data()), names.size()}, text, output,
                                     return_site, frame);
        const auto source = return_site == 0x4191c7 ? w_.current_slot : id; // 4191C7 rereads the original global.
        if (w_.clock.phase == 3)
            return;
        for (auto &c : w_.controls.controllers)
            if (c.active == 1 && c.vehicle != source)
            {
                for (auto &n : c.notices)
                    if (n.remaining == 0)
                    {
                        n.remaining = 180;
                        n.initial = 180;
                        n.tick = w_.simulation_tick;
                        n.source_vehicle = source;
                        // The formatter has already written its terminator.442C80 scans only for an admitted recipient.
                        const auto length = std::find(output.begin(), output.end(), std::uint8_t{0}) - output.begin();
                        if (length >= 64)
                            gap(0x442cca, 0, "trial notice exceeds represented64-byte source stack local");
                        n.audience = 1;
                        std::memcpy(n.text.data(), output.data(), static_cast<std::size_t>(length) + 1u);
                        break;
                    }
            }
    }
    void check_retirement()
    { // 4187A0 gates mode3;418660 remains represented if mode changes in future ownership routes.
        if (w_.clock.game_mode != 3)
            return;
        auto id = vehicle(w_, 0x4187a9);
        const auto &s = w_.vehicles.physics[id];
        if (!(c0(s.energy, 0) || s.energy == 0) || !c0(std::abs(double(s.speed)), as_float(0x3fd55555)))
            return;
        id = vehicle(w_, 0x4186c0);
        auto &r = w_.vehicles.race[id];
        if (w_.vehicles.physics[id].energy_exempt != 0 || r.stopped != 0)
            return;
        r.stopped = 1;
        r.finish_state = 0;
        disable_vehicle_control(r, w_.vehicles.physics[id]);
        if (w_.clock.game_mode == 2)
            named_notice(vehicle(w_, 0x4186fc), 17, 0x474668, 0x4186a4);
    }
    void prepare_special(W id)
    { // 418D20, descriptor+24 is adjacent data, NOT point_count+8.
        auto &r = w_.vehicles.race[id];
        const auto saved = w_.vehicles.contact[id].surface.saved_24a4;
        if (saved == 0)
            return;
        const auto count = descriptor(0x418d34).read_word(0x24, 0x418d40);
        r.spline_index = saved - sgn_trial_special_offset(count, &w_.clock.course_length);
        if (w_.reset_globals.transition_active == 1)
        {
            auto &transition = w_.controls.controllers[0].respawn_transition;
            transition.state = 1;
            transition.phase = 0;
            w_.reset_globals.transition_active = 0;
        }
    }
    void end_special(W id)
    { // 418D90 captures old global; restores only after the reached complete chain.
        auto &r = w_.vehicles.race[id];
        const auto saved = w_.current_slot;
        r.timing26a8 = 1;
        w_.current_slot = id;
        if (w_.vehicles.physics[id].special_mode == 1)
        {
            auto &contact = w_.vehicles.contact[id];
            contact.surface.saved_24a4 = 0;
            w_.vehicles.physics[id].special_mode = 0;
            r.control_enabled = std::bit_cast<std::int32_t>(contact.surface.saved_2278);
            const auto found = contact::player::find_controller(w_.controls, id);
            w_.controls.current = found.value_or(0xffffffffu);
            if (found)
            {
                auto &camera = w_.controls.controllers[*found].camera;
                const auto mode = w_.vehicles.race[vehicle(w_, 0x418de0)].saved_camera_mode;
                if (camera.mode != mode)
                {
                    camera.mode = mode;
                    camera.twist = 0;
                    camera.collision_bypass_ticks = 300;
                    for (W i = 0; i < 6; ++i)
                        if (w_.vehicles.race[i].active == 1)
                            reset_camera_dependent_effects(w_.vehicles.physics[i]);
                }
            }
            contact::player::restore_primary(w_.controls);
            named_notice(vehicle(w_, 0x41918b), 23, 0x47468c, 0x4191c7);
            w_.reset_globals.special_timer = 0;
            w_.reset_globals.special_value = 0;
        }
        w_.current_slot = saved;
    }
    void mask_respawn(W id)
    { // 4127E0 borrows624474, deliberately distinct from571328.
        const auto spline = descriptor(0x41299c);
        auto &r = w_.vehicles.race[id];
        W selected = r.spline_index, advanced = 0;
        // No preflight of mask or entire spline. Each reached source read can retain earlier mutations.
        for (std::uint64_t attempts = 0;; ++attempts)
        {
            const auto count = spline.read_word(8, 0x4127f8);
            nonzero(count, 0x412805);
            selected = std::bit_cast<std::int32_t>(selected) < 0 ? count - ((0u - selected) % count) : selected % count;
            const auto mask_index = (selected * 127u) / count;
            if (mask_index >= w_.curves.respawn.size())
                gap(0x412820, 0, "respawn mask index outside actual128-word owner");
            if (c3(w_.curves.respawn[mask_index], 1.0))
                break;
            ++selected;
            ++advanced;
            if (attempts >= std::uint64_t(count) + 1u)
                gap(0x41283c, 0x4127e0, "source respawn search cycles without an accepted mask entry");
        }
        const auto points = spline.at_normalized_offset(spline.read_word(4, 0x41283e));
        r.spline_index += advanced;
        r.respawn_position.x = point(points, selected, 0, 0x412864);
        r.respawn_position.y = point(points, selected, 4, 0x412869);
        r.respawn_position.z = point(points, selected, 8, 0x41286f);
        r.respawn_position.w = point(points, selected, 12, 0x412875);
        // Original next point is selected+1 WITHOUT modulo; many files retain a closing duplicate.
        VehicleVector heading;
        const auto nx = point(points, selected + 1u, 0, 0x412881), cx = point(points, selected, 0, 0x412884);
        heading.x = f32(double(nx) - cx);
        const auto ny = point(points, selected + 1u, 4, 0x41288c), cy = point(points, selected, 4, 0x41288f);
        heading.y = f32(double(ny) - cy);
        const auto nz = point(points, selected + 1u, 8, 0x412895), cz = point(points, selected, 8, 0x412898);
        heading.z = f32(double(nz) - cz);
        normalize(heading);
        r.respawn_heading.x = heading.x;
        r.respawn_heading.y = heading.y;
        r.respawn_heading.z = heading.z;
        for (auto &index : r.route_indices)
            index = std::bit_cast<std::int32_t>(r.spline_index);
        // 4128BB/4128DB copies an unwritten stack W. Retain the opaque existing W; never invent direction data.
    }
    void reset(W id)
    { // 412940 on actual physical/contact/race banks, no VehicleResetWorld or parameter snapshot.
        auto &s = w_.vehicles.physics[id];
        auto &r = w_.vehicles.race[id];
        auto &c = w_.vehicles.contact[id];
        if (w_.clock.phase != 3)
        {
            s.energy = f32(double(s.energy) - w_.gameplay.values[s.upgrade_active == 1 ? 3u : 1u]);
            if (c.impact_count.value < 0)
                c.impact_count.value = 0;
        }
        check_retirement();
        if (r.stopped == 1)
            return;
        reset_motion(id);
        r.respawn_requested = 0;
        reset_camera_dependent_effects(s);
        prepare_special(id);
        end_special(id);
        reset_stunt_score_hold(s);
        reset_stunt_state(s, w_.vehicles.physics[vehicle(w_, 0x4118b0)]);
        s.gear = 1;
        mask_respawn(id);
        place_pose(id, r.respawn_position);
        const auto forward = r.respawn_heading;
        VehicleVector up{-w_.gravity.unit.x, -w_.gravity.unit.y, -w_.gravity.unit.z, w_.gravity.unit.w};
        VehicleVector right;
        right.x = f32(double(forward.y) * up.z - double(forward.z) * up.y);
        right.y = f32(double(forward.z) * up.x - double(forward.x) * up.z);
        right.z = f32(double(forward.x) * up.y - double(forward.y) * up.x);
        normalize(right);
        right.w = s.orientation[3]; // 412A70 unwritten local retains only the native opaque lane.
        s.orientation = vehicle_identity();
        s.orientation[0] = right.x;
        s.orientation[1] = right.y;
        s.orientation[2] = right.z;
        s.orientation[3] = right.w;
        s.orientation[4] = up.x;
        s.orientation[5] = up.y;
        s.orientation[6] = up.z;
        s.orientation[7] = up.w;
        s.orientation[8] = forward.x;
        s.orientation[9] = forward.y;
        s.orientation[10] = forward.z;
        s.orientation[11] = forward.w;
        c.body.orientation = s.orientation;
        if (w_.level.game_type == 2)
            r.collision_hold = 300;
        r.pending_reset = 0;
        r.input_hold = 30;
        const auto found = contact::player::find_controller(w_.controls, id);
        w_.controls.current = found.value_or(0xffffffffu);
        if (found)
            w_.controls.controllers[*found].camera.collision_bypass_ticks = 300;
        contact::player::restore_primary(w_.controls);
    }
    void place(W id, float distance)
    { // 412B10 ->4289A0
        const auto *binding = w_.grid.start_spline.source();
        if (!binding)
            gap(0x412b1a, 0x4289a0, "412B10 requires captured source571328 binding");
        const auto spline = *binding;
        auto &r = w_.vehicles.race[id];
        const auto count = spline.read_word(8, 0x4289a7);
        const auto length = as_float(spline.read_word(24, 0x4289c1));
        float parameter;
        const auto integer =
            sgn_trial_quotient(count, &distance, &length, &parameter); // FST local before unrounded FTOL.
        nonzero(count, 0x4289d6);
        const auto selected =
            std::bit_cast<std::int32_t>(integer) < 0 ? count - ((0u - integer) % count) : integer % count;
        const auto next = (selected + 1u) % count;
        const auto points = spline.at_normalized_offset(spline.read_word(4, 0x4289f1));
        r.respawn_position.x = point(points, selected, 0, 0x4289fb);
        r.respawn_position.y = point(points, selected, 4, 0x4289ff);
        r.respawn_position.z = point(points, selected, 8, 0x428a05);
        r.respawn_position.w = point(points, selected, 12, 0x428a0b);
        const float dx = f32(double(point(points, next, 0, 0x428a19)) - r.respawn_position.x);
        const float dy = f32(double(point(points, next, 4, 0x428a20)) - r.respawn_position.y);
        const float dz = f32(double(point(points, next, 8, 0x428a29)) - r.respawn_position.z);
        if (!std::isfinite(parameter))
            gap(0x46291a, 0x465020, "spline fraction reaches CRT nonfinite/error dispatch beyond finite PMD domain");
        const double fraction = std::fmod(double(parameter), 1.0); // 46291A finite fmod(x,1).
        const float sx = f32(double(dx) * fraction);
        r.spline_index = selected;
        const float sy = f32(double(dy) * fraction);
        const double sz = fraction * dz;
        r.respawn_position.x = f32(double(sx) + r.respawn_position.x);
        r.respawn_position.y = f32(double(sy) + r.respawn_position.y);
        r.respawn_position.z = f32(sz + r.respawn_position.z);
        const auto reread_count = spline.read_word(8, 0x412b3b);
        nonzero(reread_count, 0x412b49);
        const auto progress = (selected * 100u) / reread_count;
        r.current_progress = std::bit_cast<std::int32_t>(progress);
        r.previous_progress = std::bit_cast<std::int32_t>(progress - 1u);
        const auto next_heading = (selected + 1u) % reread_count;
        r.progress = sgn_lifecycle_i32_float(progress);
        VehicleVector heading;
        auto p = spline.at_normalized_offset(spline.read_word(4, 0x412b6b));
        const auto nx = point(p, next_heading, 0, 0x412b76), cx = point(p, selected, 0, 0x412b79);
        heading.x = f32(double(nx) - cx);
        p = spline.at_normalized_offset(spline.read_word(4, 0x412b7f));
        const auto ny = point(p, next_heading, 4, 0x412b82), cy = point(p, selected, 4, 0x412b86);
        heading.y = f32(double(ny) - cy);
        p = spline.at_normalized_offset(spline.read_word(4, 0x412b90));
        const auto nz = point(p, next_heading, 8, 0x412b93), cz = point(p, selected, 8, 0x412b97);
        heading.z = f32(double(nz) - cz);
        normalize(heading);
        for (auto &i : r.route_indices)
            i = std::bit_cast<std::int32_t>(selected);
        r.respawn_heading.x = heading.x;
        r.respawn_heading.y = heading.y;
        r.respawn_heading.z = heading.z;
        reset(id);
    }

  public:
    explicit TrialStart(const std::shared_ptr<RaceSessionPlatform> &p) : borrow_(p), platform_(*p), w_(p->state()) {}
    // NEW R17 (replay_audio_reset): the SAME private 4187A0/412940 bodies, now also
    // reached from 41895E and the 418320 tail at 4184ED. No behaviour change.
    void retirement_4187a0()
    {
        check_retirement();
    }
    void reset_412940(W id)
    {
        if (id >= 6)
            gap(0x412940, 0, "412940 target outside six-slot bank");
        reset(id);
    }
    void reset_motion(W id)
    { // 413490 writes XYZ, never the neighboring W lanes.
        if (id >= 6)
            gap(0x413491, 0, "source motion target outside six-slot bank");
        auto &s = w_.vehicles.physics[id];
        stop_all_wheels(s);
        s.total_speed = 0;
        s.speed = 0;
        s.rpm = 0;
        s.previous_velocity.x = 0;
        s.previous_velocity.y = 0;
        s.previous_velocity.z = 0;
        s.velocity.x = 0;
        s.velocity.y = 0;
        s.velocity.z = 0;
        s.acceleration.x = 0;
        s.acceleration.y = 0;
        s.acceleration.z = 0;
        s.incremental = vehicle_identity();
        s.secondary_rotation = vehicle_identity();
    }
    void place_pose(W id, const VehicleVector &p)
    { // 408640, captured vehicle/position pointer domain.
        if (id >= 6)
            gap(0x408640, 0, "source placement target outside six-slot bank");
        auto &s = w_.vehicles.physics[id];
        auto &c = w_.vehicles.contact[id];
        s.previous_position = p;
        s.position = p;
        const auto position = contact::Vector4{p.x, p.y, p.z, std::bit_cast<W>(p.w)};
        c.body.previous_position = position;
        c.body.position = position;
        for (W i = 0; i < c.rig.count; ++i)
        {
            if (i >= c.rig.probes.size())
                gap(0x4086ca, 0, "source contact probe outside retained rig");
            auto &probe = c.rig.probes[i];
            probe.current.x = f32(double(probe.local.x) + c.body.position.x);
            probe.current.y = f32(double(probe.local.y) + c.body.position.y);
            probe.current.z = f32(double(probe.local.z) + c.body.position.z);
            probe.previous = probe.current;
        }
    }
    void restart(W id)
    { // 413530 exact reset fields;413490 is a separate preceding grid child.
        if (id >= 6)
            gap(0x413532, 0, "source restart target outside six-slot bank");
        auto &s = w_.vehicles.physics[id];
        auto &r = w_.vehicles.race[id];
        auto &c = w_.vehicles.contact[id];
        auto &state = w_.vehicles.restart[id];
        stop_all_wheels(s);
        s.total_speed = 0;
        s.speed = 0;
        s.previous_velocity.x = 0;
        s.previous_velocity.y = 0;
        s.previous_velocity.z = 0;
        s.velocity.x = 0;
        s.velocity.y = 0;
        s.velocity.z = 0;
        s.acceleration.x = 0;
        s.acceleration.y = 0;
        s.acceleration.z = 0;
        s.incremental = vehicle_identity();
        s.secondary_rotation = vehicle_identity();
        s.throttle = 0;
        s.rpm = 0;
        s.turbo_active = 0;
        s.turbo_rpm = 0;
        s.turbo_force = 0;
        // 22AA..22B3 then22A0..22A9. Other profile/neighbor bytes remain untouched.
        s.input = {};
        state.previous_input.throttle = 0;
        state.previous_input.brake = 0;
        state.previous_input.left = 0;
        state.previous_input.right = 0;
        s.previous_shift = 0;
        s.previous_stunt = 0;
        state.previous_input.rear_lock = 0;
        state.previous_input.steering_boost = 0;
        state.previous_input.turbo = 0;
        state.previous_input.auxiliary = 0;
        s.steering_ramp = 0;
        s.brake_ramp = 0;
        s.control_flags[0] = 0;
        s.control_flags[2] = 0;
        s.control_flags[3] = 0;
        s.upgrade_active = 0;
        r.route_indices[0] = 0;
        r.route_fractions[0] = 0;
        r.route_indices[1] = 0;
        r.route_fractions[1] = 0;
        state.mode_2630 = 1;
        s.performance_multiplier = 1;
        state.mode_2620 = 1;
        c.com_reset_ticks = 0;
        c.com_2628 = 0;
        c.com_262c = 0;
        reset_camera_dependent_effects(s);
        end_special(id); // tail418D90 uses the same actual globals/notice formatter.
    }
    void run()
    {
        w_.trial_start.reference_at_start = w_.ghosts.reference_completed;
        constexpr std::array<W, 5> slots{0, 4, 3, 1, 2};
        for (W i = 0; i < 5; ++i)
            w_.vehicles.race[slots[i]].recording = ReplayHandle{i};
        for (auto slot : slots)
        {
            const auto h = w_.vehicles.race[slot].recording;
            if (!h)
                gap(0x41aac0, 0, "trial replay stop reached null buffer");
            stop_replay(w_.replays, *h);
        }
        for (auto &r : w_.vehicles.race)
            r.active = 0;
        w_.vehicles.race[0].active = 1;
        const auto availability = w_.trial_selection.availability[0];
        if (w_.ghosts.reference_completed == 1 && availability == 0)
        {
            w_.ghosts.reference_completed = 0;
            w_.ghosts.roles[0].active = 1;
        }
        else if (availability == 1)
            w_.ghosts.roles[1].active = 1;
        w_.current_slot = 0;
        reset_race_record(w_.vehicles.race[0], w_.vehicles.physics[0]);
        reset_route_tracking(w_.vehicles.race[vehicle(w_, 0x405350)]);
        const auto offset = w_.level.rolling_start_offset;
        const auto *binding = w_.grid.start_spline.source();
        if (!binding)
            gap(0x4431f5, 0x412b10, "571328 has no source binding from4130D0;443100 does not perform428800 lookup");
        const auto length = as_float(binding->read_word(24, 0x4431f5));
        const auto id = vehicle(w_, 0x4431ef);
        place(id, sgn_trial_distance(offset, &length));
        const auto after = vehicle(w_, 0x443200);
        auto &r = w_.vehicles.race[after];
        r.previous_progress %= 100;
        r.current_progress %= 100;
        r.automatic_drive = 1;
        const auto p = w_.vehicles.physics[after].position;
        auto &camera = w_.controls.controllers[controller(w_, 0x44323f)].camera;
        camera.position = {p.x, p.y, p.z, p.w};
        w_.vehicles.physics[after].torque_curve = 3;
        set_overlay_transition(w_.overlays[controller(w_, 0x442ad0)], 2, 240, 180, 0, 100, 0, as_float(0x3f75c28f));
        const auto final = vehicle(w_, 0x44327e);
        w_.trial_start.phase_ticks = 180;
        w_.vehicles.ai[final].steering_ramp_ticks = 300;
        for (auto &c : w_.controls.controllers)
            contact::player::clear_notices(c.notices);
        clear_score_popups(w_.score_popups);
        clear_vehicle_notice(w_.vehicles.race[vehicle(w_, 0x4432a2)]);
        set_level_audio_output(w_.audio_level, 0);
        set_hud_countdown(w_.hud_countdown, controller(w_, 0x447b70), w_.selected_hud, 420);
        w_.clock.race_mode = 3;
        w_.clock.rolling_state = 1;
        w_.clock.phase = 1;
        w_.clock.game_mode = 4;
        w_.clock.lap_count = 100;
        w_.trial_start.announcement_ticks = 240;
    }
};
} // namespace
void start_source_time_trial_443100(FrontendState &frontend)
{
    const auto platform = frontend.shared_platform();
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry() || &frontend.world() != &platform->state())
        throw FormatError("443100 requires same source Platform");
    TrialStart(platform).run();
}
void check_source_vehicle_retirement_4187a0(FrontendState &frontend)
{
    const auto p = frontend.shared_platform();
    p->require_owner_thread();
    if (!p->source_construction_only_entry() || &frontend.world() != &p->state())
        throw FormatError("4187A0 requires same source Platform");
    TrialStart(p).retirement_4187a0();
}
void reset_source_vehicle_412940(FrontendState &frontend, W target)
{
    const auto p = frontend.shared_platform();
    p->require_owner_thread();
    if (!p->source_construction_only_entry() || &frontend.world() != &p->state())
        throw FormatError("412940 requires same source Platform");
    TrialStart(p).reset_412940(target);
}
void reset_source_vehicle_motion_413490(FrontendState &frontend, W target)
{
    const auto p = frontend.shared_platform();
    p->require_owner_thread();
    if (!p->source_construction_only_entry() || &frontend.world() != &p->state())
        throw FormatError("413490 requires same source Platform");
    TrialStart(p).reset_motion(target);
}
void restart_source_vehicle_413530(FrontendState &frontend, W target)
{
    const auto p = frontend.shared_platform();
    p->require_owner_thread();
    if (!p->source_construction_only_entry() || &frontend.world() != &p->state())
        throw FormatError("413530 requires same source Platform");
    TrialStart(p).restart(target);
}
void place_source_vehicle_408640(FrontendState &frontend, W target, const VehicleVector &position)
{
    const auto p = frontend.shared_platform();
    p->require_owner_thread();
    if (!p->source_construction_only_entry() || &frontend.world() != &p->state())
        throw FormatError("408640 requires same source Platform");
    TrialStart(p).place_pose(target, position);
}

} // namespace sgn::retail
