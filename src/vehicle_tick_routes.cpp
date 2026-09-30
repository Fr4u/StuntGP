#include "sgn/vehicle_tick_routes.hpp"
#include "sgn/spline_resolution.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
struct PreciseVector
{
    double x, y, z;
};
// R18: call-scoped resolution of 49A158/15C/160 (native or live source form).
std::array<SplineView, 3> routes(const AiAutomaticGlobals &globals, std::uint32_t site)
{
    if (!globals.routes)
        throw std::runtime_error("vehicle route cache is not initialized");
    return resolve_ai_routes(globals.routes, site);
}
Vec3 xyz(VehicleVector v)
{
    return {v.x, v.y, v.z};
}
double lateral_signal(const VehicleState &s, Vec3 target, Vec3 gravity, bool upgraded)
{
    // 4054FB..4055C0 /40602C..4060EB retain X while Y/Z are first stored.
    const double x = double(s.position.x) - target.x;
    const float y = vehicle_f32(double(s.position.y) - target.y), z = vehicle_f32(double(s.position.z) - target.z);
    const double vertical = (double(gravity.z) * z + double(gravity.y) * y) + double(gravity.x) * x;
    const float gx = vehicle_f32(double(gravity.x) * vertical), gy = vehicle_f32(double(gravity.y) * vertical),
                gz = vehicle_f32(double(gravity.z) * vertical);
    double signal =
        ((((double(gz) + z) * s.basis_x.z + (double(gy) + y) * s.basis_x.y) + (x + gx) * s.basis_x.x) * 0.0005f) *
        2.25f;
    double divisor = upgraded ? (double(s.average_grip) * s.upgrade_condition) * 0.35f : double(s.average_grip) * 0.35f;
    if (divisor < 1)
        divisor = 1;
    signal /= divisor;
    if (s.up.y < 0)
        signal = -signal;
    return signal;
}
void steer_amount(const VehicleParameters &p, VehicleState &s, double amount, bool retained_sign)
{
    const float stored = vehicle_f32(amount);
    if ((retained_sign ? amount : double(stored)) < 0)
        vehicle_steer_decrease(s.steering, p.steering_max, -stored);
    else
        vehicle_steer_increase(s.steering, p.steering_max, stored);
}
void project_routes(VehicleState &s, VehicleRaceState &race, AiAutomaticGlobals &globals, std::uint32_t site)
{
    const auto views = routes(globals, site);
    for (unsigned j : {1u, 0u})
    {
        const auto result = project_spline(views[j], xyz(s.position), race.route_indices[j]);
        race.route_indices[j] = result.segment;
        race.route_fractions[j] = result.percent;
    }
    for (unsigned j = 0; j < 2; ++j)
        globals.route_distances[j] =
            vehicle_f32((double(race.route_fractions[j]) * views[j].descriptor.length) * 0.01f);
}
PreciseVector bend_mix(Vec3 a, Vec3 b, float blend, bool retain_x)
{
    const double x = (double(b.x) - a.x) * blend + a.x;
    const float y = vehicle_f32(double(vehicle_f32(double(vehicle_f32(double(b.y) - a.y)) * blend)) + a.y);
    const float z = vehicle_f32(double(vehicle_f32(double(vehicle_f32(double(b.z) - a.z)) * blend)) + a.z);
    return {retain_x ? x : double(vehicle_f32(x)), y, z};
}
} // namespace
void limit_vehicle_route_speed(const VehicleParameters &p, VehicleState &s, float speed)
{
    // 405BE0..405C70. Compare the unrounded difference before loading its store.
    const double difference = double(speed) - s.speed;
    const float stored = vehicle_f32(difference);
    if (difference > 0)
        return;
    s.control_flags[2] = 0;
    if (stored > -5.0f)
    {
        s.throttle = vehicle_f32(double(s.throttle) * 0.8f);
        return;
    }
    double braking = double(stored) * -32.0f;
    s.throttle = 0;
    s.input.throttle = 0;
    if (braking > 255)
        braking = 255;
    s.input.brake = static_cast<std::uint8_t>(static_cast<int>(braking));
    digital_brake(p, s);
}
void steer_vehicle_route_target(const VehicleParameters &p, VehicleState &s, const AiAutomaticMemory &ai, Vec3 target,
                                float gain, const VehicleRouteAssistSettings &settings)
{
    if (settings.phase == 1)
        return;
    double scale = gain;
    if (ai.steering_ramp_ticks != 0)
    {
        const auto remaining = 300u - std::bit_cast<std::uint32_t>(ai.steering_ramp_ticks);
        const double ramp = double(remaining) * 0.0033333334140479565f;
        scale = (ramp * ramp) * gain;
    }
    steer_amount(p, s, lateral_signal(s, target, settings.unit_gravity, true) * scale, false);
}
void steer_vehicle_override_target(const VehicleParameters &p, VehicleState &s, Vec3 target, float gain, Vec3 gravity)
{
    steer_amount(p, s, lateral_signal(s, target, gravity, false) * gain, true);
}
void guide_vehicle_route(const VehicleParameters &p, VehicleState &s, const VehicleRouteAssistState &state,
                         const VehicleRouteAssistContext &c, float gain)
{
    const auto views = routes(c.routes, 0x405610);
    double scale = double(s.speed) * 0.013333333656191826f;
    if (scale > 1)
        scale = 1;
    else if (scale < 0.25f)
        scale = 0.25f;
    const float offset = vehicle_f32(scale * 2000.0f);
    const auto a = sample_spline(views[0], vehicle_f32(double(offset) + c.routes.route_distances[0])).value;
    const auto b = sample_spline(views[1], vehicle_f32(double(offset) + c.routes.route_distances[1])).value;
    const float dx = vehicle_f32(double(b.x) - a.x), dy = vehicle_f32(double(b.y) - a.y),
                dz = vehicle_f32(double(b.z) - a.z);
    Vec3 target{vehicle_f32(double(vehicle_f32(double(state.blend) * dx)) + a.x),
                vehicle_f32(double(vehicle_f32(double(state.blend) * dy)) + a.y),
                vehicle_f32(double(state.blend) * dz + a.z)};
    if (s.contact_class != 0)
        return;
    const PreciseVector difference{double(target.x) - s.position.x, double(target.y) - s.position.y,
                                   double(target.z) - s.position.z};
    const double forward = (difference.z * s.basis_z.z + difference.y * s.basis_z.y) + difference.x * s.basis_z.x;
    if (forward < 0.1f)
    {
        const double sideways = (difference.x * s.basis_x.x + difference.y * s.basis_x.y) + difference.z * s.basis_x.z;
        const float side = sideways < 0 ? -5000.0f : 5000.0f;
        target = {vehicle_f32(double(vehicle_f32(double(side) * s.basis_x.x)) + s.position.x),
                  vehicle_f32(double(vehicle_f32(double(side) * s.basis_x.y)) + s.position.y),
                  vehicle_f32(double(side) * s.basis_x.z + s.position.z)};
    }
    if (s.contact_class == 0)
        steer_vehicle_route_target(p, s, c.ai, target, gain, c.settings);
}
void guide_vehicle_override(const VehicleParameters &p, VehicleState &s, const VehicleRouteAssistContext &c, float gain)
{
    const auto point =
        sample_spline(routes(c.routes, 0x406140)[0], vehicle_f32(double(c.routes.route_distances[0]) + 2000.0f));
    if (s.contact_class == 0)
        steer_vehicle_override_target(p, s, xyz(point.value), gain, c.settings.unit_gravity);
}
void update_vehicle_route_braking(const VehicleParameters &p, VehicleState &s, VehicleRaceState &race,
                                  VehicleRouteAssistState &state, const VehicleRouteAssistContext &c)
{
    if (s.upgrade_active == 1 && !(c.driver_skill < c.settings.upgraded_skill_threshold))
        return;
    const auto selector = c.selectors.field_771c;
    if (selector == 0 || s.turbo_active == 1 || s.mass_override == 1 || s.special_mode == 1)
        return;
    if (selector >= c.settings.bend_speed.size())
        throw std::runtime_error("route bend selector outside source table");
    state.blend = 1;
    project_routes(s, race, c.routes, 0x405c80);
    const auto views = routes(c.routes, 0x405c80);
    const double raw_scale = double(s.speed) * 0.01f;
    float scale = vehicle_f32(raw_scale);
    if (raw_scale > 1)
        scale = 1;
    else if (scale < 0.5f)
        scale = 0.5f;
    const float first = vehicle_f32(double(scale) * 2500.0f), second = vehicle_f32(double(scale) * 5000.0f);
    const auto sample = [&](unsigned route, float offset) {
        return xyz(sample_spline(views[route], vehicle_f32(double(offset) + c.routes.route_distances[route])).value);
    };
    // Source samples route1 three times, then route0 three times. Sample calls
    // are read-only, but retain their exact source order for adjacent storage.
    const auto b0 = sample(1, 0), b1 = sample(1, first), b2 = sample(1, second);
    const auto a0 = sample(0, 0), a1 = sample(0, first), a2 = sample(0, second);
    const auto start = bend_mix(a0, b0, state.blend, true), middle = bend_mix(a1, b1, state.blend, false),
               end = bend_mix(a2, b2, state.blend, false);
    const PreciseVector direction{middle.x - start.x, middle.y - start.y, middle.z - start.z};
    const auto g = c.settings.unit_gravity;
    const auto side = vehicle_normalize({vehicle_f32(direction.y * g.z - direction.z * g.y),
                                         vehicle_f32(direction.z * g.x - direction.x * g.z),
                                         vehicle_f32(direction.x * g.y - direction.y * g.x), 0});
    const double dot = ((end.z - middle.z) * side.z + (end.y - middle.y) * side.y) + (end.x - middle.x) * side.x;
    const double bend = std::abs(dot * 0.0004f);
    if (bend >= 0.2f)
        limit_vehicle_route_speed(
            p, s, vehicle_f32((double(c.settings.bend_speed[selector]) - bend * 100.0f) + 16.66666603088379f));
}
void update_vehicle_route_guidance(const VehicleParameters &p, VehicleState &s, VehicleRaceState &race,
                                   const VehicleRouteAssistState &state, const VehicleRouteAssistContext &c)
{
    if (s.upgrade_active == 1)
        return;
    project_routes(s, race, c.routes, 0x406190);
    if (s.mass_override == 1)
    {
        guide_vehicle_override(p, s, c, c.settings.mass_override_gain);
        return;
    }
    const auto selector = c.selectors.field_7720;
    if (selector == 0 || s.turbo_active == 1 || s.special_mode == 1)
        return;
    if (selector >= c.settings.steering_gain.size())
        throw std::runtime_error("route steering selector outside source table");
    guide_vehicle_route(p, s, state, c, c.settings.steering_gain[selector]);
}
} // namespace sgn::retail
