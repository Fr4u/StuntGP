#include "sgn/retail_race_initialization.hpp"
#include <bit>
#include <utility>

namespace sgn::retail
{
namespace
{
void publish_projection(RaceFrameProjection output, const CameraState &camera)
{
    output.matrix = camera.projection;
    output.inverse_depth_scale = camera.inverse_depth_scale;
    output.depth_offset = camera.depth_offset;
}
bool preset_writes(std::uint32_t split, std::uint32_t preset)
{
    return split != 0 || (preset >= 1 && preset <= 4);
}
} // namespace
void validate_race_initialization_bindings(const RaceInitializationWorld &world)
{
    if (world.reset.current >= 6 || world.load_selection.template_vehicle >= 6 || world.hud.controls.current >= 4 ||
        world.hud.selected_hud >= 4)
        throw FormatError("race initialization cursor outside native storage");
    if (&world.hud.controls != &world.reset.controls || &world.audio.audio.controls != &world.reset.controls ||
        &world.audio.audio.level != &world.audio.level || &world.audio.assets != &world.assets)
        throw FormatError("race initialization requires shared controller, audio and asset owners");
    if (std::bit_cast<std::uint32_t>(world.reset.parameters.mode) != world.clock.game_mode ||
        std::bit_cast<std::uint32_t>(world.reset.parameters.stage) != world.clock.phase)
        throw FormatError("race reset parameters must reflect the entry world mode and phase");
    for (unsigned i = 0; i < 6; ++i)
    {
        const auto &reset = world.reset.vehicles[i];
        if (&reset.physics != &world.vehicles.physics[i] || &reset.race != &world.vehicles.race[i] ||
            &reset.contact != &world.vehicles.contact[i] || reset.id != i)
            throw FormatError("race initialization reset views must borrow the same physical slots");
    }
    for (const auto &controller : world.reset.controls.controllers)
        if (controller.vehicle >= 6)
            throw FormatError("race initialization controller vehicle identity");
}
RaceInitializationObservation initialize_race(RaceInitializationWorld world, const RaceInitializationParameters &input)
{
    validate_race_initialization_bindings(world);
    auto &storage = world.vehicles;
    auto &current = world.reset.current;
    auto &controls = world.reset.controls;
    auto loads = storage.load_views();
    VehicleLoadWorld loader{loads, current};
    reload_selected_vehicle_configurations(loader, world.assets, input.cars, world.load_selection, input.control_kind);
    auto tuning = [&]<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<VehicleTuningView, 6>{
            {{storage.parameters[I], storage.physics[I], storage.load[I].sounds, storage.audio[I]}...}};
    }(std::make_index_sequence<6>{});
    apply_selection_vehicle_tuning(input.selection, tuning);
    world.state.field_5898c8 = 1;
    world.state.fields_62dcf0.fill(0);
    world.state.fields_62d480.fill(0);
    // 431A90 -> 42FA00; the latter is literally RET in the pinned D3D binary.
    for (auto &controller : controls.controllers)
        contact::player::clear_notices(controller.notices);
    initialize_race_hud(world.hud);
    reset_current_race_hud(storage, current, world.counters);
    enable_selected_race_hud(world.hud);
    RaceInitializationObservation observed;
    initialize_race_grid(storage, world.reset, world.splines, world.grid, world.clock.race_mode, &observed.first_grid);
    const auto saved_current = current;
    current = 0;
    for (unsigned i = 0; i < 6; ++i)
    {
        auto &contacts = storage.contact[i];
        contact::response::initialize_rig(input.cars[i].model, contacts.body.position, contacts.rig); // 4082D0 only
        ++current;
    }
    current = saved_current;
    initialize_race_grid(storage, world.reset, world.splines, world.grid, world.clock.race_mode, &observed.second_grid);
    reset_all_race_records(storage, current, world.clock, world.splines);

    // These invocation-only observations retain shared settings changes between
    // camera calls. Only settings are published back; physics remains its owner.
    std::array<CameraVehicle, 6> cameras;
    CameraEffectTargets effect_targets;
    for (unsigned i = 0; i < 6; ++i)
    {
        cameras[i] = storage.camera_snapshot(i);
        effect_targets.vehicles[i] = &storage.physics[i];
        effect_targets.active[i] = std::bit_cast<std::uint32_t>(storage.race[i].active);
    }
    const auto effect_reset = camera_vehicle_reset_adapter(effect_targets);
    const auto split = world.audio.audio.split_setting;
    CameraRaceStartView camera_view{controls, cameras,      world.selected_camera_preset,
                                    split,    effect_reset, world.frame_projection.shared_model_identity};
    const auto activation_camera = controls.current;
    const bool activation_writes = split <= 4 && preset_writes(split, world.selected_camera_preset);
    activate_race_cameras(camera_view, input.selected_vehicles);
    if (activation_writes)
        publish_projection(world.frame_projection, controls.controllers[activation_camera].camera);
    arm_race_hud_countdown(world.hud, storage.physics, 60);
    for (unsigned i = 0; i < 4; ++i)
    {
        controls.current = i;
        auto &controller = controls.controllers[i];
        if (controller.active != 1)
            continue;
        world.load_selection.template_vehicle = controller.vehicle;
        observed.camera_position_opaque[i] = initialize_race_camera(camera_view, world.load_selection.template_vehicle);
        const bool writes = preset_writes(split, world.selected_camera_preset);
        camera_restore_preset(controller.camera, cameras[world.load_selection.template_vehicle],
                              world.selected_camera_preset, split, effect_reset,
                              world.frame_projection.shared_model_identity);
        if (writes)
            publish_projection(world.frame_projection, controller.camera);
    }
    for (unsigned i = 0; i < 6; ++i)
        storage.store_camera_settings(i, cameras[i]);
    for (unsigned i = 0; i < 6; ++i)
        activate_vehicle_for_race(storage.race[i], storage.startup[i]);
    prepare_vehicles_for_race(storage);
    world.clock.game_mode = 1;
    world.clock.phase = 2;
    world.state.field_60f2c0 = 0;
    world.automatic.initialized = 0;
    reconfigure_audio_for_split(world.audio);
    if (input.scene_mode == 1)
        select_audio_music_track(world.music, input.music_track);
    world.state.start_delay_ticks = 60;
    set_level_audio_output(world.audio.level, 0);
    return observed;
}
} // namespace sgn::retail
