#include "sgn/source_cold_camera_initialization.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_camera_vehicle.hpp"
#include "sgn/vehicle_camera_settings.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_float;
void xyz(CameraVector &v, float x, float y, float z) noexcept
{
    v.x = x;
    v.y = y;
    v.z = z;
}
void publish_projection(RaceSessionState &w, const CameraState &camera)
{
    w.frame_projection = camera.projection;
    w.inverse_depth_scale = camera.inverse_depth_scale;
    w.depth_offset = camera.depth_offset;
}
void copy_primary_controller_4025fb(RaceSessionState &w, unsigned i)
{
    // Native owners of the SAME source74C-byte controller. The HUD/overlay
    // owners are split out of Controls in this host; they must be copied too.
    w.controls.controllers[i] = w.controls.controllers[0];
    w.camera_lifecycle[i] = w.camera_lifecycle[0];
    w.camera_presentation[i] = w.camera_presentation[0];
    w.overlays[i] = w.overlays[0];
    w.hud_countdown[i] = w.hud_countdown[0];
    w.hud_layout[i] = w.hud_layout[0];
}
void initialize_other_controllers_4025d0(RaceSessionState &w)
{
    for (unsigned i = 1; i < 4; ++i)
    {
        w.controls.current = i; // 4025F5, before actual primary copy
        copy_primary_controller_4025fb(w, i);
        auto &controller = w.controls.controllers[i];
        controller.active = 0;  // 4025FD
        controller.vehicle = i; // 402605, source49A1F8+i*7724 identity
        // 404310 reads live split/preset at EACH child call. Do not compute one
        // snapshot before the loop;40D280 can change selected_camera_preset.
        const auto preset = w.split_setting != 0 ? 1u : w.selected_camera_preset;
        if (preset >= 1 && preset <= 4)
        {
            CameraVehicle vehicle;
            read_vehicle_camera_settings(vehicle, w.vehicles.camera_settings[i], w.vehicles.race[i].saved_camera_mode);
            CameraEffectTargets targets;
            for (unsigned slot = 0; slot < 6; ++slot)
            {
                targets.vehicles[slot] = &w.vehicles.physics[slot];
                targets.active[slot] = static_cast<std::uint32_t>(w.vehicles.race[slot].active);
            }
            camera_restore_preset(controller.camera, vehicle, w.selected_camera_preset, w.split_setting,
                                  camera_vehicle_reset_adapter(targets), w.shared_model_identity); // 40260B ->404310
            write_vehicle_camera_settings(w.vehicles.camera_settings[i], w.vehicles.race[i].saved_camera_mode, vehicle);
            publish_projection(w, controller.camera); // actual44F4C0 reached by the preset
        } // 404345 returns directly for out-of-range preset: no settings write.
    }
    const auto split = w.split_setting; // 402628, after all three404310 calls
    if (split == 1 || split == 2)
    {
        if (w.vehicles.race[1].active == 1)
            w.controls.controllers[1].active = 1;
    }
    else if (split == 3 || split == 4)
    {
        if (w.vehicles.race[1].active == 1)
            w.controls.controllers[1].active = 1;
        if (w.vehicles.race[2].active == 1)
            w.controls.controllers[2].active = 1;
        if (split == 4 && w.vehicles.race[3].active == 1)
            w.controls.controllers[3].active = 1;
    }
    contact::player::restore_primary(w.controls); // 4026D3/40266E/4026B6 ->402580
}
} // namespace
void initialize_source_cold_camera_402420(const std::weak_ptr<RaceSessionPlatform> &borrow)
{
    const auto platform = borrow.lock();
    if (!platform)
        throw SourceAllocationError(0x402420, "cold camera platform expired");
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry())
        throw SourceAllocationError(0x402420, "camera startup requires source platform");
    auto &w = platform->state();
    if (w.scene_mode != 2)
        throw SourceAllocationError(0x401f13, "cold camera entry requires actual scene2");
    // 402422 ->401F00: actualscene!=1 branch, count first, exactly32*64B
    // authored records. Retain499F88/8C until their reached402531/402537 stores.
    w.camera_bank.count = 0;        // 401F21:499F90
    w.camera_bank.records.fill({}); // 401F26:4722D8..472AD7
    initialize_source_camera_controllers_402427(borrow);
}
void initialize_source_camera_controllers_402427(const std::weak_ptr<RaceSessionPlatform> &borrow)
{
    const auto platform = borrow.lock();
    if (!platform)
        throw SourceAllocationError(0x402427, "camera platform expired");
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry())
        throw SourceAllocationError(0x402427, "camera startup requires source platform");
    auto &w = platform->state();
    w.controls.current = 0; // 40242E:4722D0=498248
    auto &primary = w.controls.controllers[0];
    auto &camera = primary.camera;
    auto &life = w.camera_lifecycle[0];
    life.pending_record.reset();                                     // 402438:498420, sourceNULL
    life.pending_mode = 0;                                           // 40243E:498424
    camera.pending_lens = 0;                                         // 402444:498428
    camera.requested_lens = 0;                                       // 40244A:49842C
    primary.active = 1;                                              // 402450
    primary.vehicle = 0;                                             // 40245A:4982F4=49A1F8
    const auto spline = find_source_camera_spline_428800(borrow, 1); // 402464
    w.camera_presentation[0].spline.bind_source(spline);             // 402474, no descriptor reads
    camera.lens = as_float(0x404ccccd);
    camera.near_clip = 1;
    camera.far_factor = as_float(0x43000000);
    life.clip_near = as_float(0x42c80000);
    life.clip_far = as_float(0x48c35000);
    camera.half_width = as_float(0x3faa9fbe);
    camera.half_height = 1;
    xyz(camera.position, 0, 0, 0);
    xyz(camera.target, 0, 0, as_float(0x447a0000));
    xyz(camera.up, 0, 1, 0);
    xyz(life.scale, 1, 1, 1);
    xyz(camera.stabilized_up, 0, 1, 0);
    camera_rebuild_projection(camera, w.shared_model_identity); // 4024F2 ->44F4C0, no extra matrix arena allocation
    publish_projection(w, camera);
    camera.mode = 1; // 4024FC, direct store; NOT4028D0, no effect reset here
    camera.control_angles = {0x1000000u, 0, 0};
    camera.twist = 0;
    life.base_lens = 1;
    camera_clear_velocity(camera);       // 402522 ->4023D0, XYZ/speed only
    camera_clear_acceleration(camera);   // 402527 ->4023F0, XYZ only
    w.camera_session.path_speed = 0;     // 402531:499F88
    w.camera_session.path_parameter = 0; // 402537:499F8C
    life.current_record = 0;             // 40253E:4722D8 identity, NONNULL slot0 even when bank.count0
    life.next_mode = 10;
    camera.pending_follow_setup = 1;
    camera.settle_ticks = 0;
    camera.speed_lift = 0;
    camera.height_offset = 0;
    life.unknown_1c8 = 0;
    initialize_other_controllers_4025d0(w); // 402575 tail, same owners
}
} // namespace sgn::retail
