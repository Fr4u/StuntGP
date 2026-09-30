#include "sgn/source_normal_race_initialize.hpp"
#include "sgn/source_race_config_receivers.hpp"
#include "sgn/source_initial_grid.hpp"
#include "sgn/source_race_lifecycle.hpp"
#include "sgn/source_cold_audio_input.hpp"
#include "sgn/source_camera_spline.hpp"
#include "sgn/source_camera_stack_words.hpp"
#include "sgn/source_music_calls.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_race_hud.hpp"
#include "sgn/retail_race_prepare.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>
#include <utility>
extern "C" void sgn_trial_normalize(float *);
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_float;
float f32(double value)
{
    return static_cast<float>(value);
}
[[noreturn]] void gap(W site, W child, const char *reason)
{
    throw SourceRaceLifecycleBoundary(site, child, reason);
}
class Initialize final
{
    FrontendState &frontend_;
    std::shared_ptr<RaceSessionPlatform> platform_;
    RaceSessionState &w_;
    const SourceCameraStackWords::CallDomain stack_domain_;
    W vehicle(W id, W site) const
    {
        if (id >= 6)
            gap(site, 0, "race initialization vehicle pointer outside six source owners");
        return id;
    }
    W current(W site) const
    {
        return vehicle(w_.current_slot, site);
    }
    auto &controller(W site) const
    {
        if (w_.controls.current >= 4)
            gap(site, 0, "race initialization current controller outside four source owners");
        return w_.controls.controllers[w_.controls.current];
    }
    W observed(W site) const
    {
        return vehicle(controller(site).vehicle, site);
    }
    RaceHudView hud()
    {
        return {w_.controls, w_.hud_countdown, w_.hud_layout, w_.selected_hud, w_.hud_margins};
    }
    void clear_notices()
    {
        for (auto &c : w_.controls.controllers)
            contact::player::clear_notices(c.notices);
    }
    void tuning()
    { // 41F5D0: physical order, selected kind2 + configuration.present1 only.
        for (W i = 0; i < 6; ++i)
        {
            const auto &selected = w_.selection[i];
            if (selected.kind != 2 || selected.configuration.present != 1)
                continue;
            const auto &raw = selected.configuration.tuning_words;
            const auto bits = [&](W source_offset) { return raw[(source_offset + 8u) / 4u]; };
            const auto value = [&](W offset) { return as_float(bits(offset)); };
            auto &p = w_.vehicles.parameters[i];
            auto &s = w_.vehicles.physics[i];
            auto &sounds = w_.vehicles.load[i].sounds;
            auto &audio = w_.vehicles.audio[i];
            const auto first_mass = raw[0], first_count = bits(0x2c); // 41F5F1/F4 capture before first store.
            p.mass = as_float(first_mass);
            const float extra_mass = as_float(raw[1]);
            p.com_a = value(0);
            p.com_b = value(4);
            p.com_height = value(8);
            const W ride = bits(0x68);
            p.mass = f32(double(extra_mass) + p.mass);
            p.ride_height = as_float(ride);
            p.engine_decay = value(0x0c);
            p.rpm_idle = value(0x10);
            p.rpm_limit = value(0x14);
            p.rpm_max = value(0x18);
            p.max_torque = value(0x1c);
            s.torque_curve = std::bit_cast<std::int32_t>(bits(0x20));
            p.fuel_capacity = value(0xa4);
            p.refuel_rate = value(0xac);
            p.fuel_consumption = value(0xa8);
            p.drive_mode = std::bit_cast<std::int32_t>(bits(0x28));
            p.gear_count = std::bit_cast<std::int32_t>(first_count + 1u);
            p.shift_time = value(0x30);
            p.efficiency = value(0x34);
            p.power_split = value(0x38);
            p.final_drive = value(0x3c);
            p.ratios[0] = as_float(0xc003d70a);
            p.ratios[1] = 2;
            if (first_count != 0)
            {
                W index = 0;
                do
                {
                    // Reached raw read precedes destination write. Do not reject
                    // the invocation or earlier cars before these source stores.
                    if (index >= 46)
                        gap(0x41f6ca, 0, "tuning source word leaves actual64-word selection owner");
                    const W ratio = bits(0x40u + index * 4u);
                    if (index + 2u >= p.ratios.size())
                        gap(0x41f6cd, 0, "source tuning ratio store crosses native ratio owner");
                    p.ratios[index + 2u] = as_float(ratio);
                    ++index;
                } while (index < bits(0x2c));
            }
            p.brake_bias = value(0x60);
            p.brake_acceleration = value(0x64);
            for (auto &tyre : p.tyres)
            {
                tyre.static_friction = value(0x88);
                tyre.kinetic_friction = value(0x8c);
            }
            for (auto &tyre : p.tyres)
            {
                tyre.travel_in = value(0x6c);
                tyre.damping_in = value(0x70);
                tyre.stiffness_in = value(0x74);
                tyre.travel_out = value(0x78);
                tyre.damping_out = value(0x7c);
                tyre.stiffness_out = value(0x80);
            }
            p.drag_coefficient = value(0x90);
            p.lift_coefficient = value(0x94);
            p.steering_max = value(0x98);
            p.steering_sensitivity = value(0x9c);
            p.steering_acceleration = value(0xa0);
            p.turbo_acceleration = value(0xb0);
            p.turbo_max = value(0xb4);
            p.turbo_decay = value(0xb8);
            p.turbo_scale = value(0xbc);
            p.turbo_consumption = value(0xc0);
            p.turbo_energy_threshold = value(0xc4);
            p.launch_time = value(0xc8);
            p.launch_tolerance = value(0xcc);
            p.launch_speed = value(0xd0);
            p.max_pitch_speed = value(0xd4);
            p.max_yaw_speed = value(0xd8);
            sounds.engine_rev = std::bit_cast<std::int32_t>(bits(0xe0));
            sounds.engine_idle = std::bit_cast<std::int32_t>(bits(0xe4));
            sounds.engine_boost = std::bit_cast<std::int32_t>(bits(0xe8));
            sounds.horn = std::bit_cast<std::int32_t>(bits(0xdc));
            audio.field_7718 = bits(0xec);
            audio.field_771c = bits(0xf0);
            audio.field_7720 = bits(0xf4);
        }
    }
    void records_and_length()
    { // 41D980 ->41D390 ->428840, no lookup by spline ID.
        w_.current_slot = 0;
        for (W count = 0; count < 6; ++count)
        {
            const W id = current(0x41d300);
            reset_race_record(w_.vehicles.race[id], w_.vehicles.physics[id]);
            ++w_.current_slot;
        }
        w_.current_slot = 0;
        const auto at = platform_->source_scene_resources().models().source_loading_state().track.splines;
        const SourceCameraSplineRef descriptor(platform_, at.allocation, at.offset);
        const auto points = descriptor.at_normalized_offset(descriptor.read_word(4, 0x428847));
        const W count = descriptor.read_word(8, 0x42884e);
        W px = points.read_word(0, 0x428857), py = points.read_word(4, 0x42885e), pz = points.read_word(8, 0x428861);
        [[maybe_unused]] W pw = points.read_word(12, 0x428864); // Read even when count0.
        double length = 0;
        for (W i = 0; i < count; ++i)
        {
            const auto offset = std::uint64_t(i) * 16u;
            const W x = points.read_word(offset, 0x428879), y = points.read_word(offset + 4u, 0x428887);
            const W z = points.read_word(offset + 8u, 0x42888e);
            pw = points.read_word(offset + 12u, 0x42889d);
            const double dx = double(as_float(x)) - as_float(px), dy = double(as_float(y)) - as_float(py),
                         dz = double(as_float(z)) - as_float(pz);
            length += std::sqrt((dz * dz + dy * dy) + dx * dx);
            px = x;
            py = y;
            pz = z;
        }
        w_.clock.course_length = f32(length); // Only41D39B publishes after normal child return.
    }
    void projection()
    {
        auto &c = controller(0x44f4c0).camera;
        camera_rebuild_projection(c, w_.shared_model_identity);
        w_.frame_projection = c.projection;
        w_.inverse_depth_scale = c.inverse_depth_scale;
        w_.depth_offset = c.depth_offset;
    }
    W preset_choice(W target, W selected)
    { // Actual direct preset;40D380 retains nested40D280 order.
        if (selected < 1 || selected > 4)
            return selected;
        const W id = vehicle(target, selected == 2 ? 0x40d202 : selected == 3 ? 0x40d302 : 0x40d282);
        const W base = selected == 4 ? 1u : selected;
        auto &v = w_.vehicles.camera_settings[id];
        v.angles.x = base == 1 ? 13.0f : base == 2 ? 18.0f : 20.0f;
        v.angles.y = 0;
        v.angles.z = 0;
        v.distance = base == 1 ? 600.0f : base == 2 ? 1000.0f : 1500.0f;
        v.approach_scale = base == 1 ? 0.5f : 0.125f;
        v.target_offset.x = 0;
        v.target_offset.y = 150;
        v.target_offset.z = 0;
        w_.vehicles.race[id].saved_camera_mode = 5;
        v.adaptive_distance = 0;
        auto &mode = controller(0x4028d0).camera;
        mode.mode = 5;
        mode.twist = 0;
        mode.collision_bypass_ticks = 300;
        for (W slot = 0; slot < 6; ++slot)
            if (w_.vehicles.race[slot].active == 1)
                reset_camera_dependent_effects(w_.vehicles.physics[slot]);
        auto &lens = controller(0x403310).camera;
        lens.lens = as_float(0x4005c28f);
        lens.half_width = as_float(0x3faa9fbe);
        lens.half_height = 1;
        projection();
        camera_request_lens(controller(0x402ab0).camera, as_float(0x4005c28f));
        w_.selected_camera_preset = base;
        if (selected == 4)
        {
            v.adaptive_distance = 1;
            w_.selected_camera_preset = 4;
        }
        return selected; // Host invocation observation; no extra source store/read.
    }
    W preset(W target)
    { // 404310 alone applies the source split override.
        return preset_choice(target, w_.split_setting != 0 ? 1u : w_.selected_camera_preset);
    }
    void cameras()
    { // 402710 selects branch before any404310 can change globals.
        for (auto &c : w_.controls.controllers)
            c.active = 0;
        const W split = w_.split_setting;
        const W count = split > 4 ? 0u : split == 0 ? 1u : split <= 2 ? 2u : split;
        for (W i = 0; i < count; ++i)
        {
            const W target = i == 0 ? w_.trial_selection.split_selection : w_.other_selected_vehicles[i - 1u];
            auto &c = w_.controls.controllers[i];
            c.vehicle = target;
            c.active = 1;   // 4026F7/6FD before404310.
            preset(target); // Uses current4722D0, not bound controller i.
        }
        w_.controls.current = 0; // 402580 only at normal tail.
    }
    void countdown(W ticks)
    { // 447BD0 has no eager validation of later vehicle identities.
        const W saved = w_.controls.current;
        w_.controls.current = 0;
        for (W count = 0; count < 4; ++count)
        {
            if (w_.vehicles.physics[observed(0x447bf5)].energy_exempt == 0)
                set_hud_countdown(w_.hud_countdown, w_.controls.current, w_.selected_hud, ticks);
            ++w_.controls.current;
        }
        w_.controls.current = saved;
    }
    void camera_position(SourceCameraStackWords::Frame &stack)
    { // 403F40, destination captured by404280.
        // The same retained stack fragment recorded the actually selected
        // 404310 branch. No native address or invented source ESP is used.
        auto &c = controller(0x403f46).camera;
        const W id = observed(0x403f4f);
        const auto &p = w_.vehicles.physics[id];
        const auto &v = w_.vehicles.camera_settings[id];
        const float third = as_float(0x3eaaaaab), tenth = as_float(0x3dcccccd);
        const float ty = f32(double(v.target_offset.y) + p.position.y),
                    tz = f32(double(v.target_offset.z) + p.position.z);
        const double dx = (double(v.target_offset.x) + p.position.x) - c.target.x;
        const float dy = f32(double(ty) - c.target.y), dz = f32(double(tz) - c.target.z);
        c.target.x = f32(dx * third + c.target.x);
        c.target.y = f32(double(f32(double(dy) * third)) + c.target.y);
        c.target.z = f32(double(f32(double(dz) * third)) + c.target.z);
        const double ux = (double(c.stabilized_up.x) - p.up.x) * tenth;
        const float uy = f32(double(f32(double(c.stabilized_up.y) - p.up.y)) * tenth);
        const float uz = f32(double(f32(double(c.stabilized_up.z) - p.up.z)) * tenth);
        c.stabilized_up.x = f32(double(c.stabilized_up.x) - ux);
        c.stabilized_up.y = f32(double(c.stabilized_up.y) - uy);
        c.stabilized_up.z = f32(double(c.stabilized_up.z) - uz);
        const auto up = c.stabilized_up; // Actual source16-byte local; no new persistent settings.
        const double dot = (double(up.z) * p.basis_z.z + double(up.y) * p.basis_z.y) + double(up.x) * p.basis_z.x;
        CameraVector forward{f32(double(p.basis_z.x) - f32(dot * up.x)), f32(double(p.basis_z.y) - f32(dot * up.y)),
                             f32(double(p.basis_z.z) - dot * up.z), 0};
        sgn_trial_normalize(&forward.x);
        CameraVector right{f32(double(forward.y) * up.z - double(forward.z) * up.y),
                           f32(double(forward.z) * up.x - double(forward.x) * up.z),
                           f32(double(forward.x) * up.y - double(forward.y) * up.x), 0};
        sgn_trial_normalize(&right.x);
        const double rx = double(v.orbit_offset.x) * right.x, upx = double(v.orbit_offset.y) * up.x;
        const float ry = f32(double(v.orbit_offset.x) * right.y), rz = f32(double(v.orbit_offset.x) * right.z);
        const float upy = f32(double(v.orbit_offset.y) * up.y), upz = f32(double(v.orbit_offset.y) * up.z);
        const float fx = f32(double(v.orbit_offset.z) * forward.x), fy = f32(double(v.orbit_offset.z) * forward.y),
                    fz = f32(double(v.orbit_offset.z) * forward.z);
        c.position.x = f32(-((upx + rx) + fx) + p.position.x);
        c.position.y = f32(-double(f32(double(fy) + f32(double(upy) + ry))) + p.position.y);
        const float z = f32(-double(f32(double(fz) + f32(double(upz) + rz))) + p.position.z); // 404202
        double height = double(w_.vehicles.parameters[id].height) - 115.0;                    // 404205/40420B
        c.position.z = z;                            // 404214, before the reached W read
        const auto observed_w = stack.read_404217(); // Actual retained S-28 word.
        if (!observed_w)
            gap(0x404217, 0, "403F40 W after an invalid404310 preset requires unrecorded caller stack history at S-28");
        const W position_w = *observed_w;                                    // Literal or NEW opaque relocation token.
        const bool negative_or_unordered = height < 0 || std::isnan(height); // 40421A
        // 404220 is a DWORD copy. The token transports stack identity through
        // the unchanged16-byte vectors; its float interpretation is forbidden.
        c.position.w = as_float(position_w); // 404220, before height correction
        if (negative_or_unordered)
            height = 0; // 404223..40422C, FCOM C0 including unordered.
        const float hx = f32(height * p.up.x), hy = f32(height * p.up.y);
        const double hz = height * p.up.z;
        c.position.x = f32(double(c.position.x) - hx);
        c.position.z = f32(double(c.position.z) - hz);
        c.position.y = f32(((double(c.position.y) - hy) - c.height_offset) + c.speed_lift);
    }
    void initialize_camera(W target)
    { // 404280 reads target active before current mode.
        if (w_.vehicles.race[vehicle(target, 0x404280)].active == 0)
            return;
        if (controller(0x404293).camera.mode != 5)
            return;
        auto stack = platform_->source_camera_stack_words().enter_404280_from_40d5c6(stack_domain_);
        const W applied_preset = preset(target);
        // Materialize only the proven call-link footprint after the actual
        // projection succeeds. This host bookkeeping has no source side effect.
        stack.after_preset_404310(applied_preset);
        camera_position(stack);
        auto &c = controller(0x4042ac).camera;
        W id = observed(0x4042b1);
        c.target.x = f32(double(w_.vehicles.camera_settings[id].target_offset.x) + w_.vehicles.physics[id].position.x);
        id = observed(0x4042c6);
        c.target.y = f32(double(w_.vehicles.camera_settings[id].target_offset.y) + w_.vehicles.physics[id].position.y);
        id = observed(0x4042db);
        c.target.z = f32(double(w_.vehicles.camera_settings[id].target_offset.z) + w_.vehicles.physics[id].position.z);
        c.velocity.x = 0;
        c.velocity.y = 0;
        c.velocity.z = 0;
    }
    void audio(InputWindowHandles handles)
    { // 460B80, captured comparisons before calls.
        const W split = w_.split_setting, capability = w_.audio_initialization.spatial_capability,
                old = w_.audio_driver.spatial_mode;
        w_.audio_initialization.saved_spatial_mode = old;
        const W desired = split == 0 ? capability : 0u;
        w_.audio_driver.spatial_mode = desired;
        if (old == desired)
            return;
        shutdown_source_audio_4621d0(*platform_);
        // Actual source native owner. Normal false is ignored by40D500; NEW
        // ownership failures propagate after their real prefix, notably retained
        // warm primary461D25. No preview AudioDriver or extra releases are used.
        (void)initialize_platform_source_audio_462090(*platform_, handles);
    }

