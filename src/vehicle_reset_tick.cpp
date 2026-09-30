#include "sgn/vehicle_reset_tick.hpp"
#include <bit>
#include <cmath>
#include <stdexcept>

namespace sgn::retail
{
void clear_accumulated_impact_loss(contact::solver::State &s) noexcept
{
    // Unordered compares set the same equality bit tested by this entry.
    if (s.accumulated_energy_loss == 0 || std::isnan(s.accumulated_energy_loss))
        return;
    s.energy_hold = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(s.energy_hold) - 1u);
    s.accumulated_energy_loss = 0;
}
void check_stuck_vehicle(VehicleState &s, VehicleRaceState &race) noexcept
{
    if (!(std::abs(double(s.speed)) <= 1.6666666666666667) || s.contact_class != 2)
        return;
    bool spinning = false;
    for (const auto &wheel : s.wheels)
        if (std::abs(double(wheel.speed)) > 16.666666666666668)
        {
            spinning = true;
            break;
        }
    if (!spinning)
        return;
    ++race.pending_reset;
    if (race.pending_reset >= 240)
        race.respawn_requested = 1;
}
bool finish_vehicle_bounds(VehicleResetWorld &world, const VehicleResetBounds &bounds)
{
    if (world.current >= 6)
        throw std::out_of_range("bounds current vehicle");
    auto &car = world.vehicles[world.current];
    auto &s = car.physics;
    auto &race = car.race;
    const auto &p = world.parameters;
    if (p.mode != 3)
    {
        const auto count = p.spline.descriptor.point_count;
        if (count == 0)
            throw std::domain_error("bounds spline count is zero");
        const auto across = vehicle_normalize(vehicle_cross(race.respawn_heading, p.unit_gravity));
        const auto index = ((race.spline_index * 255u) / count) & 255u;
        const double dx = double(s.position.x) - race.respawn_position.x;
        const double dy = double(s.position.y) - race.respawn_position.y;
        const double dz = double(s.position.z) - race.respawn_position.z;
        const double lateral = (dz * across.z + dy * across.y) + dx * across.x;
        if (s.special_mode != 1)
        {
            if (bounds.right[index] < 0.99f && lateral > double(bounds.right[index]) * 10000.0f)
                race.respawn_requested = 1;
            if (bounds.left[index] < 0.99f && lateral < double(bounds.left[index]) * -10000.0f)
                race.respawn_requested = 1;
        }
        if (s.contact_class != 0)
        {
            const auto g = p.unit_gravity;
            const double depth = (double(g.z) * dz + double(g.y) * dy) + double(g.x) * dx;
            if (depth > 3000.0f)
                race.respawn_requested = 1;
        }
    }
    bool placed = false;
    if (race.respawn_requested == 1)
        placed = reset_vehicle(world, world.current);
    if (race.collision_hold != 0)
        --race.collision_hold;
    return placed;
}
} // namespace sgn::retail
