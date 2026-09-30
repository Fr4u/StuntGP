#include "sgn/contact_ai_stunts.hpp"
#include "sgn/spline_resolution.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
std::int32_t draw_integer(VehicleRandom &random, std::int32_t amplitude)
{
    random.state = random.state * 0x343fdu + 0x269ec3u;
    const auto sample = static_cast<std::int32_t>((random.state >> 16) & 0x7fff) - 0x3fff;
    return (sample * amplitude) / 0x7fff; // used amplitudes8/100 cannot overflow
}
void release_directions(VehicleState &v)
{
    v.input.throttle = 0;
    v.input.brake = 0;
    v.input.left = 0;
    v.input.right = 0;
}
} // namespace
void advance_ai_stunt(const VehicleParameters &parameters, VehicleState &v, AiStuntHelperState &helper,
                      const AiStuntContext &context)
{
    if (std::bit_cast<std::int32_t>(v.stunt_meter) < 0)
    {
        v.stunt_meter = 0;
        return;
    }
    --v.stunt_meter;
    switch (helper.input_kind)
    {
    case 0:
        v.input.throttle = 255;
        break;
    case 1:
        v.input.brake = 255;
        break;
    case 2:
        v.input.left = 255;
        break;
    case 3:
        v.input.right = 255;
        break;
    case 5:
        return;
    default:
        break;
    }
    if (helper.grade <= 2)
    {
        constexpr std::array<std::int32_t, 3> release{8, 15, 20}, stop{10, 18, 26};
        const auto first = v.stunt.phase_indices[0], second = v.stunt.phase_indices[1];
        if (first >= release[helper.grade] || second >= release[helper.grade])
        {
            release_directions(v);
            if (first >= stop[helper.grade] || second >= stop[helper.grade])
            {
                v.stunt.angular_acceleration.x = 0;
                v.stunt.angular_acceleration.y = 0;
                v.stunt.angular_acceleration.z = 0;
                v.stunt.angular_velocity.x = 0;
                v.stunt.angular_velocity.y = 0;
                v.stunt.angular_velocity.z = 0;
            }
        }
    }
    digital_stunt_input(parameters, v, context.tick, context.world_up);
}
void choose_ai_stunt(AiAutomaticVehicle car, contact::solver::State &contact, AiStuntHelperState &helper,
                     const AiAutomaticGlobals &globals, const AiStuntKnots &knots, VehicleRandom &random,
                     const AiStuntContext &context)
{
    auto &v = car.physical;
    if (v.stunt_meter != 0)
    {
        advance_ai_stunt(car.parameters, v, helper, context);
        return;
    }
    if (contact.pair_effect_ticks != 0)
    {
        --contact.pair_effect_ticks;
        return;
    }
    if (v.contact_class != 0)
        return;
    if (!globals.routes)
        throw FormatError("AI stunt requires the initialized shared route cache");
    const auto route = resolve_ai_routes(globals.routes, 0x407010)[2]; // R18 call-scoped route view
    const auto first = sample_spline(route, vehicle_f32(double(globals.route_distances[2]) + 100.0)).value;
    const auto second = sample_spline(route, vehicle_f32(double(globals.route_distances[2]) + 200.0)).value;
    helper.planned_route_distance = 0;
    float length{};
    const auto forward =
        vehicle_normalize({vehicle_f32(double(second.x) - first.x), vehicle_f32(double(second.y) - first.y),
                           vehicle_f32(double(second.z) - first.z), 0},
                          &length);
    if (length < 1)
        return;
    const auto side = vehicle_normalize(
        vehicle_cross(forward, {context.world_up.x, context.world_up.y, context.world_up.z, 0}), &length);
    if (length < double(std::bit_cast<float>(std::uint32_t{0x3f666666})))
        return;
    const auto progress = vehicle_f32(std::fmod(double(car.race.route_fractions[2]), 100.0));
    auto &cursor = car.ai.counter2674;
    if (cursor >= knots.size())
        throw FormatError("AI stunt knot cursor outside source table");
    while (knots[cursor].progress > 0 && knots[cursor].progress < progress)
    {
        ++cursor;
        if (cursor >= knots.size())
            throw FormatError("AI stunt table has no zero terminator");
    }
    const auto &knot = knots[cursor];
    if (knot.progress == 0)
        return;
    if (knot.grade > 2 || !std::isfinite(knot.progress) || !std::isfinite(knot.width) || knot.width < 0)
        throw FormatError("AI stunt knot outside verified authored domain");
    const float distance = vehicle_f32(double(route.descriptor.length) * knot.progress *
                                       double(std::bit_cast<float>(std::uint32_t{0x3c23d70a})));
    const auto target = sample_spline(route, distance).value;
    helper.planned_route_distance = distance;
    // These subtractions remain in x87 registers in the original. Do not narrow
    // them to Vec3 floats before the two projections.
    const double dx = double(target.x) - v.position.x, dy = double(target.y) - v.position.y,
                 dz = double(target.z) - v.position.z;
    if (std::abs((dz * forward.z + dy * forward.y) + dx * forward.x) > 400.0)
        return;
    if (std::abs((dz * side.z + dy * side.y) + dx * side.x) > knot.width)
        return;
    const double threshold =
        std::abs(double(car.ai.lateral_fraction)) * (car.ai.lateral_fraction < 0 ? 60.0 : 20.0) + 10.0;
    if (!std::isfinite(threshold) || threshold >= 2147483648.0)
        throw FormatError("AI stunt probability conversion outside source signed domain");
    if (std::abs(draw_integer(random, 100)) > static_cast<std::int32_t>(threshold))
    {
        helper.input_kind = 5;
        v.stunt_meter = 120;
        return;
    }
    if (v.speed < std::bit_cast<float>(std::uint32_t{0x42855555}))
        return;
    unsigned level = 0;
    if (knot.grade >= 1 && v.speed >= std::bit_cast<float>(std::uint32_t{0x42b75555}))
        level = 1;
    if (knot.grade >= 2 && v.speed >= std::bit_cast<float>(std::uint32_t{0x42d8aaab}))
        level = 2;
    v.stunt.launch_tick = context.tick;
    constexpr std::array<std::uint32_t, 24> choices{0, 1, 0, 1, 0, 1, 2, 3, 0, 1, 2, 3,
                                                    0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};
    helper.input_kind = choices[static_cast<unsigned>(std::abs(draw_integer(random, 8))) + level * 8];
    helper.grade = knot.grade;
    v.stunt_meter = (level * 3 + 3) * 40;
    ++cursor;
}
} // namespace sgn::retail
