#include "sgn/vehicle_challenge_progression.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_signed;
using sgn::bit_reinterpret::as_unsigned;
void domain(const VehicleProgressionContext &c)
{
    if (c.clock.game_mode != 3 || c.clock.lap_count != 100 || c.current >= 6)
        throw FormatError("challenge progression requires authored mode3/lap_count100 domain");
}
void domain(const ChallengeClockWorld &w)
{
    if (w.clock.game_mode != 3 || w.current >= 6)
        throw FormatError("challenge clock mode/current identity");
}
std::uint32_t integer(double value)
{
    if (!std::isfinite(value) || value < -9223372036854775808.0 || value >= 9223372036854775808.0)
        throw FormatError("challenge score outside source int64 conversion domain");
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
}
void disable_all(ChallengeClockWorld &w)
{ // 443730, all six even inactive records
    for (unsigned j = 0; j < 6; ++j)
        disable_vehicle_control(w.race[j], w.physical[j]);
}
void notice(ChallengeClockWorld &w, std::uint32_t duration, std::string_view text, std::uint32_t audience,
            std::uint32_t priority)
{
    contact::player::post_notice(w.controls, as_signed(w.clock.phase), w.tick, duration, text, w.current, audience,
                                 priority);
}
} // namespace
void complete_challenge_lap(const VehicleState &physical, VehicleRaceState &race, VehicleProgressionState &state,
                            AiAutomaticMemory &ai, const VehicleProgressionContext &c)
{
    domain(c);
    if (c.clock.phase == 3)
        return;
    const auto index = as_unsigned(race.race_index);
    if (index > 25)
        throw FormatError("challenge lap index exceeds source storage");
    race.timing26a8 = 0;
    score_vehicle_lap(physical, race, state, c.records);
    // 41DD81..41DD8D deliberately skips41DC30 in mode3.
    race.split_timer = race.lap_timer;
    const auto store = [&](std::uint32_t value) {
        if (index < 24)
            race.lap_times[index] = as_signed(value);
        else if (index == 24)
            state.lap_time_24 = value;
        else
            state.lap_scores[0] = value;
    };
    store(as_unsigned(race.lap_timer));
    if (index > 24)
        store(0);
    else
        race.race_index = as_signed(index + 1u);
    race.lap_index = as_signed(as_unsigned(race.lap_index) + 1u);
    race.lap_timer = 0;
    ai.counter2674 = 0;
    // The authored100 skips41DA70; mode3 skips41D8A0/41D8F0.
}
VehicleProgressionObservation tick_challenge_progression(const VehicleState &physical, VehicleRaceState &race,
                                                         VehicleProgressionState &state, AiAutomaticMemory &ai,
                                                         const VehicleProgressionContext &c)
{
    domain(c);
    tick_vehicle_race_timers(physical, race, c.clock);
    if (!c.start_spline)
        return {};
    const auto result =
        project_vehicle_progress(race, state, {physical.position.x, physical.position.y, physical.position.z},
                                 *c.start_spline, *c.start_spline, c.unit_gravity);
    const auto before = (race.previous_progress % 100 + 100) % 100, after = (race.current_progress % 100 + 100) % 100;
    if (before == 99 && after == 0)
    {
        if (race.progress_crossing == 0)
        {
            race.progress_crossing = 1;
            complete_challenge_lap(physical, race, state, ai, c);
        }
    }
    else
        race.progress_crossing = 0;
    // 41E0F0 and41D760 both take their exact mode3 early returns.
    return result;
}
void tick_challenge_clock(ChallengeClockWorld &w)
{
    domain(w);
    if (w.clock.phase != 2 || w.clock.remaining_ticks == 0)
        return;
    if (w.clock.remaining_ticks != 1)
    {
        --w.clock.remaining_ticks;
        return;
    }
    // Absolute source aliases refer to slot0, not necessarily current.
    auto &first = w.race[0];
    first.stunt_score = integer(w.physical[0].score);
    first.best_lap_ticks = 0;
    first.total_ticks = 0;
    VehicleStuntContext context{as_signed(w.clock.phase),
                                as_signed(w.clock.game_mode),
                                as_signed(w.score_block),
                                as_signed(w.hud_block),
                                w.tick,
                                0,
                                {},
                                w.audio};
    award_stunt_score(w.physical[w.current], w.controls, w.current, w.score_popups, context, 5, 0,
                      w.race[w.current].stunt_score);
    w.physical[w.current].score = vehicle_f32(double(as_signed(first.stunt_score))); // signed FILD at443804
    notice(w, 300, w.time_up_text, 0, 100);
    w.clock.remaining_ticks = 0;
    w.score_block = 180;
    disable_all(w);
    if (w.clock.phase == 3)
        disable_all(w); // 443843..44384C
}
void tick_challenge_end_countdown(ChallengeClockWorld &w)
{
    domain(w);
    if (w.score_block != 0 && --w.score_block == 0)
    {
        w.physical[0].energy_exempt = 1;
        w.clock.remaining_ticks = 0;
        w.clock.phase = 3;
        return;
    }
    const auto remaining = w.clock.remaining_ticks;
    if (remaining == 0 || remaining > 600)
        return;
    if (remaining == 600)
        notice(w, 180, w.ten_seconds_text, 2, 0);
    if (remaining % 60 == 0)
        w.audio.countdown_beep(27050u - (remaining * 500u) / 60u, 255u - (remaining * 10u) / 60u);
}
} // namespace sgn::retail
