#include "sgn/vehicle_tick.hpp"
#include "sgn/retail_race_start.hpp"
#include <bit>

namespace sgn::retail
{
unsigned performance_reference(std::span<const VehicleRaceState, 6> world) noexcept
{
    // 00408180..004081C2: first active==1 with place==0, otherwise slot2.
    for (unsigned j = 0; j < 6; ++j)
        if (world[j].active == 1 && world[j].place == 0)
            return j;
    return 2;
}
void update_vehicle_performance(VehicleState &s, const VehicleRestartState &restart, VehicleTickState &tick,
                                std::span<const VehicleRaceState, 6> world, unsigned current,
                                const VehiclePerformanceSettings &settings)
{
    if (current >= world.size())
        throw std::runtime_error("performance current vehicle identity");
    if (restart.mode_2630 == 0)
        return;
    const auto other = performance_reference(world);
    s.performance_multiplier = 1;
    const double gap = ((double(world[current].progress) - world[other].progress) * settings.route_length) * 0.01f;
    tick.performance_gap = vehicle_f32(gap);
    s.performance_multiplier = 1;
    if (other == current)
        return;
    const double shifted = gap - 1000.0f;
    float distance = vehicle_f32(shifted); // 0040823D stores, but comparison retains shifted.
    if (!(shifted < 0))
        return;
    const float limit = s.upgrade_active == 1 ? settings.upgraded_distance : settings.normal_distance;
    const float gain = s.upgrade_active == 1 ? settings.upgraded_gain : settings.normal_gain;
    if (distance < -double(limit))
        distance = -limit;
    double factor = (std::abs(double(distance)) / limit) * 2;
    if (factor > 1)
        factor = 1;
    s.performance_multiplier = vehicle_f32(double(gain) * factor + 1.0f);
}
void reset_vehicle_com(const VehicleParameters &p, VehicleState &s, contact::solver::State &contact) noexcept
{
    s.effective_com_height = p.com_height;
    contact.com_262c = 0;
    contact.com_2628 = 0;
    contact.com_reset_ticks = 0;
}
void update_vehicle_com(const VehicleParameters &p, VehicleState &s, contact::solver::State &contact, Vec3 gravity)
{
    // 00410B30..00410CC8. Wheel tests are signed; the reset timer is tested unsigned.
    const auto a = s.wheels[0].contact_countdown, b = s.wheels[1].contact_countdown;
    const auto c = s.wheels[2].contact_countdown, d = s.wheels[3].contact_countdown;
    if ((c <= 0 && d <= 0 && a > 2 && b > 2) || (a <= 0 && c <= 0 && b > 2 && d > 2) ||
        (b <= 0 && d <= 0 && a > 2 && c > 2))
    {
        contact.com_262c = std::bit_cast<std::uint32_t>(500.0f);
        contact.com_reset_ticks = 24;
    }
    unsigned unsupported = 0, supported = 0;
    for (const auto &wheel : s.wheels)
    {
        unsupported += wheel.contact_countdown <= 0;
        supported += wheel.contact_countdown > 2;
    }
    if (unsupported == 4 || supported >= 3)
    {
        reset_vehicle_com(p, s, contact);
        return;
    }
    auto timer = std::bit_cast<std::uint32_t>(contact.com_reset_ticks);
    if (timer == 0)
        return;
    contact.com_reset_ticks = std::bit_cast<std::int32_t>(timer - 1);
    const double dot = (double(gravity.z) * s.up.z + double(gravity.y) * s.up.y) + double(gravity.x) * s.up.x;
    double square = dot * dot;
    if (dot < 0)
        square = -square;
    const double target = (square + 1.0f) * std::bit_cast<float>(contact.com_262c);
    double delta = target - std::bit_cast<float>(contact.com_2628);
    if (delta < 0.01f)
        delta = 0.01f;
    else if (delta > 1.0f)
        delta = 1.0f;
    s.effective_com_height = vehicle_f32(double(p.com_height) - target * delta);
    contact.com_2628 = std::bit_cast<std::uint32_t>(vehicle_f32(target));
}
void update_vehicle_start_boost(const VehicleParameters &p, VehicleState &s, const VehicleRaceState &race,
                                contact::solver::State &contact, VehicleTickState &state,
                                const VehicleStartBoostContext &context)
{
    // 00412670..00412770, including full refill and notice callees.
    if (context.current >= 6)
        throw std::runtime_error("start boost vehicle identity");
    if (context.race_mode == 3)
        return;
    if (state.start_boost_ticks != 0)
    {
        --state.start_boost_ticks;
        s.performance_multiplier = vehicle_f32(double(state.start_boost_ticks) * 0.0055555556900799274f + 1.0f);
        s.turbo_active = 1;
        refill_vehicle(p, s);
        refill_impact_energy(s, contact);
        return;
    }
    if (race.start_countdown == 0 || s.control_flags[0] != 0 || s.control_flags[2] != 1)
        return;
    if (!(double(p.rpm_max) * 0.2f < double(s.rpm)))
        return;
    const auto difference = race.start_countdown - 60u;
    const auto magnitude = std::bit_cast<std::int32_t>(difference) < 0 ? 0u - difference : difference;
    if (std::bit_cast<std::int32_t>(magnitude) >= 10)
        return;
    state.start_boost_ticks = 180;
    s.rpm = p.rpm_max;
    contact::player::post_notice(context.controls, context.phase, context.tick, 180, context.text, context.current, 2,
                                 90);
    s.score = vehicle_f32(double(context.score_award) + s.score);
}
} // namespace sgn::retail
