#include "sgn/vehicle_progression.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cstring>
#include <limits>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_signed;
using sgn::bit_reinterpret::as_unsigned;
void time_trial_domain(const VehicleProgressionContext &c)
{
    if (c.clock.game_mode != 4 || c.clock.lap_count != 100)
        throw FormatError("progression: only source TimeTrial mode4/lap_count100 qualified");
    if (c.current >= 6)
        throw FormatError("progression vehicle identity");
}
unsigned lap_index(const VehicleRaceState &race)
{
    const auto index = as_unsigned(race.race_index);
    if (index > 25)
        throw FormatError("progression lap index exceeds original stable 0..25 domain");
    return index;
}
std::uint32_t &lap_score(VehicleRaceState &race, VehicleProgressionState &state, unsigned index)
{
    if (index == 25)
        return race.spline_index;
    return state.lap_scores.at(index);
}
void store_lap_time(VehicleRaceState &race, VehicleProgressionState &state, unsigned index, std::uint32_t value)
{
    if (index < 24)
        race.lap_times[index] = as_signed(value);
    else if (index == 24)
        state.lap_time_24 = value;
    else if (index == 25)
        state.lap_scores[0] = value;
    else
        throw FormatError("progression lap time index");
}
std::string source_text(std::string_view text)
{
    return std::string(text.substr(0, text.find('\0')));
}
std::uint32_t checkpoint_timestamp(const VehicleRaceState &race) noexcept
{
    return as_unsigned(race.race_index) * 12000u + as_unsigned(race.lap_timer);
}
std::uint32_t record_threshold(const VehicleProgressionContext &c)
{
    const auto at = ReplayBank::base(c.ghosts.roles[0].replay) + 0x1cu;
    std::uint32_t value{};
    std::memcpy(&value, c.replays.bytes().data() + at, 4);
    return value;
}
void arm_record_notice(VehicleRaceState &race) noexcept
{ // 443DE0
    if (race.notice_active != 1)
    {
        race.notice_active = 1;
        race.notice_ticks = 0;
    }
}
} // namespace
void tick_vehicle_race_timers(const VehicleState &s, VehicleRaceState &race, const RaceClockState &clock) noexcept
{
    // 0041DE70..0041DECF: unsigned lap comparison and wrapping INC.
    if (s.energy_exempt == 1)
    {
        race.lap_timer = -1;
        return;
    }
    if (clock.lap_count != 100 && as_unsigned(race.race_index) > clock.lap_count)
    {
        race.lap_timer = 1;
        return;
    }
    if (race.lap_timer != -1)
        race.lap_timer = as_signed(as_unsigned(race.lap_timer) + 1u);
    if (race.race_timer != -1)
        race.race_timer = as_signed(as_unsigned(race.race_timer) + 1u);
}
void accept_vehicle_progress(VehicleRaceState &race, std::int32_t progress) noexcept
{
    // 0041D9C0..0041D9EB uses signed comparisons after a wrapping SUB.
    if (as_signed(as_unsigned(progress) - as_unsigned(race.current_progress)) > 15)
        return;
    if (race.previous_progress < race.current_progress)
        race.previous_progress = race.current_progress;
    race.current_progress = progress;
}
VehicleProgressionObservation project_vehicle_progress(VehicleRaceState &race, VehicleProgressionState &state,
                                                       Vec3 position, SplineView spline, const SplineView &grid,
                                                       Vec3 up)
{
    // 0041DED0..0041E0E8. The supplied segment and global grid count are separate
    // source inputs; no modulo repair of the persistent signed segment is made.
    const auto count = spline.descriptor.point_count, global_count = grid.descriptor.point_count;
    if (count == 0 || global_count == 0 || global_count >= 0x80000000u)
        throw FormatError("progression spline count");
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
        throw FormatError("progression nonfinite position");
    auto segment = race.spline_index;
    int direction = 0;
    bool adjacent = false;
    for (std::uint64_t examined = 0; examined < std::uint64_t(count) * 2 + 2; ++examined)
    {
        const auto selected = as_signed(segment) < 0 ? count - ((0u - segment) % count) : segment % count;
        const auto first = spline_point(spline, selected), next = spline_point(spline, (selected + 1u) % count);
        adjacent |= first.adjacent_storage_read || next.adjacent_storage_read;
        float length{};
        const auto axis = vehicle_normalize({vehicle_f32(double(next.value.x) - first.value.x),
                                             vehicle_f32(double(next.value.y) - first.value.y),
                                             vehicle_f32(double(next.value.z) - first.value.z), race.respawn_heading.w},
                                            &length);
        if (!std::isfinite(length) || length == 0)
            throw FormatError("progression degenerate spline segment");
        const double distance =
            ((double(position.z) - first.value.z) * axis.z + (double(position.y) - first.value.y) * axis.y) +
            (double(position.x) - first.value.x) * axis.x;
        const float projected = vehicle_f32(distance);
        if (distance < 0 && direction <= 0)
        {
            --segment;
            direction = -1;
            continue;
        }
        if (distance >= 0 && projected >= length && direction >= 0)
        {
            ++segment;
            direction = 1;
            continue;
        }
        race.spline_index = segment;
        accept_vehicle_progress(race, as_signed((segment * 100u) / global_count));
        state.previous_projection = race.progress;
        race.progress =
            vehicle_f32(((double(projected) / length + as_signed(segment)) * 100.0f) / as_signed(global_count));
        race.respawn_heading = axis;
        auto right = vehicle_cross(axis, {up.x, up.y, up.z, 0});
        right.w = race.route_right.w;
        race.route_right = vehicle_normalize(right);
        race.respawn_position.x = vehicle_f32(double(vehicle_f32(double(projected) * axis.x)) + first.value.x);
        race.respawn_position.y = vehicle_f32(double(vehicle_f32(double(projected) * axis.y)) + first.value.y);
        race.respawn_position.z = vehicle_f32(double(vehicle_f32(double(projected) * axis.z)) + first.value.z);
        return {true, adjacent};
    }
    throw FormatError("original progression projection failed to converge within two traversals");
}
void score_vehicle_lap(const VehicleState &s, VehicleRaceState &race, VehicleProgressionState &state,
                       ProgressionRecordState &records)
{
    // 0041DCE0..0041DD53: __ftol's low32, then unsigned comparisons.
    const auto index = lap_index(race);
    if (!std::isfinite(s.score) || double(s.score) < -9223372036854775808.0 || double(s.score) >= 9223372036854775808.0)
        throw FormatError("lap score outside source int64 conversion domain");
    const auto total = static_cast<std::uint32_t>(static_cast<std::int64_t>(s.score));
    auto &current = lap_score(race, state, index);
    current = index == 0 ? total : total - state.lap_scores.at(index - 1);
    if (current > race.total_ticks)
        race.total_ticks = current;
    if (current > records.score)
        records.score = current;
}
void record_vehicle_lap(VehicleRaceState &race, VehicleProgressionState &state, const VehicleProgressionContext &c)
{
    // 0041DC30..0041DCD4, full copies41D3B0/41D3D0 and 442FB0/443DE0.
    const auto ticks = as_unsigned(race.lap_timer);
    if (ticks < race.best_lap_ticks)
    {
        race.best_lap_ticks = ticks;
        state.personal_checkpoint_times = state.checkpoint_times;
    }
    if (ticks >= c.records.lap_ticks)
        return;
    c.records.previous_lap_ticks = c.records.lap_ticks;
    c.records.lap_ticks = ticks;
    state.record_checkpoint_times = state.checkpoint_times;
    if (c.clock.game_mode == 4)
    {
        if (!contact::player::find_controller(c.controls, c.current))
            return;
        if (c.record_enabled != 0 && c.records.lap_ticks < record_threshold(c))
            state.record_improved = 1;
        arm_record_notice(race);
    }
    else if (c.clock.game_mode == 2 && as_unsigned(race.race_index) < c.clock.lap_count &&
             contact::player::find_controller(c.controls, c.current))
        arm_record_notice(race);
}
std::string format_progression_time(std::uint32_t ticks)
{
    // 004482A0's fraction units multiply wraps BEFORE unsigned division by6.
    const auto units = ((ticks * 10u) / 6u) % 10u, tens = (ticks / 6u) % 10u;
    const auto seconds = (ticks / 60u) % 60u, minutes = ((ticks / 60u) % 3600u) / 60u;
    std::string out;
    if (minutes / 10u != 0)
        out += std::to_string(minutes / 10u);
    if (minutes != 0)
    {
        out += std::to_string(minutes % 10u);
        out += '.';
        out += std::to_string(seconds / 10u);
    }
    else if (seconds / 10u != 0)
        out += std::to_string(seconds / 10u);
    out += std::to_string(seconds % 10u);
    out += '.';
    out += std::to_string(tens);
    out += std::to_string(units);
    return out; // 41D3F0 literals474898/8A8/8B8/8C4 use periods, not colons.
}
void announce_vehicle_checkpoint(const VehicleRaceState &race, VehicleProgressionState &state,
                                 const VehicleProgressionContext &c)
{
    // 0041D4B0..0041D5B9. Threshold guards signed ABS including INT_MIN.
    const auto index = state.checkpoint_index;
    if (index >= 10 || c.current >= 6)
        throw FormatError("checkpoint identity");
    const auto timestamp = checkpoint_timestamp(race);
    state.checkpoint_times[index] = timestamp;
    if (race.lap_index <= 0)
        return;
    const auto difference = timestamp - state.personal_checkpoint_times[index];
    const auto magnitude = as_signed(difference) < 0 ? 0u - difference : difference;
    if (as_signed(magnitude) > 1800)
        return;
    const auto text = source_text(c.names[c.current]) + " " + source_text(c.checkpoint_text) + " " +
                      format_progression_time(magnitude) + " " + source_text(c.units_text) + " " +
                      source_text(as_signed(difference) < 0 ? c.ahead_text : c.behind_text);
    if (text.size() >= 32)
        throw FormatError("checkpoint announcement exceeds source local 31 bytes");
    contact::player::post_notice(c.controls, static_cast<std::int32_t>(c.clock.phase), c.tick, 180, text, c.current, 0,
                                 0);
}
void check_vehicle_wrong_way(const VehicleState &s, VehicleRaceState &race, const VehicleProgressionState &state,
                             const VehicleProgressionContext &c)
{
    // 0041E0F0..0041E208. The sound call uses the real chain-audio device boundary.
    if (c.clock.game_mode == 1 || c.clock.game_mode == 3)
        return;
    if (race.progress > race.previous_fraction)
        race.previous_fraction = race.progress;
    if (!contact::player::find_controller(c.controls, c.current))
        return;
    if (s.energy_exempt != 1 && race.stopped != 1 && s.special_mode != 1 && race.unsafe_contact_ticks <= 4 &&
        double(race.previous_fraction) - race.progress >= 1.0f && race.progress < state.previous_projection)
    {
        if (race.event7678 == 90)
            c.audio.play_chain(44);
        if (race.event7678 > 120)
        {
            race.event7678 = 60;
            return;
        }
        ++race.event7678;
        return;
    }
    if (race.event7678 != 0)
        --race.event7678;
}
void complete_time_trial_lap(const VehicleState &s, VehicleRaceState &race, VehicleProgressionState &state,
                             AiAutomaticMemory &ai, const VehicleProgressionContext &c)
{
    time_trial_domain(c);
    if (c.clock.phase == 3)
        return;
    const auto index = lap_index(race);
    race.timing26a8 = 0;
    score_vehicle_lap(s, race, state, c.records);
    record_vehicle_lap(race, state, c);
    race.split_timer = race.lap_timer;
    store_lap_time(race, state, index, as_unsigned(race.lap_timer));
    if (index > 24)
        store_lap_time(race, state, index, 0);
    else
        race.race_index = as_signed(index + 1u);
    race.lap_index = as_signed(as_unsigned(race.lap_index) + 1u);
    race.lap_timer = 0;
    ai.counter2674 = 0; // full407000. lap_count100 skips41DA70; mode4 skips41D8A0/41D8F0.
}
void check_time_trial_checkpoints(const VehicleRaceState &race, VehicleProgressionState &state,
                                  const VehicleProgressionContext &c)
{
    time_trial_domain(c);
    if (race.lap_index < 0)
        return;
    constexpr std::array<float, 10> thresholds = {10, 20, 30, 40, 50, 60, 70, 80, 90, 99}; // 474870
    const auto index = state.checkpoint_index;
    if (index >= 10)
        throw FormatError("checkpoint index");
    const float previous = vehicle_f32(std::fmod(double(state.previous_projection), 100.0));
    const float current = vehicle_f32(std::fmod(double(race.progress), 100.0));
    if (previous <= thresholds[index] && current >= thresholds[index])
    {
        if (index == 2 || index == 5 || index == 8)
            announce_vehicle_checkpoint(race, state, c);
        state.checkpoint_times[state.checkpoint_index] = checkpoint_timestamp(race);
        if (++state.checkpoint_index >= 10)
            state.checkpoint_index = 0;
    }
}
VehicleProgressionObservation tick_time_trial_progression(const VehicleState &s, VehicleRaceState &race,
                                                          VehicleProgressionState &state, AiAutomaticMemory &ai,
                                                          const VehicleProgressionContext &c)
{
    time_trial_domain(c);
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
            complete_time_trial_lap(s, race, state, ai, c);
        }
    }
    else
        race.progress_crossing = 0;
    check_vehicle_wrong_way(s, race, state, c);
    check_time_trial_checkpoints(race, state, c);
    return observation;
}
} // namespace sgn::retail
