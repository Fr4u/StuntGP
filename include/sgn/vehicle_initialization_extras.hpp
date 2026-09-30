#pragma once
#include <cstdint>
#include <span>
namespace sgn::retail
{
// Persistent word omitted from the earlier component inventories. The exact
// 40A2F0 write is known; no rendering meaning is inferred from its -60 bits.
// Full413670 copies it with the vehicle. It is not a world/grid parameter.
struct VehicleInitializationExtras
{
    std::uint32_t field_28b0{};
};
inline void initialize_cold_vehicle_extras(std::span<VehicleInitializationExtras, 6> vehicles) noexcept
{
    for (auto &vehicle : vehicles)
        vehicle = {};
    vehicles[0].field_28b0 = 0xc2700000u; // 0040A4E7
}
} // namespace sgn::retail
