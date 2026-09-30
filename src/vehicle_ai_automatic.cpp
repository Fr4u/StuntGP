#include "sgn/vehicle_ai_automatic.hpp"
#include "sgn/vehicle_ai.hpp"
#include "sgn/retail_level_selection.hpp"
#include "sgn/vehicle_stunt_awards.hpp"
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
// R18: call-scoped resolution of 49A158/15C/160 (native or live source form).
std::array<SplineView, 3> routes(const AiAutomaticGlobals &globals, std::uint32_t site)
{
    if (!globals.routes)
        throw FormatError("automatic AI requires initialized spline routes");
    return resolve_ai_routes(globals.routes, site);
}
void steer(AiAutomaticVehicle car, AiAutomaticGlobals &globals, const AiAutomaticEnvironment &environment)
{
    // Temporary observations for the already qualified steering kernel. Only
    // its two written results return to the authoritative physical/global state.
    AiField observations{};
    auto &o = observations[0];
    const auto &p = car.physical;
    o.position = xyz(p.position);
    o.right = xyz(p.basis_x);
    o.up_y = p.up.y;
    o.gear = p.gear;
    o.steering = p.steering;
    o.max_steering = car.parameters.steering_max;
    o.steering_divisor_signal = p.average_grip;
    o.driving_factor = p.upgrade_condition;
    o.steering_ramp_ticks = car.ai.steering_ramp_ticks;
    AiTactics tactics{};
    tactics.target = globals.target;
    tactics.response_scale = globals.response_scale;
    tactics.world_up = environment.world_up;
    ai_steer_to_target(observations, 0, tactics);
    car.physical.steering = o.steering;
    globals.response_scale = tactics.response_scale;
}
} // namespace
void initialize_ai_automatic(const TrackSplineSource &set, std::span<AiAutomaticVehicle, 6> cars, AiAutomaticGlobals &g)
{
    if (g.initialized == 1)
        return;
    // 40529B..405306: the route lookups publish into 49A158/15C/160. The owner
    // publishes before the flag, so a decoded missing route still throws
    // before any write, as the former selected-then-store order did.
    set.publish_ai_routes(g.routes);
    g.initialized = 1;
    g.route_distances.fill(0);
    // 004064B0 is an actual RET in the pinned executable, not an omitted helper.
    for (auto car : cars)
    {
        car.race.route_indices.fill(0);
        car.race.route_fractions.fill(0);
        car.ai.reverse_ticks = 0;
        car.ai.counter2674 = 0;
        car.physical.stunt_meter = 0;
        car.ai.passing_ticks = 0;
        car.ai.counter2694 = 0;
        car.ai.following_ticks = 0;
    }
}
Vec3 sample_ai_route(const AiAutomaticGlobals &g, float distance, float lateral)
{
    const auto splines = routes(g, 0x406870);
    std::array<Vec3, 3> points;
    for (unsigned i = 0; i < 3; ++i)
        points[i] = xyz(sample_spline(splines[i], f32(double(g.route_distances[i]) + distance)).value);
    const auto &middle = points[1];
    const auto &edge = points[lateral < 0 ? 2 : 0];
    const double factor = lateral < 0 ? -double(lateral) : double(lateral);
    const double x = (double(edge.x) - middle.x) * factor;
    const float y = f32(double(f32(double(edge.y) - middle.y)) * factor);
    const float z = f32(double(f32(double(edge.z) - middle.z)) * factor);
    return {f32(x + middle.x), f32(double(y) + middle.y), f32(double(z) + middle.z)};
}
void choose_ai_route_target(AiAutomaticVehicle car, AiAutomaticGlobals &g)
{
    const auto splines = routes(g, 0x406b10);
    auto &p = car.physical;
    auto &r = car.race;
    // 406B35,406B59,406B7D: original order is middle, left, right.
    for (unsigned i : {1u, 0u, 2u})
    {
        const auto projection = project_spline(splines[i], xyz(p.position), r.route_indices[i]);
        r.route_indices[i] = projection.segment;
        r.route_fractions[i] = projection.percent;
    }
    for (unsigned i = 0; i < 3; ++i)
        g.route_distances[i] = f32(double(r.route_fractions[i]) * splines[i].descriptor.length * double(0.01f));
    double scale = double(p.speed) * double(0.013333333656191826f);
    if (scale > 1)
        scale = 1;
    else if (scale < 0.25f)
        scale = 0.25f;
    Vec3 target = sample_ai_route(g, f32(scale * 2000), car.ai.lateral_fraction);
    if (p.contact_class == 0)
    {
        const double dx = double(target.x) - p.position.x, dy = double(target.y) - p.position.y,
                     dz = double(target.z) - p.position.z;
        const double forward = (dz * p.basis_z.z + dy * p.basis_z.y) + dx * p.basis_z.x;
        if (forward < double(0.1f))
        {
            g.response_scale = 10;
            const double side = (dy * p.basis_x.y + dz * p.basis_x.z) + dx * p.basis_x.x;
            const double offset = side < 0 ? -10000.0 : 10000.0;
            const float x = f32(offset * p.basis_x.x), y = f32(offset * p.basis_x.y);
            target = {f32(double(x) + p.position.x), f32(double(y) + p.position.y),
                      f32(offset * p.basis_x.z + p.position.z)};
        }
    }
    g.target = target; // 49A154 is copied from an uninitialized local, not used here.
}
void choose_ai_route_speed(AiAutomaticVehicle car, AiAutomaticGlobals &g, const AiAutomaticEnvironment &e)
{
    const auto &p = car.physical;
    const double extended = double(p.speed) * double(0.01f);
    float scale = f32(extended);
    if (extended > 1)
        scale = 1;
    else if (scale < 0.5f)
        scale = 0.5f;
    const Vec3 origin = sample_ai_route(g, 0, car.ai.lateral_fraction);
    const Vec3 near = sample_ai_route(g, f32(double(scale) * 2500), car.ai.lateral_fraction);
    const Vec3 far = sample_ai_route(g, f32(double(scale) * 5000), car.ai.lateral_fraction);
    const double dx = double(near.x) - origin.x, dy = double(near.y) - origin.y, dz = double(near.z) - origin.z;
    // This entry crosses its unrounded delta with world-up before each FST.
    const VehicleVector side{f32(double(e.world_up.z) * dy - double(e.world_up.y) * dz),
                             f32(double(e.world_up.x) * dz - double(e.world_up.z) * dx),
                             f32(double(e.world_up.y) * dx - double(e.world_up.x) * dy), 0};
    const auto unit = vehicle_normalize(side);
    const double bend =
        (((double(far.z) - near.z) * unit.z + (double(far.y) - near.y) * unit.y) + (double(far.x) - near.x) * unit.x);
    car.ai.bend = f32(bend);
    double speed =
        108.33333587646484f - std::abs(bend * double(0.00039999998989515007f)) * 100 + double(16.66666603088379f);
    std::uint32_t braking = 0;
    if (p.energy_exempt == 1)
    {
        speed = e.energy_exempt_speed;
        braking = 1;
    }
    else if (car.race.automatic_drive == 1)
    {
        speed = e.automatic_speed;
        braking = 1;
    }
    if (p.mass_override == 1)
        speed = 166.6666717529297f;
    g.target_speed = f32(speed);
    g.braking_allowed = braking;
}
void apply_ai_pedals(AiAutomaticVehicle car, AiAutomaticGlobals &g)
{
    auto &p = car.physical;
    double delta = double(g.target_speed) - p.speed;
    if (p.average_grip >= 10)
        delta -= double(p.upgrade_condition) * p.average_grip * 5;
    if (delta > 0)
    {
        double throttle = delta * double(0.1f);
        if (throttle > 1)
            throttle = 1;
        p.throttle = f32(throttle);
        p.control_flags[2] = 1;
        return;
    }
    if (g.braking_allowed != 1)
        return;
    p.control_flags[2] = 0;
    if (delta > -5)
    {
        p.throttle = f32(double(p.throttle) * double(0.8f));
        return;
    }
    double brake = delta * -32;
    if (brake > 255)
        brake = 255;
    if (!std::isfinite(brake) || brake < 0 || brake > 255)
        throw FormatError("automatic AI brake outside recovered finite domain");
    p.input.brake = static_cast<std::uint8_t>(brake); // original __ftol, then AL
    digital_brake(car.parameters, p);
    p.throttle = 0;
}
void update_player_start_throttle(std::span<AiAutomaticVehicle, 6> cars, unsigned current,
                                  const AiPlayerPrelude &context)
{
    if (current >= 6 || context.controls.current >= 4)
        throw FormatError("player prestart current identity");
    const auto selected = context.controls.controllers[context.controls.current].vehicle;
    if (selected >= 6)
        throw FormatError("player prestart controller vehicle identity");
    if (cars[selected].race.start_countdown == 0)
        return; // 407F6C..407F74
    auto car = cars[current];
    auto &ticks = car.ai.start_throttle_ticks;
    if (car.race.active == 1 && context.cars[current].settings.ai_skill != 1 && ticks == 0 &&
        vehicle_random_integer(context.random, 64) == 0)
        ticks = 3;
    if (ticks != 0)
    {
        --ticks;
        car.physical.throttle = 1;
    } // 407FD8..407FE9, also inactive slots
}
void ai_automatic_tick(const TrackSplineSource &set, std::span<AiAutomaticVehicle, 6> cars, std::size_t self,
                       AiAutomaticGlobals &g, const AiAutomaticEnvironment &e, const AiPlayerPrelude *player,
                       AiRivalStep *rival)
{
    if (self >= cars.size())
        throw FormatError("automatic AI current slot outside six vehicles");
    initialize_ai_automatic(set, cars, g);
    auto car = cars[self];
    auto &p = car.physical;
    auto &r = car.race;
    if (r.active == 0)
        return;
    if (p.speed > 5 && car.ai.steering_ramp_ticks != 0)
        car.ai.steering_ramp_ticks =
            std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(car.ai.steering_ramp_ticks) - 1u);
    if (p.special_mode == 1 || r.stopped == 1)
        return;
    if (p.energy_exempt != 1 && r.automatic_drive != 1)
    {
        if (!player)
            throw FormatError("ordinary player AI requires the original prestart inputs");
        update_player_start_throttle(cars, static_cast<unsigned>(self), *player); // 408074
        if (p.upgrade_active == 0)
            return; // 408079..408087, ordinary player return
        if (!rival)
            throw FormatError("ordinary rival AI requires its source world bindings");
        rival->tick(static_cast<unsigned>(self));
        return;
    }
    if (e.game_type == 3)
        return;
    if (p.gear == 0)
        p.gear = 2;
    if (p.energy_exempt == 1)
        r.collision_hold = 0x34bc0;
    p.turbo_active = 0;
    p.upgrade_condition = 1;
    car.ai.lateral_fraction = 0;
    g.response_scale = 1;
    disable_vehicle_control(r, p);
    choose_ai_route_target(car, g);
    choose_ai_route_speed(car, g, e);
    steer(car, g, e);
    apply_ai_pedals(car, g);
}
} // namespace sgn::retail
