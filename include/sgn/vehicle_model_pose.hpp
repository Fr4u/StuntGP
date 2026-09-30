#pragma once
#include "sgn/vehicle_model_effects.hpp"
namespace sgn::retail
{
// Borrow all transforms from the selected LOD, preserving authored aliases.
// Source-unwritten stack W lanes retain the current destination word.
void pose_vehicle_body(const VehicleState &, const VehicleTrailInputs &, VehicleModelEffectResource &); // 4138A0
void pose_vehicle_wheels(const VehicleState &, std::span<const VehicleWheelAnimation, 4>,
                         VehicleModelEffectResource &); // 4139F0
void pose_vehicle_effect_anchors(const VehicleState &, const VehicleAnchorState &,
                                 const VehicleMatrix &shared_model_basis,
                                 VehicleModelEffectResource &); // 414130
} // namespace sgn::retail
