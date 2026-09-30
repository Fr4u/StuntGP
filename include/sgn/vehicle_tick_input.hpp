#pragma once
#include "sgn/vehicle_tick.hpp"
namespace sgn::retail
{
struct VehicleTickInputContext
{
    std::uint32_t tick{};
    std::int32_t phase{}; // 60F2B4 notice guard
    Vec3 unit_gravity{};
    contact::player::Controls &controls;
    unsigned current;
    std::array<std::string_view, 6> vehicle_names;
    std::string_view low_energy_text;
};
void analog_steering_input(const VehicleParameters &, VehicleState &, const VehicleRaceState &); // 40C500
void analog_vehicle_input(const VehicleParameters &, VehicleState &, const VehicleRaceState &,
                          const VehicleTickInputContext &);                                             // 40C730
void mixed_vehicle_input(const VehicleParameters &, VehicleState &, const VehicleTickInputContext &);   // 40C710
void digital_vehicle_input(const VehicleParameters &, VehicleState &, const VehicleTickInputContext &); // 40D160
void update_low_energy_notice(VehicleState &, VehicleTickState &, const VehicleTickInputContext &);     // 40D6B0
void dispatch_vehicle_input(const VehicleParameters &, VehicleState &, VehicleRaceState &, VehicleTickState &,
                            const VehicleTickInputContext &); // full40D790
} // namespace sgn::retail
