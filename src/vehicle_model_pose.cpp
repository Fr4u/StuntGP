#include "sgn/vehicle_model_pose.hpp"
namespace sgn::retail
{
void pose_vehicle_body(const VehicleState &vehicle, const VehicleTrailInputs &inputs,
                       VehicleModelEffectResource &resource)
{
    const auto lod = resource.selected_lod();
    const auto up = vehicle_normalize({vehicle.body_roll, 1, 0, 0});
    const auto forward = vehicle_normalize({0, vehicle.body_pitch, 1, 0});
    const auto store = [&](unsigned part) {
        auto &t = resource.part(lod, part).transform;
        t.up = {up.x, up.y, up.z};
        t.forward = {forward.x, forward.y, forward.z};
        t.position.y = vehicle.body_heave;
    };
    store(0);
    store(5);
    if (inputs.field_28a0 == 1)
        store(10);
}
void pose_vehicle_wheels(const VehicleState &vehicle, std::span<const VehicleWheelAnimation, 4> wheels,
                         VehicleModelEffectResource &resource)
{
    const auto lod = resource.selected_lod();
    const auto steering = vehicle_rotation(1, vehicle.steering);
    const auto store = [&](unsigned part, const VehicleMatrix &matrix, bool copied_w) {
        auto &t = resource.part(lod, part).transform;
        t.up = {matrix[4], matrix[5], matrix[6]};
        t.forward = {matrix[8], matrix[9], matrix[10]};
        if (copied_w)
        {
            t.up_w = std::bit_cast<std::uint32_t>(matrix[7]);
            t.forward_w = std::bit_cast<std::uint32_t>(matrix[11]);
        }
    };
    store(1, wheels[0].rotation, true);
    store(2, wheels[1].rotation, true);
    VehicleMatrix result{};
    vehicle_multiply_basis(result, wheels[2].rotation, steering);
    store(3, result, false);
    vehicle_multiply_basis(result, wheels[3].rotation, steering);
    store(4, result, false);
}
void pose_vehicle_effect_anchors(const VehicleState &vehicle, const VehicleAnchorState &anchors,
                                 const VehicleMatrix &basis, VehicleModelEffectResource &resource)
{
    if (anchors.effect_available[0] == 0)
        return;
    const auto lod = resource.selected_lod();
    const auto store = [&](unsigned index) {
        auto &target = resource.part(lod, 19 + index).transform;
        const auto &body = resource.part(lod, 0).transform;
        target.up = body.up;
        target.up_w = body.up_w;
        target.forward = body.forward;
        target.forward_w = body.forward_w;
        const auto &point = anchors.effect_position[index];
        const double y = double(point.y) + vehicle.body_heave;
        // The source keeps Y in x87 across all three dot products; no FSTP.
        target.position.x =
            vehicle_f32((double(basis[8]) * point.z + double(basis[0]) * point.x) + double(basis[4]) * y);
        target.position.y =
            vehicle_f32((double(basis[9]) * point.z + double(basis[1]) * point.x) + double(basis[5]) * y);
        target.position.z =
            vehicle_f32((double(basis[10]) * point.z + double(basis[2]) * point.x) + double(basis[6]) * y);
    };
    store(0);
    if (anchors.effect_available[1] != 0)
        store(1);
}
} // namespace sgn::retail
