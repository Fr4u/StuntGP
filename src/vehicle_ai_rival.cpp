#include "sgn/vehicle_ai_rival.hpp"
#include "sgn/vehicle_ai.hpp"
#include "sgn/spline_resolution.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
float f32(double x)
{
    return static_cast<float>(x);
}
Vec3 xyz(VehicleVector v)
{
    return {v.x, v.y, v.z};
}
std::optional<unsigned> ranked(const AiRivalWorld &w, unsigned index)
{
    if (index > w.ranking.slots.size())
        throw FormatError("rival AI ranking lookup outside source domain");
    const auto value = index == w.ranking.slots.size() ? w.globals.ranking_tail : w.ranking.slots[index];
    if (value && *value >= 6)
        throw FormatError("rival AI ranking vehicle identity");
    return value;
}
std::uint32_t integer(double x)
{
    if (!std::isfinite(x) || x < -9223372036854775808.0 || x >= 9223372036854775808.0)
        throw FormatError("rival AI source integer conversion");
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(x));
}
AiField observations(const AiRivalWorld &w)
{
    AiField field{};
    for (unsigned j = 0; j < 6; ++j)
    {
        const auto car = w.cars[j];
        const auto &p = car.physical;
        const auto &r = car.race;
        const auto &a = car.ai;
        auto &o = field[j];
        o.active = r.active;
        o.position = xyz(p.position);
        o.right = xyz(p.basis_x);
        o.forward = xyz(p.basis_z);
        o.route_position = xyz(r.respawn_position);
        o.route_right = xyz(r.route_right);
        o.speed = p.speed;
        o.throttle = p.throttle;
        o.route_progress = r.progress;
        o.contact_class = p.contact_class;
        o.bend = a.bend;
        o.race_index = r.race_index;
        o.rival_counters = w.contacts[j].pair_counters.count;
        o.passing_target = a.passing_target;
        o.passing_ticks = a.passing_ticks;
        o.passing_offset = a.passing_offset;
        o.following_ticks = a.following_ticks;
        o.steering = p.steering;
        o.max_steering = car.parameters.steering_max;
        o.up_y = p.up.y;
        o.driving_factor = p.upgrade_condition;
        o.steering_divisor_signal = p.average_grip;
        o.steering_ramp_ticks = a.steering_ramp_ticks;
        o.reverse_ticks = a.reverse_ticks;
        o.gear = p.gear;
    }
    return field;
}
void publish(AiRivalWorld &w, unsigned current, const AiField &field, const AiTactics &tactics)
{
    auto car = w.cars[current];
    const auto &o = field[current];
    auto &a = car.ai;
    car.physical.throttle = o.throttle;
    car.physical.steering = o.steering;
    car.physical.gear = o.gear;
    a.passing_target = o.passing_target;
    a.passing_ticks = o.passing_ticks;
    a.passing_offset = o.passing_offset;
    a.following_ticks = o.following_ticks;
    a.reverse_ticks = o.reverse_ticks;
    w.globals.target = tactics.target;
    w.globals.target_speed = tactics.target_speed;
    w.globals.response_scale = tactics.response_scale;
}
} // namespace
void adjust_ai_skill(AiRivalWorld &w, unsigned current)
{
    auto car = w.cars[current];
    auto &p = car.physical;
    const auto &settings = w.settings;
    if (w.restart[current].mode_2630 == 0)
        return;
    if (car.race.place == 0)
    {
        const auto other = ranked(w, 1);
        if (!other)
            return;
        double gap = -double(w.tick_state[*other].performance_gap) - settings.leader_distance;
        if (gap < 0)
            return;
        gap = (1.0 / settings.leader_distance) * gap;
        if (gap > 1)
            gap = 1;
        const double factor = double(p.upgrade_condition) - gap * settings.leader_penalty;
        p.upgrade_condition = f32(factor);
        if (factor < 0)
            p.upgrade_condition = 0;
    }
    else if (p.upgrade_condition < 1)
    {
        double gap = -double(w.tick_state[current].performance_gap) - settings.boost_distance;
        if (gap < 0)
            return;
        gap = (1.0 / settings.boost_distance) * gap;
        if (gap > 1)
            gap = 1;
        const double factor = gap * settings.boost_gain + p.upgrade_condition;
        p.upgrade_condition = f32(factor);
        if (factor > 1)
            p.upgrade_condition = 1;
    }
}
void adjust_ai_lateral(AiRivalWorld &w, unsigned current)
{
    auto car = w.cars[current];
    const auto other = ranked(w, car.race.place + 1u);
    if (!other)
        return;
    const double difference = double(w.tick_state[current].performance_gap) - w.tick_state[*other].performance_gap;
    const float stored = f32(difference);
    if (difference <= 0 || stored >= 7500)
        return;
    const double scaled = double(stored) * 0.00013333333481568843f;
    double square = scaled * scaled;
    if (square < double(0.1f))
        square = 0;
    double lateral =
        car.ai.lateral_fraction < 0 ? 1 - square + car.ai.lateral_fraction : car.ai.lateral_fraction - (1 - square);
    if (car.ai.lateral_fraction < 0)
    {
        if (lateral > 0)
            lateral = 0;
        else if (lateral < -1)
            lateral = -1;
    }
    else
    {
        if (lateral < 0)
            lateral = 0;
        else if (lateral > 1)
            lateral = 1;
    }
    if (std::abs(lateral) < car.ai.lateral_fraction)
        car.ai.lateral_fraction = f32(lateral);
}
bool ai_turbo_for_speed(AiRivalWorld &w, unsigned current)
{
    const auto car = w.cars[current];
    const auto &p = car.physical;
    if (!(double(p.energy_capacity) * 0.3f < p.energy) || !(p.throttle > .75f) || !(p.speed > 3.3333332538604736f))
        return false;
    return ((1.0 - p.upgrade_condition) * w.settings.turbo_factor + 1) * (double(car.parameters.rpm_max) * 0.4f) >
           p.rpm;
}
bool ai_turbo_for_stunt(AiRivalWorld &w, unsigned current)
{
    const auto car = w.cars[current];
    const auto &p = car.physical;
    if (car.ai.counter2674 >= w.knots.size())
        throw FormatError("rival AI stunt knot cursor");
    const auto &knot = w.knots[car.ai.counter2674];
    if (knot.progress == 0)
        return false;
    if (!w.globals.routes)
        throw FormatError("rival AI stunt requires route cache");
    // clang-format off: keep the trailing comment on one line (ReflowComments would split it)
    const auto length = resolve_ai_routes(w.globals.routes, 0x4077b0)[2].descriptor.length; // R18 call-scoped route view
    // clang-format on
    if (!(double(knot.progress) * length * 0.01f - double(car.race.route_fractions[2]) * length * 0.01f < 10000))
        return false;
    return double(p.energy_capacity) * .5f < p.energy && p.throttle > .5f &&
           double(car.parameters.rpm_max) * .75f > p.rpm;
}
void update_ai_turbo(AiRivalWorld &w, unsigned current)
{
    auto car = w.cars[current];
    auto &p = car.physical;
    auto &a = car.ai;
    if (a.turbo_cooldown != 0)
    {
        --a.turbo_cooldown;
        return;
    }
    if (a.counter2694 != 0)
    {
        p.turbo_active = 1;
        --a.counter2694;
        if (double(car.parameters.rpm_max) * .99f < p.rpm || std::bit_cast<std::int32_t>(p.stunt_meter) > 0)
            a.counter2694 = 0;
        if (a.counter2694 == 0)
        {
            p.turbo_active = 0;
            a.turbo_cooldown = a.turbo_rest;
        }
        return;
    }
    const bool speed = ai_turbo_for_speed(w, current);
    if (!speed && !ai_turbo_for_stunt(w, current))
        return;
    const float factor = f32(double(w.settings.turbo_factor) * p.upgrade_condition + 1);
    a.counter2694 =
        integer((std::abs(w.random.signed_value(speed ? 120.0f : 60.0f)) + (speed ? 180.0 : 120.0)) * factor);
    a.turbo_rest = integer((std::abs(w.random.signed_value(300.0f)) + 300.0) / factor);
}
void choose_ai_pit(AiRivalWorld &w, unsigned current)
{
    const auto car = w.cars[current];
    auto &p = car.physical;
    const auto &r = car.race;
    if (!w.rail.spline || double(p.energy_capacity) * car.parameters.turbo_energy_threshold < p.energy ||
        r.timing26a8 == 1 || std::bit_cast<std::uint32_t>(r.race_index) >= w.clock.lap_count)
        return;
    p.input.turbo = 0;
    const float distance = f32(double(r.current_progress % 100) * w.clock.course_length * 0.01f);
    const float first = f32(double(w.clock.course_length) - 32500);
    const double last = double(w.clock.course_length) - 10000;
    if (double(w.clock.course_length) - 35000 < distance && distance < last)
    {
        w.globals.target_speed = 66.66666412353516f;
        w.globals.braking_allowed = 0;
    }
    if (distance < first || distance > last)
        return;
    double response = (double(distance) - first) * 20 * 0.00009999999747378752f;
    if (response > 20)
        response = 20;
    w.globals.response_scale = f32(response);
    const auto target = spline_point(resolve_spline(w.rail.spline, 0x406d30).value(), 16)
                            .value; // [descriptor+4]+100, no sampling; R18 call-scoped view
    w.globals.target = xyz(target);
    w.globals.target_opaque = std::bit_cast<std::uint32_t>(target.w);
}
void update_rival_ai(AiRivalWorld &w, unsigned current)
{
    if (current >= 6)
        throw FormatError("rival AI current slot");
    auto car = w.cars[current];
    auto &p = car.physical;
    auto &g = w.globals;
    disable_vehicle_control(car.race, p);
    g.response_scale = 1;
    p.upgrade_condition = f32(double(w.drivers[current].skill) * 0.01f);
    car.ai.lateral_fraction = f32(double(w.drivers[current].mode) * 0.01f);
    adjust_ai_skill(w, current);
    adjust_ai_lateral(w, current);
    choose_ai_route_target(car, g);
    choose_ai_route_speed(car, g, w.environment);
    auto field = observations(w);
    AiTactics tactics{g.target,
                      g.target_speed,
                      g.response_scale,
                      w.settings.spline_length,
                      w.clock.course_length,
                      w.environment.world_up};
    ai_follow_rival(field, current, tactics);
    ai_pass_rival(field, current, tactics);
    ai_yield_to_rival(field, current, tactics);
    publish(w, current, field, tactics);
    choose_ai_stunt(car, w.contacts[current], w.stunts[current], g, w.knots, w.random,
                    {w.tick, w.environment.world_up});
    // 407DD0 is a literal RET.
    update_ai_turbo(w, current);
    choose_ai_pit(w, current);
    field = observations(w);
    tactics.target = g.target;
    tactics.target_speed = g.target_speed;
    tactics.response_scale = g.response_scale;
    ai_steer_to_target(field, current, tactics);
    publish(w, current, field, tactics);
    apply_ai_pedals(car, g);
    field = observations(w);
    ai_recover_stalled(field, current);
    publish(w, current, field, tactics);
}
} // namespace sgn::retail
