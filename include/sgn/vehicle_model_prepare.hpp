#pragma once
#include "sgn/vehicle_model_pose.hpp"
#include "sgn/vehicle_model_basis.hpp"
#include "sgn/vehicle_segments.hpp"

namespace sgn::retail
{
// Full414350 composition. The shared model basis must be rebuilt after body
// and wheel writes, and before either segment or effect-anchor presentation.
void prepare_vehicle_model(const VehicleState &, const VehicleTrailInputs &, std::span<const VehicleWheelAnimation, 4>,
                           const VehiclePresentationState &, const VehicleAnchorState &, const VehicleSegments &,
                           ModelResource &, unsigned source_lod, VehicleMatrix &shared_model_basis,
                           contact::Vector4 &effect_position);
// Full414350 on the canonical bank payload. The model owns selected LOD+5C;
// checked scalar accesses retain authored aliases and actual bank lifetime.
void prepare_vehicle_model(const VehicleState &, const VehicleTrailInputs &, std::span<const VehicleWheelAnimation, 4>,
                           const VehiclePresentationState &, const VehicleAnchorState &, const VehicleSegments &,
                           SourcePmdModelView, VehicleMatrix &shared_model_basis, contact::Vector4 &effect_position);
void prepare_vehicle_model(const VehicleState &, const VehicleTrailInputs &, std::span<const VehicleWheelAnimation, 4>,
                           const VehiclePresentationState &, const VehicleAnchorState &, const VehicleSegments &,
                           ModelResources &, std::size_t handle, VehicleMatrix &shared_model_basis,
                           contact::Vector4 &effect_position);
} // namespace sgn::retail
