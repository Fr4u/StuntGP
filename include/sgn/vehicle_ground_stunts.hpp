#pragma once
#include "sgn/vehicle_score.hpp"

namespace sgn::retail
{
// Borrowed configuration snapshots in original call order. These are not new
// per-car state: counters and contact data retain their existing owners.
struct VehicleGroundStuntAward
{
    std::uint32_t points{};
    std::string_view text;
};
struct VehicleGroundStuntRewards
{
    // Points:60D718,714,710,70C,6F0,6DC,6E0,6E8,6E4,6F4.
    // Text:612860,85C,868,864,880,8CC,8C8,900,904,8D0.
    std::array<VehicleGroundStuntAward, 10> values{};
};
struct VehicleGroundStuntContext
{
    contact::player::Controls &controls;
    std::uint32_t current{};
    VehicleScoreState &score;
    const VehicleStuntContext &stunt;
    const VehicleGroundStuntRewards &rewards;
};
void award_vehicle_balance(VehicleState &, const VehicleGroundStuntContext &, unsigned kind); // 410CD0/D10/D50/D90
void update_vehicle_balance(VehicleState &, const VehicleGroundStuntContext &);               // 410DF0
void update_vehicle_flip(VehicleState &, const VehicleGroundStuntContext &);                  // 410F80
void update_vehicle_drift(VehicleState &, const VehicleGroundStuntContext &);                 // 411150
void update_vehicle_slipstream(VehicleState &, const VehicleGroundStuntContext &);            // 411290
void update_vehicle_roof_slide(VehicleState &, std::int32_t shock_hold, const VehicleGroundStuntContext &); // 411420
} // namespace sgn::retail
