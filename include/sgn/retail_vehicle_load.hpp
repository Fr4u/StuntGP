#pragma once
#include "sgn/vehicle_wake.hpp"

namespace sgn::retail
{
struct VehicleSoundIds
{
    std::int32_t horn{}, engine_rev{}, engine_idle{}, engine_boost{}; // 7708/C/10/14
};
struct VehicleLoadState
{
    float current_refuel_rate{}; // 2274, separate from authored parameter 1184
    VehicleSoundIds sounds;
};
struct VehicleLoadVehicle
{
    VehicleParameters &parameters;
    VehicleState &physics;
    VehicleRaceState &race;
    VehiclePositionHistory &history; // owner: vehicle_wake.hpp
    VehicleLoadState &load;
};
struct VehicleLoadWorld
{
    std::span<VehicleLoadVehicle, 6> vehicles;
    unsigned &current; // one shared current slot, source 4D5BB8
};
// Complete SFX section with mandatory keys. Missing/malformed fields throw.
VehicleSoundIds decode_vehicle_sound_ids(std::string_view);                         // 40BC30 values
void reset_loaded_position_history(const VehicleState &, VehiclePositionHistory &); // 40A0D0
void reset_loaded_vehicle_histories(std::span<VehicleLoadVehicle, 6>);              // 40A120, all six
// Full 40BDE0 gameplay writes/order. Host AssetStore and RAII replace the old
// filesystem allocator; no original pointer or parallel vehicle blob is held.
// target is source 474630; config_id is signed 472C74; control_kind is 60F018.
// Only Car%d.cfg is addressed by this entry (formula17.cfg is not a Car ID).
void load_vehicle_configuration(VehicleLoadWorld &, const AssetStore &, unsigned target, std::int32_t config_id,
                                std::int32_t control_kind);
} // namespace sgn::retail
