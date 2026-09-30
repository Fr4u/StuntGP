#include "sgn/retail_race_start.hpp"
#include <bit>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
unsigned selected_vehicle(const contact::player::Controls &controls)
{
    if (controls.current >= 4)
        throw std::out_of_range("race controller selector");
    const auto vehicle = controls.controllers[controls.current].vehicle;
    if (vehicle >= 6)
        throw std::out_of_range("race controller vehicle");
    return vehicle;
}
unsigned current_vehicle(const RaceStartWorld &world)
{
    if (world.current_slot >= 6)
        throw std::out_of_range("race current vehicle");
    return world.current_slot;
}
void select_texture(RaceStartWorld &world, std::uint32_t id)
{
    if (world.presentation.selected_texture() != id)
        world.presentation.texture(15, id);
}
} // namespace
void choose_race_torque_curve(const VehicleRaceDriverSettings &settings, VehicleState &physical,
                              const std::array<float, 3> &thresholds)
{
    // 00405880: unordered and equal compare outcomes both continue to the next
    // threshold. This exact order also handles non-monotonic source tables.
    physical.torque_curve = settings.skill > thresholds[0]   ? 3
                            : settings.skill > thresholds[1] ? 2
                            : settings.skill > thresholds[2] ? 1
                                                             : 0;
}
void configure_race_drivers(RaceStartWorld &world)
{
    // 004058F0..0040595D: six slots; only exact active==1 enters the body.
    for (unsigned i = 0; i < 6; ++i)
        if (world.race[i].active == 1)
        {
            const auto &car = world.cars[i];
            world.ai[i].steering_ramp_ticks = 300;
            if (car.ai_skill == 1)
                world.physics[i].upgrade_active = 0;
            else
            {
                world.drivers[i].skill = vehicle_f32(double(std::bit_cast<std::int32_t>(car.ai_skill)));
                world.drivers[i].mode = vehicle_f32(double(std::bit_cast<std::int32_t>(car.ai_mode)));
                world.physics[i].upgrade_active = 1; // ESI=vehicle+26A4, [ESI-64h]=2640
                choose_race_torque_curve(world.drivers[i], world.physics[i], world.torque_thresholds);
            }
        }
}
void begin_race_timers(RaceStartWorld &world)
{
    configure_race_drivers(world);
    world.clock.phase = 2;
    begin_lap_timers(world.race);
}
void hold_start_vehicles(std::span<VehicleState, 6> physics, std::span<const VehicleRaceState, 6> race,
                         const RaceClockState &clock) noexcept
{
    // 00412780..004127D0. The original advances EAX only inside the active
    // branch. Once an inactive slot is reached, the remaining iterations keep
    // testing that same slot; do not silently change this into a skip traversal.
    if (clock.phase != 1 || clock.race_mode == 3)
        return;
    unsigned current = 0;
    for (unsigned n = 0; n < 6; ++n)
        if (race[current].active == 1)
        {
            auto &vehicle = physics[current];
            vehicle.brake_ramp = std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(vehicle.brake_ramp) + 6u);
            for (auto &wheel : vehicle.wheels)
                wheel.speed = 0;
            ++current;
        }
}
void tick_race_countdown(RaceStartWorld &world)
{
    // 00444C50 sets the global current car even when its countdown is zero.
    world.current_slot = selected_vehicle(world.controls);
    auto &race = world.race[world.current_slot];
    if (race.start_countdown == 0)
        return;
    --race.start_countdown;
    if (race.start_countdown > 60)
        hold_start_vehicles(world.physics, world.race, world.clock);
    const auto count = world.race[current_vehicle(world)].start_countdown;
    if (count > 180)
    {
        if (count == 240)
            world.presentation.sound(44, 255);
        select_texture(world, 19);
    }
    else if (count > 120)
    {
        if (count == 180)
            world.presentation.sound(44, 255);
        select_texture(world, 18);
    }
    else if (count > 60)
    {
        if (count == 120)
            world.presentation.sound(44, 255);
        select_texture(world, 17);
    }
    else if (count != 0)
    {
        select_texture(world, 16);
        if (world.race[current_vehicle(world)].start_countdown == 60)
        {
            world.presentation.sound(45, 255);
            enable_all_controls(world.race);
            begin_race_timers(world);
            world.race[selected_vehicle(world.controls)].automatic_drive = 0;
        }
    }
}
void tick_vehicle_notice(RaceStartWorld &world)
{
    // 00443E20 re-reads the selected vehicle after presentation calls.
    if (world.race[current_vehicle(world)].notice_active == 0)
        return;
    if (world.race[current_vehicle(world)].notice_ticks == 0)
        world.presentation.sound(47, 127);
    select_texture(world, 20);
    auto &race = world.race[current_vehicle(world)];
    ++race.notice_ticks;
    if (race.notice_ticks > 120)
        race.notice_active = 0;
}
void refill_vehicle(const VehicleParameters &parameters, VehicleState &physical) noexcept
{
    physical.fuel = parameters.fuel_capacity;
    physical.energy = physical.energy_capacity;
}
void refill_impact_energy(VehicleState &physical, contact::solver::State &contacts) noexcept
{
    contacts.impact_count.value = 0;
    physical.energy = physical.energy_capacity;
}
} // namespace sgn::retail
