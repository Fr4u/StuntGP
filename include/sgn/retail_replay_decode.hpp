#pragma once
#include "sgn/vehicle.hpp"
#include "sgn/vehicle_visual_state.hpp"
#include <span>

namespace sgn::retail
{
// Actual packed retail frames; current/next replace 4d5cb8/570ac4.
// phase is the selected recording's +8 field, not display frame time.
struct ReplayFramePair
{
    std::span<const std::uint8_t, 44> current, next;
    float phase;
};
// Each function implements one entire source entry, independently callable.
// The source copies five uninitialized stack W lanes. Native policy preserves
// the corresponding existing W instead; all defined XYZ and W copies remain.
void decode_replay_position(const ReplayFramePair &, VehicleState &); // 00419ee0
void decode_replay_basis(const ReplayFramePair &, VehicleState &);    // 0041a000
void decode_replay_wheels(const ReplayFramePair &, VehicleState &, const VehicleParameters &,
                          std::span<VehicleWheelAnimation, 4>); // 0041a230
} // namespace sgn::retail
