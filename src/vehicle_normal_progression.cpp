#include "sgn/vehicle_normal_progression.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>
namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_signed;
using sgn::bit_reinterpret::as_unsigned;
std::string text(std::string_view v)
{
    return std::string(v.substr(0, v.find('\0')));
}
void domain(const VehicleProgressionContext &c, const NormalRaceProgression &n)
{
    if (c.clock.game_mode != 2 || c.current >= 6 || &c.clock != &n.finish.clock || &c.records != &n.finish.records)
        throw FormatError("normal progression source owners/mode");
}
void notice(const VehicleProgressionContext &c, std::string_view message)
{
    contact::player::post_notice(c.controls, as_signed(c.clock.phase), c.tick, 180, message, c.current, 2, 0);
}
void lap_notice(const VehicleRaceState &race, const VehicleProgressionContext &c)
{ // 41D8F0
    const auto index = as_unsigned(race.race_index);
    if (index >= c.clock.lap_count)
        return;
    std::string message =
        "LAP " + std::to_string(as_signed(index)) + "/" + std::to_string(as_signed(c.clock.lap_count));
    if (index != 1)
        message += " - TIME: " + format_progression_time(as_unsigned(race.split_timer));
    if (message.size() >= 64)
        throw FormatError("normal lap notice exceeds source local");
    notice(c, message);
}
} // namespace
void announce_normal_checkpoint(const VehicleRaceState &race, const VehicleProgressionState &state,
                                const VehicleProgressionContext &c, const NormalRaceProgression &n)
{
    domain(c, n);
    const auto second = n.finish.ranking.slots[1];
    if (!second || race.lap_index < 0 || as_unsigned(race.race_index) >= c.clock.lap_count)
        return;
    unsigned other{}, index{};
    if (race.place == 0)
    {
        other = *second;
        if (other >= 6)
            throw FormatError("normal checkpoint second identity");
        if (as_unsigned(n.finish.race[other].race_index) < 1)
            return;
        const auto previous = n.progressions[other].checkpoint_index - 1u;
        index = as_signed(previous) < 0 ? 9u : previous;
    }
    else
    {
        const auto at = race.place - 1u;
        if (at >= 6 || !n.finish.ranking.slots[at])
            throw FormatError("normal checkpoint preceding identity");
        other = *n.finish.ranking.slots[at];
        index = state.checkpoint_index;
    }
    if (other >= 6 || index >= 10)
        throw FormatError("normal checkpoint field identity");
    const auto delta = state.checkpoint_times[index] - n.progressions[other].checkpoint_times[index];
    const auto magnitude = as_signed(delta) < 0 ? 0u - delta : delta;
    if (as_signed(magnitude) > 1800)
        return;
    const auto message = text(c.names[other]) + " " + text(c.checkpoint_text) + " " +
                         format_progression_time(magnitude) + " " + text(c.units_text) + " " +
                         text(race.place == 0 ? n.leading_text : n.following_text) + "!";
    if (message.size() >= 32)
        throw FormatError("normal checkpoint notice exceeds source local");
    notice(c, message);
}
void complete_normal_lap(const VehicleState &s, VehicleRaceState &race, VehicleProgressionState &state,
                         AiAutomaticMemory &ai, const VehicleProgressionContext &c, NormalRaceProgression &n)
{
    domain(c, n);
    if (&s != &n.finish.physical[c.current] || &race != &n.finish.race[c.current])
        throw FormatError("normal lap current owners");
    if (c.clock.phase == 3)
        return;
    const auto index = as_unsigned(race.race_index);
    if (index > 25)
        throw FormatError("normal lap index exceeds source storage");
    race.timing26a8 = 0;
    score_vehicle_lap(s, race, state, c.records);
    record_vehicle_lap(race, state, c);
    race.split_timer = race.lap_timer;
    const auto store = [&](std::int32_t value) {
        if (index < 24)
            race.lap_times[index] = value;
        else if (index == 24)
            state.lap_time_24 = as_unsigned(value);
        else
            state.lap_scores[0] = as_unsigned(value);
    };
    store(race.lap_timer);
    if (index > 24)
        store(0);
    else
        race.race_index = as_signed(index + 1u);
    race.lap_index = as_signed(as_unsigned(race.lap_index) + 1u);
    race.lap_timer = 0;
    ai.counter2674 = 0;
    if (c.clock.lap_count != 100 && as_unsigned(race.race_index) > c.clock.lap_count && s.energy_exempt == 0)
        finish_vehicle_race(n.finish, c.current);
    if (race.active == 1 && contact::player::find_controller(c.controls, c.current) &&
        as_unsigned(race.race_index) > 1 && as_unsigned(race.race_index) == c.clock.lap_count)
        notice(c, n.last_lap_text); // 41D8A0 exact ==lap_count guard
    if (n.split_setting != 0)
        lap_notice(race, c);
}
void check_normal_checkpoints(const VehicleRaceState &race, VehicleProgressionState &state,
                              const VehicleProgressionContext &c, const NormalRaceProgression &n)
{
    domain(c, n);
    if (race.lap_index < 0)
        return;
    constexpr std::array<float, 10> thresholds{10, 20, 30, 40, 50, 60, 70, 80, 90, 99};
    const auto index = state.checkpoint_index;
    if (index >= thresholds.size())
        throw FormatError("normal checkpoint cursor");
    const float before = vehicle_f32(std::fmod(double(state.previous_projection), 100.0)),
                after = vehicle_f32(std::fmod(double(race.progress), 100.0));
    if (before <= thresholds[index] && after >= thresholds[index])
    {
        if (index == 9)
            announce_normal_checkpoint(race, state, c, n); // 41D7E6 before current timestamp write
        state.checkpoint_times[state.checkpoint_index] =
            as_unsigned(race.race_index) * 12000u + as_unsigned(race.lap_timer);
        if (++state.checkpoint_index >= 10)
            state.checkpoint_index = 0;
    }
}
VehicleProgressionObservation tick_normal_progression(const VehicleState &s, VehicleRaceState &race,
                                                      VehicleProgressionState &state, AiAutomaticMemory &ai,
                                                      const VehicleProgressionContext &c, NormalRaceProgression &n)
{
    domain(c, n);
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
            complete_normal_lap(s, race, state, ai, c, n);
        }
    }
    else
        race.progress_crossing = 0;
    check_vehicle_wrong_way(s, race, state, c);
    check_normal_checkpoints(race, state, c, n);
    return observation;
}
} // namespace sgn::retail
