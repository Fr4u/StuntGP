#pragma once
#include "sgn/vehicle_reset.hpp"

namespace sgn::retail
{
// The other two previous-input bytes already have one owner in VehicleState:
// previous_shift (+22A4) and previous_stunt (+22A5). Do not duplicate them.
struct VehiclePreviousInputRemainder
{
    std::uint8_t throttle{}, brake{}, left{}, right{};                // +22A0..22A3
    std::uint8_t rear_lock{}, steering_boost{}, turbo{}, auxiliary{}; // +22A6..22A9
};
struct VehicleRestartState
{
    VehiclePreviousInputRemainder previous_input;
    std::uint32_t mode_2620{}, mode_2630{}; // semantic roles not yet established
};
// Full 00413530..0041365C, including the tail call to real 00418D90.
// Target and source global-current identities remain separate.
void restart_vehicle(VehicleResetWorld &, unsigned target, VehicleRestartState &);
} // namespace sgn::retail
