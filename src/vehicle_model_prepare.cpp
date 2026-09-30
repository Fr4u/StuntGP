#include "sgn/vehicle_model_prepare.hpp"

namespace sgn::retail
{
void prepare_vehicle_model(const VehicleState &vehicle, const VehicleTrailInputs &inputs,
                           std::span<const VehicleWheelAnimation, 4> wheels,
                           const VehiclePresentationState &presentation, const VehicleAnchorState &anchors,
                           const VehicleSegments &segments, ModelResource &model, unsigned source_lod,
                           VehicleMatrix &basis, contact::Vector4 &effect_position)
{
    VehicleModelEffectResourceView resource(model, source_lod);
    pose_vehicle_body(vehicle, inputs, resource);
    pose_vehicle_wheels(vehicle, wheels, resource);
    prepare_vehicle_model_basis(model.trail_model(), basis);
    std::span<ModelTransform> run;
    if (presentation.field_3134 != 0)
        run = model.source_transform_run(model.source_lod_index(source_lod), 11, 6);
    present_vehicle_segments(vehicle, presentation, anchors, segments, basis, run, effect_position);
    pose_vehicle_effect_anchors(vehicle, anchors, basis, resource);
}
} // namespace sgn::retail
