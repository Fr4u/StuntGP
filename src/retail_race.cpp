#include "sgn/retail_race.hpp"

namespace sgn::retail
{
void reset_race_record(VehicleRaceState &s, VehicleState &physics) noexcept
{
    s.lap_times.fill(-1);
    s.lap_timer = -1;
    s.split_timer = -1;
    s.race_timer = -1;
    s.stunt_score = 0;
    s.best_lap_ticks = 216000;
    s.total_ticks = 0;
    s.timing26a8 = 0;
    s.race_index = 0;
    s.lap_index = -1;
    s.previous_progress = -2;
    s.current_progress = -1;
    s.previous_fraction = -1;
    s.spline_index = 0;
    s.progress_crossing = 0;
    s.event7678 = 0;
    physics.energy_exempt = 0;
    s.finish_state = 0;
}
void enable_vehicle_control(VehicleRaceState &s) noexcept
{
    if (s.active == 1 && s.control_enabled == 0)
        s.control_enabled = 1;
}
void disable_vehicle_control(VehicleRaceState &race, VehicleState &physics) noexcept
{
    if (race.active == 1 && race.control_enabled == 1)
    {
        race.control_enabled = 0;
        physics.throttle = 0;
    }
}
void enable_all_controls(std::span<VehicleRaceState, 6> cars) noexcept
{
    for (auto &car : cars)
        enable_vehicle_control(car);
}
void begin_lap_timers(std::span<VehicleRaceState, 6> cars) noexcept
{
    for (auto &car : cars)
        if (car.active == 1 && car.race_timer == -1)
            car.race_timer = 0;
}
void arm_start_countdown(std::span<VehicleRaceState, 6> cars) noexcept
{
    // The original loop writes all six slots, including inactive ghosts.
    for (auto &car : cars)
        car.start_countdown = 301;
}
void check_rolling_start(RaceClockState &race, const VehicleRaceState &current,
                         std::span<VehicleRaceState, 6> cars) noexcept
{
    // 00443A43..00443A65, binary32 0.01 at 0046E294 promoted for x87 operations.
    if (race.rolling_state != 480 &&
        double(race.course_length) - 30000.0 < (double(race.course_length) * current.progress) * double(0.01f))
    {
        race.rolling_state = 480;
        arm_start_countdown(cars);
    }
}
void clear_vehicle_notice(VehicleRaceState &s) noexcept
{
    s.notice_active = 0;
    s.notice_ticks = 0;
}
void reset_route_tracking(VehicleRaceState &s) noexcept
{
    s.route_indices[1] = 0;
    s.route_fractions[1] = 0;
    s.route_indices[0] = 0;
    s.route_fractions[0] = 0;
}
} // namespace sgn::retail
