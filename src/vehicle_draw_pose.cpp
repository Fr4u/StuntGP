#include "sgn/vehicle_draw_pose.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
ModelTransform vehicle_draw_pose(const VehicleState &vehicle, float contact_origin_y,
                                 const ModelTransform &template_pose)
{
    // 4181F3 copies live4864E8 before the position/up/target writers.
    auto pose = template_pose;
    const double offset = 70.0 - double(contact_origin_y);
    const float x = vehicle_f32(offset * vehicle.up.x), y = vehicle_f32(offset * vehicle.up.y);
    pose.position = {vehicle_f32(double(vehicle.position.x) - x), vehicle_f32(double(vehicle.position.y) - y),
                     vehicle_f32(double(vehicle.position.z) - offset * vehicle.up.z)};
    pose.position_w = std::bit_cast<std::uint32_t>(vehicle.position.w);
    pose.up = vehicle.up;
    pose.up_w = std::bit_cast<std::uint32_t>(vehicle.basis_y_w);
    pose.forward = {vehicle_f32(double(vehicle.basis_z.x) + pose.position.x),
                    vehicle_f32(double(vehicle.basis_z.y) + pose.position.y),
                    vehicle_f32(double(vehicle.basis_z.z) + pose.position.z)};
    pose.forward_w = std::bit_cast<std::uint32_t>(vehicle.basis_z.w);
    return pose;
}
std::uint16_t vehicle_draw_ambience(const VehicleModelEffects &effects)
{
    const double value = double(std::bit_cast<float>(effects.route_envelope)) * 512.0; // 46E5AC
    if (!std::isfinite(value) || value >= 9223372036854775808.0 || value < -9223372036854775808.0)
        return 0;
    return static_cast<std::uint16_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(value)));
}
void update_vehicle_draw_flags(const VehicleRaceState &race, VehicleModelStorage &model) noexcept
{
    const auto hold = race.collision_hold;
    model.field_2708 = (hold == 0 || (hold < 60 && (hold & 7u) < 3)) ? 0u : 16u;
}
} // namespace sgn::retail