  public:
    Initialize(FrontendState &frontend, std::shared_ptr<RaceSessionPlatform> platform,
               SourceCameraStackWords::CallDomain domain = SourceCameraStackWords::CallDomain::normal_scene)
        : frontend_(frontend), platform_(std::move(platform)), w_(platform_->state()), stack_domain_(domain)
    {
    }
    void direct_preset(W target, W selected)
    {
        (void)preset_choice(target, selected);
    }
    void run(SourceBootFilePlatform &files, NativeSourceMenuCrt &crt, InputWindowHandles handles)
    {
        reload_selected_source_vehicle_configurations_40bec0(frontend_, files, crt); // 40D502.
        tuning();                                                                    // 40D507.
        w_.startup.field_5898c8 = 1;
        w_.startup.fields_62dcf0.fill(0);
        w_.startup.fields_62d480.fill(0);
        // 431A90 ->42FA00 is an exact RET child in pinned D3D PE.
        clear_notices(); // 442C30, distinct from the second call within448110.
        initialize_race_hud(hud());
        reset_current_race_hud(w_.vehicles, current(0x443d90), w_.startup_counters);
        enable_selected_race_hud(hud());
        initialize_source_race_grid_pair_40d549(frontend_); // Exactly one pair at original site.
        records_and_length();
        cameras();
        countdown(60);
        for (W i = 0; i < 4; ++i)
        {
            const auto active = w_.controls.controllers[i].active;
            w_.controls.current = i;
            if (active != 1)
                continue;
            w_.load_selection.template_vehicle = w_.controls.controllers[i].vehicle;
            initialize_camera(w_.load_selection.template_vehicle);
            preset(w_.load_selection.template_vehicle); // Fresh474630 after404280.
        }
        for (W i = 0; i < 6; ++i)
            activate_vehicle_for_race(w_.vehicles.race[i], w_.vehicles.startup[i]);
        prepare_vehicles_for_race(w_.vehicles);
        w_.clock.game_mode = 1;
        w_.clock.phase = 2;
        w_.startup.field_60f2c0 = 0;
        w_.automatic.initialized = 0;
        audio(handles); // 40D623, after all previous source state changes.
        if (w_.scene_mode == 1)
            select_audio_music_track(w_.music, w_.level.music_track);
        w_.startup.start_delay_ticks = 60;
        set_level_audio_output(w_.audio_level, 0); // 40D64B tail460CB0.
    }
};
} // namespace
void initialize_source_normal_race_40d500(FrontendState &frontend, SourceBootFilePlatform &files,
                                          NativeSourceMenuCrt &crt, InputWindowHandles handles)
{
    const auto platform = frontend.shared_platform();
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry() || &frontend.world() != &platform->state())
        throw FormatError("40D500 requires same source Platform and world");
    Initialize(frontend, platform).run(files, crt, handles);
}
void restart_source_normal_race_446af5(FrontendState &frontend, SourceBootFilePlatform &files, NativeSourceMenuCrt &crt,
                                       InputWindowHandles handles)
{
    const auto platform = frontend.shared_platform();
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry() || &frontend.world() != &platform->state())
        throw FormatError("446AF5 requires same source Platform and world");
    Initialize(frontend, platform, SourceCameraStackWords::CallDomain::pause_restart).run(files, crt, handles);
}
void apply_source_pause_camera_preset(FrontendState &frontend, W entry, W captured_vehicle)
{
    const auto platform = frontend.shared_platform();
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry() || &frontend.world() != &platform->state())
        throw FormatError("446BB0 requires same source Platform and world");
    W selected;
    switch (entry)
    {
    case 0x40d280:
        selected = 1;
        break;
    case 0x40d200:
        selected = 2;
        break;
    case 0x40d300:
        selected = 3;
        break;
    case 0x40d380:
        selected = 4;
        break;
    default:
        gap(0x446bf4, entry, "pause camera tail target outside recovered jump table");
    }
    Initialize(frontend, platform).direct_preset(captured_vehicle, selected);
}
} // namespace sgn::retail
