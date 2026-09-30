#pragma once
#include "sgn/retail_vehicle_anchors.hpp"
#include "sgn/track_contact.hpp"
#include <span>
namespace sgn::retail
{
struct ModelTransform;
// Single owner of six source records315C+i*30hex, each with three vectors.
// Full car clear zeros this value and413670 clones it. No duplicate anchors.
struct VehicleSegment
{
    VehicleVector angles{}, velocity{}, direction{}; //+00/+10/+20
};
struct VehicleSegments
{
    std::array<VehicleSegment, 6> items{};
};
// Literal fixed source step; the direction is computed before integrating angles.
void update_vehicle_segments(const VehicleParameters &, const VehicleState &, const VehiclePresentationState &,
                             const VehicleAnchorState &, VehicleSegments &,
                             VehicleMatrix *audit_rotation = nullptr); // 417F10
// Borrows the contiguous run starting at selected-LOD part11.transform, not
// transforms obtained by looking up parts11..16 separately. Empty is allowed
// only when field3134==0. Source undefined position W is retained per transform.
void present_vehicle_segments(const VehicleState &, const VehiclePresentationState &, const VehicleAnchorState &,
                              const VehicleSegments &, const VehicleMatrix &shared_model_basis,
                              std::span<ModelTransform>, contact::Vector4 &effect_position); // 413E60
} // namespace sgn::retail
