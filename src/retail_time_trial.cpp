#include "sgn/retail_time_trial.hpp"
#include <bit>
#include <stdexcept>

namespace sgn::retail
{
void set_overlay_transition(ControllerOverlayTransition &state, std::uint32_t kind, std::uint32_t hold,
                            std::uint32_t duration, std::uint32_t flags, std::uint32_t value, std::uint32_t target,
                            float rate)
{
    // 00442AD0..00442B53. Unordered FCOMP sets C0, taking the subtraction path.
    state.active = 1;
    state.kind = kind;
    state.initial_value = value;
    state.value = value;
    state.target = target;
    state.age = 0;
    state.hold_ticks = hold;
    state.duration = duration;
    state.flags = flags;
    state.rate = !(rate >= 1) ? vehicle_f32(double(rate) - std::bit_cast<float>(0x3d4ccccdu)) : rate;
}
void set_hud_countdown(std::span<ControllerHudCountdown, 4> hud, unsigned controller, unsigned &selected_hud,
                       std::uint32_t ticks)
{
    if (controller >= 4)
        throw std::out_of_range("HUD controller slot");
    selected_hud = controller;
    auto &state = hud[controller];
    state.ticks = ticks;
    state.enabled = 1;
    state.value = 100;
    state.phase = 0;
}
void set_level_audio_output(LevelAudioState &audio, std::uint32_t output) noexcept
{
    if (output != 0xffffffff)
        audio.output_b = output;
    const std::uint32_t product = audio.scale * audio.input_b;
    const std::uint32_t scaled = (product << 8) - product;
    audio.output_b_copy = static_cast<std::uint32_t>((std::uint64_t(scaled) * 0xd1b71759u) >> 45);
}
bool place_vehicle_at_distance(VehicleResetWorld &world, unsigned target, float distance)
{
    if (target >= 6)
        throw std::out_of_range("distance-start vehicle slot");
    const auto placement = prepare_distance_start(world.parameters.spline, distance);
    auto &race = world.vehicles[target].race;
    race.respawn_position = placement.position;
    race.spline_index = placement.selected_index;
    race.current_progress = std::bit_cast<std::int32_t>(placement.progress_current);
    race.previous_progress = std::bit_cast<std::int32_t>(placement.progress_previous);
    race.progress = placement.progress_percent;
    for (unsigned i = 0; i < 3; ++i)
        race.route_indices[i] = std::bit_cast<std::int32_t>(placement.route_indices[i]);
    race.respawn_heading.x = placement.heading.x;
    race.respawn_heading.y = placement.heading.y;
    race.respawn_heading.z = placement.heading.z;
    return reset_vehicle(world, target);
}
bool start_time_trial(TimeTrialStartWorld &world, std::uint32_t rolling_start_offset)
{
    if (world.controls.current >= 4)
        throw std::out_of_range("Time Trial controller selector");
    for (unsigned i = 0; i < 6; ++i)
        if (world.vehicles[i].id != i)
            throw std::invalid_argument("Time Trial vehicle identity differs from physical slot");
    world.state.reference_at_start = world.ghosts.reference_completed;
    // 00443115..0044316F bind the same five source buffers to their vehicles.
    constexpr std::array<unsigned, 5> vehicle_slots{0, 4, 3, 1, 2};
    for (unsigned buffer = 0; buffer < 5; ++buffer)
        world.vehicles[vehicle_slots[buffer]].race.recording = ReplayHandle{buffer};
    for (unsigned buffer = 0; buffer < 5; ++buffer)
        stop_replay(world.replays, ReplayHandle{buffer});
    for (auto &vehicle : world.vehicles)
        vehicle.race.active = 0;
    world.vehicles[0].race.active = 1;
    const auto availability = world.selection.availability[0];
    if (world.ghosts.reference_completed == 1 && availability == 0)
    {
        world.ghosts.reference_completed = 0;
        world.ghosts.roles[0].active = 1;
    }
    else if (availability == 1)
        world.ghosts.roles[1].active = 1;
    world.current_slot = 0;
    auto &first = world.vehicles[0];
    reset_race_record(first.race, first.physics);
    reset_route_tracking(first.race);
    // Fresh call context prevents the late mode=4 assignment from leaking into
    // 412940's checks. The source converts the start offset as an unsigned DWORD.
    auto parameters = world.reset_parameters;
    parameters.stage = std::bit_cast<std::int32_t>(world.clock.phase);
    parameters.mode = std::bit_cast<std::int32_t>(world.clock.game_mode);
    VehicleResetWorld reset{world.vehicles, world.current_slot, world.controls, world.reset_globals, parameters};
    const float distance = vehicle_f32(double(parameters.spline.descriptor.length) - double(rolling_start_offset));
    const bool placed = place_vehicle_at_distance(reset, 0, distance);
    world.current_slot = reset.current;
    first.race.previous_progress %= 100;
    first.race.current_progress %= 100;
    first.race.automatic_drive = 1;
    const auto &position = first.physics.position;
    world.controls.controllers[world.controls.current].camera.position = {position.x, position.y, position.z,
                                                                          position.w};
    first.physics.torque_curve = 3;
    set_overlay_transition(world.overlays[world.controls.current], 2, 240, 180, 0, 100, 0,
                           std::bit_cast<float>(0x3f75c28fu));
    world.state.phase_ticks = 180;
    world.ai[world.current_slot].steering_ramp_ticks = 300;
    for (auto &controller : world.controls.controllers)
        contact::player::clear_notices(controller.notices);
    clear_score_popups(world.score);
    clear_vehicle_notice(world.vehicles[world.current_slot].race);
    set_level_audio_output(world.audio, 0);
    set_hud_countdown(world.hud, world.controls.current, world.selected_hud, 420);
    world.clock.race_mode = 3;
    world.clock.rolling_state = 1;
    world.clock.phase = 1;
    world.clock.game_mode = 4;
    world.clock.lap_count = 100;
    world.state.announcement_ticks = 240;
    return placed;
}
} // namespace sgn::retail
