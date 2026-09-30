#include "sgn/retail_race_prepare.hpp"
#include <cmath>

namespace sgn::retail
{
void clear_race_startup_counters(RaceStartupCounters &state) noexcept
{
    for (auto &counters : state.fields_485650)
        counters.fill(0);
}
void reset_current_race_hud(VehicleStorage &storage, unsigned current, RaceStartupCounters &state)
{
    if (current >= VehicleStorage::physical_slots)
        throw FormatError("race HUD current vehicle outside six slots");
    state.field_60d71c = 0;
    storage.race[current].start_countdown = 0;
    storage.startup[current].fields_768c.fill(0);
    clear_vehicle_notice(storage.race[current]);
    clear_race_startup_counters(state);
}
void activate_vehicle_for_race(VehicleRaceState &race, VehicleRaceStartupState &startup) noexcept
{
    if (race.active != 0)
    {
        enable_vehicle_control(race);
        startup.field_2280 = 1;
    }
}
void prepare_vehicles_for_race(VehicleStorage &storage) noexcept
{
    for (unsigned i = 0; i < VehicleStorage::physical_slots; ++i)
    {
        auto &race = storage.race[i];
        auto &physics = storage.physics[i];
        auto &effects = storage.model_effects[i];
        race.collision_hold = 0;
        physics.score = 1;
        physics.launch_score = 1;
        physics.air_score_rate = 0;
        race.control_enabled = 1;
        physics.gear = 1;
        physics.turbo_active = 0;
        physics.turbo_rpm = 0;
        physics.turbo_force = 0;
        effects.phase = 0;
        physics.launch_effect_level = 0;
        physics.launch_effect_strength = 0;
        physics.launch_effect_timer = 0;
        storage.tick[i].start_boost_ticks = 0;
        effects.enabled = 0;
        physics.steering = 0;
        for (auto &wheel : physics.wheels)
            wheel.surface_grip = 1;
        race.start_countdown = 0;
        clear_vehicle_notice(race);
        refill_vehicle(storage.parameters[i], physics);
        refill_impact_energy(physics, storage.contact[i]);
    }
}
float measure_race_spline_length(SplineView spline)
{
    auto previous = spline_point(spline, 0).value;
    double length = 0;
    for (std::uint32_t i = 0; i < spline.descriptor.point_count; ++i)
    {
        const auto point = spline_point(spline, i).value;
        const double dx = double(point.x) - previous.x, dy = double(point.y) - previous.y,
                     dz = double(point.z) - previous.z;
        length += std::sqrt((dz * dz + dy * dy) + dx * dx);
        previous = point;
    }
    return static_cast<float>(length);
}
void reset_all_race_records(VehicleStorage &storage, unsigned &current, RaceClockState &clock, const SplineSet &splines)
{
    const auto descriptors = splines.descriptors();
    if (descriptors.empty())
        throw FormatError("race record reset requires an authored first spline descriptor");
    current = 0;
    for (auto &race : storage.race)
    {
        reset_race_record(race, storage.physics[current]);
        ++current;
    }
    current = 0;
    // 41D390 selects the first raw descriptor, then 428840 measures its points.
    // The authored descriptor length is not the result of this entry.
    clock.course_length = measure_race_spline_length(*splines.find(descriptors.front().id));
}
} // namespace sgn::retail
