#include "sgn/retail_time_trial_lifecycle.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
unsigned selected(const TimeTrialLifecycleWorld &world)
{
    const auto &controls = world.race.controls;
    if (controls.current >= 4 || controls.controllers[controls.current].vehicle >= 6)
        throw FormatError("trial lifecycle controller identity");
    return controls.controllers[controls.current].vehicle;
}
} // namespace
void validate_time_trial_lifecycle_bindings(const TimeTrialLifecycleWorld &world)
{
    const auto &start = world.start;
    const auto &race = world.race;
    const auto &ghosts = world.ghosts;
    if (world.game_type != 4 || (race.clock.game_mode != 1 && race.clock.game_mode != 4))
        throw FormatError("trial lifecycle requires game_type4 and mode1/4");
    if (&start.clock != &race.clock || &ghosts.clock != &race.clock || &start.controls != &race.controls ||
        &ghosts.controls != &race.controls || &start.current_slot != &race.current_slot ||
        &ghosts.current_slot != &race.current_slot || &start.replays != &ghosts.replays ||
        &start.ghosts != &ghosts.ghosts || &start.selection != &ghosts.selection || &start.state != &ghosts.start ||
        start.ai.data() != race.ai.data() || ghosts.physical.data() != race.physics.data() ||
        ghosts.race.data() != race.race.data())
        throw FormatError("trial lifecycle split world owners");
    if (race.current_slot >= 6 || world.load_selection.template_vehicle >= 6)
        throw FormatError("trial lifecycle current vehicle identity");
    (void)selected(world);
    for (unsigned j = 0; j < 6; ++j)
        if (start.vehicles[j].id != j || &start.vehicles[j].physics != &race.physics[j] ||
            &start.vehicles[j].race != &race.race[j] || &start.vehicles[j].contact != &world.contact[j])
            throw FormatError("trial lifecycle split car owners");
}
void disable_all_vehicle_controls(std::span<VehicleState, 6> physical, std::span<VehicleRaceState, 6> race) noexcept
{
    for (unsigned j = 0; j < 6; ++j)
        disable_vehicle_control(race[j], physical[j]);
}
void check_time_trial_completion(TimeTrialLifecycleWorld &world)
{
    validate_time_trial_lifecycle_bindings(world);
    auto &clock = world.race.clock;
    if (clock.phase != 2 || clock.game_mode == 4)
        return;
    // 44385F..44388A, actual mode1 path. Both zero writes phase3 and disables
    // all six cars once; the other paths preserve phase and active controls.
    const auto counts = count_unfinished_vehicles(world.race.physics, world.race.race, world.hud_block);
    if (counts.other == 0 && counts.upgraded == 0)
    {
        clock.phase = 3;
        disable_all_vehicle_controls(world.race.physics, world.race.race);
        return;
    }
    if (clock.phase == 3)
        disable_all_vehicle_controls(world.race.physics, world.race.race); // 443843 guard
}
bool dispatch_time_trial_start(TimeTrialLifecycleWorld &world)
{
    validate_time_trial_lifecycle_bindings(world);
    world.start.state.phase_ticks = 0;
    return start_time_trial(world.start, world.rolling_start_offset);
}
bool tick_time_trial_lifecycle(TimeTrialLifecycleWorld &world)
{
    validate_time_trial_lifecycle_bindings(world);
    auto &clock = world.race.clock;
    bool placed = false;
    if (clock.countdown_pending != 0)
    {
        --clock.countdown_pending;
        // 443A91 is signedJG, not an unsigned countdown comparison.
        if (std::bit_cast<std::int32_t>(clock.countdown_pending) <= 0)
        {
            placed = dispatch_time_trial_start(world);
            clock.countdown_pending = 0;
        }
    }
    check_time_trial_completion(world);
    if (clock.game_mode == 1)
        return placed;
    world.race.current_slot = selected(world);
    world.load_selection.template_vehicle = world.race.current_slot;
    tick_time_trial_ghosts(world.ghosts);
    tick_vehicle_notice(world.race);
    tick_race_countdown(world.race);
    check_rolling_start(clock, world.race.race[world.race.current_slot], world.race.race);
    auto vehicle = selected(world);
    refill_vehicle(world.parameters[vehicle], world.race.physics[vehicle]);
    vehicle = selected(world);
    refill_impact_energy(world.race.physics[vehicle], world.contact[vehicle]);
    return placed;
}
} // namespace sgn::retail
