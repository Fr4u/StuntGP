#include "sgn/vehicle_wake.hpp"

namespace sgn::retail
{
namespace
{
VehicleVector difference(VehicleVector a, VehicleVector b)
{
    return {vehicle_f32(double(a.x) - b.x), vehicle_f32(double(a.y) - b.y), vehicle_f32(double(a.z) - b.z), 0};
}
} // namespace
void apply_vehicle_wake(std::span<VehicleWakeVehicle, 6> vehicles, unsigned current,
                        const VehiclePositionHistory &history)
{
    if (current >= vehicles.size())
        throw std::domain_error("wake current vehicle outside six-slot source domain");
    for (unsigned target = 0; target < vehicles.size(); ++target)
    {
        auto &other = vehicles[target];
        if (target == current || other.race.active != 1 || !(other.physics.speed > 16.66666603088379f))
            continue;
        float total_length{};
        const auto direction =
            vehicle_normalize(difference(history.samples[0].position, history.samples[7].position), &total_length);
        const auto position = other.physics.position, origin = history.samples[0].position;
        const double projected =
            ((double(origin.z) - position.z) * direction.z + (double(origin.y) - position.y) * direction.y) +
            (double(origin.x) - position.x) * direction.x;
        if (!(projected > 0 && projected < total_length))
            continue;
        for (unsigned j = 0; j < 7; ++j)
        {
            const auto &a = history.samples[j];
            const auto &b = history.samples[j + 1];
            float length{};
            const auto axis = vehicle_normalize(difference(a.position, b.position), &length);
            if (!(length > 1))
                continue;
            const float dx = vehicle_f32(double(a.position.x) - position.x),
                        dy = vehicle_f32(double(a.position.y) - position.y);
            const double dz = double(a.position.z) - position.z;
            const float stored_dz = vehicle_f32(dz);
            const double along = (dz * axis.z + double(dy) * axis.y) + double(dx) * axis.x;
            const float stored_along = vehicle_f32(along);
            if (!(along > 0 && stored_along < length))
                continue;
            const double x = double(dx) - double(stored_along) * axis.x;
            const double y = double(dy) - vehicle_f32(double(stored_along) * axis.y);
            const double z = double(stored_dz) - vehicle_f32(double(stored_along) * axis.z);
            const double radius = double(a.radius) - (double(stored_along) / length) * (double(a.radius) - b.radius);
            const float radius_squared = vehicle_f32(radius * radius);
            const double distance_squared = (z * z + y * y) + x * x;
            if (!(distance_squared < radius_squared))
                continue;
            const double ratio = distance_squared / radius_squared, squared = ratio * ratio;
            other.physics.air_factor =
                vehicle_f32(squared < 0.10000000149011612f ? double(0.10000000149011612f) : squared);
            break;
        }
    }
}
void update_vehicle_position_history(std::span<VehicleWakeVehicle, 6> vehicles, unsigned current,
                                     VehiclePositionHistory &history, std::int32_t game_mode)
{
    if (current >= vehicles.size())
        throw std::domain_error("history current vehicle outside six-slot source domain");
    ++history.ticks;
    if (history.ticks >= 8)
    {
        for (unsigned j = 7; j > 0; --j)
        {
            history.samples[j] = history.samples[j - 1];
            history.samples[j].radius = vehicle_f32(double(history.samples[j].radius) * 0.949999988079071f);
        }
        history.samples[0] = {vehicles[current].physics.position, 400};
        history.ticks = 0;
    }
    if (game_mode != 4 && vehicles[current].physics.speed >= 16.66666603088379f)
        apply_vehicle_wake(vehicles, current, history);
}
} // namespace sgn::retail
