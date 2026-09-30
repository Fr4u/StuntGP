#pragma once
#include <cstdint>
namespace sgn::retail
{
class FrontendState;
struct VehicleVector;
// 443100 on the retained source platform. Null/native571328 stops at its actual read.
// No lookup, source scene construction, or copied VehicleResetParameters is introduced.
void start_source_time_trial_443100(FrontendState &);
// Actual source children shared by initial grid and later trial placement.
void reset_source_vehicle_motion_413490(FrontendState &, std::uint32_t target);
void restart_source_vehicle_413530(FrontendState &, std::uint32_t target);
void place_source_vehicle_408640(FrontendState &, std::uint32_t target, const VehicleVector &);
// NEW R17 entries onto the existing TrialStart bodies: 4187A0 (mode3 gate,418760,
// 4186C0,443700) on the current vehicle and 412940 with its complete reset chain.
void check_source_vehicle_retirement_4187a0(FrontendState &);
void reset_source_vehicle_412940(FrontendState &, std::uint32_t target);
} // namespace sgn::retail
