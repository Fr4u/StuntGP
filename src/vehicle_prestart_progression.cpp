#include "sgn/vehicle_prestart_progression.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_signed;
using sgn::bit_reinterpret::as_unsigned;
std::uint32_t source_integer(double value)
{
    if (!std::isfinite(value) || value < -9223372036854775808.0 || value >= 9223372036854775808.0)
        throw FormatError("race finish int64 conversion");
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
}
void domain(const VehicleProgressionContext &c, const VehicleRaceFinishWorld *world = nullptr)
{
    if (c.clock.game_mode != 1 || c.current >= 6)
        throw FormatError("prestart progression requires mode1");
    if (world && (&c.clock != &world->clock || &c.records != &world->records))
        throw FormatError("prestart progression world owners disagree");
}
void store_time(VehicleRaceState &race, VehicleProgressionState &state, unsigned index, std::int32_t value)
{
    if (index < 24)
        race.lap_times[index] = value;
    else if (index == 24)
        state.lap_time_24 = as_unsigned(value);
    else if (index == 25)
        state.lap_scores[0] = as_unsigned(value);
    else
        throw FormatError("prestart lap index exceeds source storage");
}
} // namespace
UnfinishedVehicleCounts count_unfinished_vehicles(std::span<const VehicleState, 6> physical,
                                                  std::span<const VehicleRaceState, 6> race, std::uint32_t hud_block)
{
    UnfinishedVehicleCounts result;
    for (unsigned j = 0; j < 6; ++j)
        if (race[j].active == 1 && physical[j].energy_exempt == 0)
        {
            if (physical[j].upgrade_active == 1)
                ++result.upgraded;
            else
                ++result.other;
        }
    if (hud_block == 1)
        result.other = 1;
    return result;
}
unsigned count_active_vehicles(std::span<const VehicleRaceState, 6> race) noexcept
{
    unsigned result = 0;
    for (const auto &car : race)
        if (car.active == 1)
            ++result;
    return result;
}
void finish_vehicle_race(VehicleRaceFinishWorld &world, unsigned current)
{
    if (current >= 6 || world.finish.count >= world.finish.order.size())
        throw FormatError("race finish result identity");
    auto &s = world.physical[current];
    auto &race = world.race[current];
    auto &finish = world.finish;
    s.energy_exempt = 1;
    race.finish_state = race.place;
    race.stunt_score = source_integer(s.score);
    finish.order[finish.count++] = current;
    if (as_unsigned(race.race_timer) < finish.best_total_ticks)
        finish.best_total_ticks = as_unsigned(race.race_timer);
    if (race.stunt_score > finish.best_total_score)
        finish.best_total_score = race.stunt_score;
    const auto active = count_active_vehicles(world.race);
    const auto unfinished = count_unfinished_vehicles(world.physical, world.race, world.hud_block);
    if (unfinished.other != 0 || unfinished.upgraded == 0 || finish.count >= active)
        return;
    // 41DB1C..41DB44: multiply150 wraps in32bits before unsigned /100 and FST.
    const float lap_penalty = vehicle_f32(double((world.records.lap_ticks * 150u) / 100u));
    for (auto index = finish.count; index < active; index = finish.count)
    {
        if (index >= world.ranking.slots.size() || !world.ranking.slots[index] || *world.ranking.slots[index] >= 6)
            throw FormatError("race finish ranking identity");
        const auto id = *world.ranking.slots[index];
        auto &other = world.race[id];
        auto &physical = world.physical[id];
        physical.energy_exempt = 1;
        other.finish_state = other.place;
        other.stunt_score = source_integer(physical.score);
        double remaining = (100.0 - std::fmod(double(other.progress), 100.0)) * std::bit_cast<float>(0x3c23d70au);
        const auto laps = as_signed(world.clock.lap_count - as_unsigned(other.race_index));
        if (laps > 0)
            remaining += laps;
        other.race_timer = as_signed(as_unsigned(other.race_timer) + source_integer(double(lap_penalty) * remaining));
        if (other.best_lap_ticks == 216000)
        {
            other.best_lap_ticks = as_unsigned(other.race_timer);
            other.total_ticks = other.stunt_score;
        }
        if (as_unsigned(other.race_timer) < finish.best_total_ticks)
            finish.best_total_ticks = as_unsigned(other.race_timer);
        if (other.stunt_score > finish.best_total_score)
            finish.best_total_score = other.stunt_score;
        finish.order[index] = id;
        ++finish.count;
    }
}
void complete_prestart_lap(const VehicleState &s, VehicleRaceState &race, VehicleProgressionState &state,
                           AiAutomaticMemory &ai, const VehicleProgressionContext &c, VehicleRaceFinishWorld &world)
{
    domain(c, &world);
    if (&s != &world.physical[c.current] || &race != &world.race[c.current])
        throw FormatError("prestart current owners disagree");
    if (c.clock.phase == 3)
        return;
    const auto index = as_unsigned(race.race_index);
    if (index > 25)
        throw FormatError("prestart lap index exceeds source stable storage");
    race.timing26a8 = 0;
    score_vehicle_lap(s, race, state, c.records);
    record_vehicle_lap(race, state, c);
    race.split_timer = race.lap_timer;
    store_time(race, state, index, race.lap_timer);
    if (index > 24)
        store_time(race, state, index, 0);
    else
        race.race_index = as_signed(index + 1u);
    race.lap_index = as_signed(as_unsigned(race.lap_index) + 1u);
    race.lap_timer = 0;
    ai.counter2674 = 0;
    if (c.clock.lap_count != 100 && as_unsigned(race.race_index) > c.clock.lap_count && s.energy_exempt == 0)
        finish_vehicle_race(world, c.current);
}
void check_prestart_checkpoints(const VehicleRaceState &race, VehicleProgressionState &state,
                                const VehicleProgressionContext &c)
{
    domain(c);
    if (race.lap_index < 0)
        return;
    constexpr std::array<float, 10> thresholds{10, 20, 30, 40, 50, 60, 70, 80, 90, 99};
    const auto index = state.checkpoint_index;
    if (index >= 10)
        throw FormatError("prestart checkpoint index");
    const float previous = vehicle_f32(std::fmod(double(state.previous_projection), 100.0));
    const float current = vehicle_f32(std::fmod(double(race.progress), 100.0));
    if (previous <= thresholds[index] && current >= thresholds[index])
    {
        state.checkpoint_times[index] = as_unsigned(race.race_index) * 12000u + as_unsigned(race.lap_timer);
        if (++state.checkpoint_index >= 10)
            state.checkpoint_index = 0;
    }
}
VehicleProgressionObservation tick_prestart_progression(const VehicleState &s, VehicleRaceState &race,
                                                        VehicleProgressionState &state, AiAutomaticMemory &ai,
                                                        const VehicleProgressionContext &c,
                                                        VehicleRaceFinishWorld &world)
{
    domain(c, &world);
    if (&s != &world.physical[c.current] || &race != &world.race[c.current])
        throw FormatError("prestart current owners disagree");
    tick_vehicle_race_timers(s, race, c.clock);
    if (!c.start_spline)
        return {};
    auto observation = project_vehicle_progress(race, state, {s.position.x, s.position.y, s.position.z},
                                                *c.start_spline, *c.start_spline, c.unit_gravity);
    const auto previous = (race.previous_progress % 100 + 100) % 100,
               current = (race.current_progress % 100 + 100) % 100;
    if (previous == 99 && current == 0)
    {
        if (race.progress_crossing == 0)
        {
            race.progress_crossing = 1;
            complete_prestart_lap(s, race, state, ai, c, world);
        }
    }
    else
        race.progress_crossing = 0;
    check_vehicle_wrong_way(s, race, state, c);
    check_prestart_checkpoints(race, state, c);
    return observation;
}
} // namespace sgn::retail
