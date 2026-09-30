#include "sgn/vehicle_model_prepare.hpp"

namespace sgn::retail
{
namespace
{
// Borrow only checked metadata; no payload pointer/reference or model copy.
class TransformFields
{
    SourcePmdByteView bytes_;

  public:
    TransformFields(SourcePmdModelView model, SourcePmdField<SourcePmdTransform> transform)
        : bytes_(model.bytes().subview(transform.location().offset, 80))
    {
    }
    SourcePmdField<float> scalar(unsigned offset) const
    {
        return bytes_.field<float>(offset);
    }
    SourcePmdField<VehicleVector> vector(unsigned offset) const
    {
        return bytes_.field<VehicleVector>(offset);
    }
    void xyz(unsigned offset, VehicleVector value) const
    {
        scalar(offset).set(value.x);
        scalar(offset + 4).set(value.y);
        scalar(offset + 8).set(value.z);
    }
};
TransformFields part_transform(SourcePmdModelView model, unsigned lod, unsigned part)
{
    return {model, model.part(lod, part).transform()};
}
unsigned selected_lod(SourcePmdModelView model)
{
    return model.source_lod_index(model.current_detail_level().value());
}

void body_4138a0(const VehicleState &vehicle, const VehicleTrailInputs &inputs, SourcePmdModelView model)
{
    const auto lod = selected_lod(model);
    const auto up = vehicle_normalize({vehicle.body_roll, 1, 0, 0});
    const auto forward = vehicle_normalize({0, vehicle.body_pitch, 1, 0});
    const auto store = [&](unsigned part) {
        const auto t = part_transform(model, lod, part);
        t.xyz(0x30, up);
        t.xyz(0x20, forward);
        t.scalar(4).set(vehicle.body_heave);
    };
    // Source stack W is undefined at41391A/35,413965/80,4139B5/D0.
    // The explicit host policy retains those destination lanes.
    store(0);
    store(5);
    if (inputs.field_28a0 == 1)
        store(10);
}
void wheels_4139f0(const VehicleState &vehicle, std::span<const VehicleWheelAnimation, 4> wheels,
                   SourcePmdModelView model)
{
    const auto lod = selected_lod(model);
    const auto steering = vehicle_rotation(1, vehicle.steering);
    const auto store = [&](unsigned part, const VehicleMatrix &matrix, bool copy_w) {
        const auto t = part_transform(model, lod, part);
        const VehicleVector up{matrix[4], matrix[5], matrix[6], matrix[7]},
            forward{matrix[8], matrix[9], matrix[10], matrix[11]};
        if (copy_w)
        {
            t.vector(0x30).set(up);
            t.vector(0x20).set(forward);
        }
        else
        {
            t.xyz(0x30, up);
            t.xyz(0x20, forward);
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
void basis_413b60(SourcePmdModelView model, VehicleMatrix &basis)
{
    // 413B72 always reads descriptor+48 (raw LOD0), after prior mutations.
    const auto body = part_transform(model, model.source_lod_index(0), 0);
    VehicleTrailModel inputs;
    inputs.base_forward = body.vector(0x20).value();
    inputs.base_up = body.vector(0x30).value();
    prepare_vehicle_model_basis(inputs, basis);
}
double transform_component(VehicleVector v, const VehicleMatrix &m, unsigned component)
{
    return (double(v.z) * m[8 + component] + double(v.y) * m[4 + component]) + double(v.x) * m[component];
}
void segments_413e60(const VehicleState &vehicle, const VehiclePresentationState &presentation,
                     const VehicleAnchorState &anchor, const VehicleSegments &segments, const VehicleMatrix &basis,
                     SourcePmdModelView model, contact::Vector4 &effect)
{
    if (presentation.field_3134 == 0)
        return;
    const auto run = model.part(selected_lod(model), 11).transform_run(6);
    VehicleVector current{vehicle_f32(transform_component(anchor.position, basis, 0)),
                          vehicle_f32(transform_component(anchor.position, basis, 1)),
                          vehicle_f32(transform_component(anchor.position, basis, 2)), 0};
    current.y = vehicle_f32(double(current.y) + vehicle.body_heave);
    for (unsigned i = 0; i < 6; ++i)
    {
        const auto source = segments.items[i].direction;
        const VehicleVector direction{vehicle_f32(transform_component(source, basis, 0)),
                                      vehicle_f32(transform_component(source, basis, 1)),
                                      vehicle_f32(transform_component(source, basis, 2)), source.w};
        const TransformFields t(model, run.at(i));
        t.vector(0x20).set(direction);
        t.xyz(0, current);
        // 413FDA..414043 keeps X product extended; Y/Z products store first.
        const double x = double(anchor.extent) * direction.x;
        const float y = vehicle_f32(double(anchor.extent) * direction.y),
                    z = vehicle_f32(double(anchor.extent) * direction.z);
        current.x = vehicle_f32(double(current.x) + x);
        current.y = vehicle_f32(double(current.y) + y);
        current.z = vehicle_f32(double(current.z) + z);
    }
    const double x = -double(current.x) * vehicle.basis_x.x;
    const float y = vehicle_f32(-double(current.x) * vehicle.basis_x.y),
                z = vehicle_f32(-double(current.x) * vehicle.basis_x.z);
    const double vertical = double(std::bit_cast<float>(0x428c0000u)) - current.y; // 46E53C
    const float ux = vehicle_f32(vertical * vehicle.up.x), uy = vehicle_f32(vertical * vehicle.up.y),
                uz = vehicle_f32(vertical * vehicle.up.z);
    const float fx = vehicle_f32(double(current.z) * vehicle.basis_z.x),
                fy = vehicle_f32(double(current.z) * vehicle.basis_z.y),
                fz = vehicle_f32(double(current.z) * vehicle.basis_z.z);
    const double ox = (x + ux) + fx;
    const float oy = vehicle_f32(double(fy) + vehicle_f32(double(y) + uy)),
                oz = vehicle_f32(double(fz) + vehicle_f32(double(z) + uz));
    effect.x = vehicle_f32(double(vehicle.position.x) - ox);
    effect.y = vehicle_f32(double(vehicle.position.y) - oy);
    effect.z = vehicle_f32(double(vehicle.position.z) - oz);
}
void effects_414130(const VehicleState &vehicle, const VehicleAnchorState &anchors, const VehicleMatrix &basis,
                    SourcePmdModelView model)
{
    if (anchors.effect_available[0] == 0)
        return;
    const auto lod = selected_lod(model);
    const auto store = [&](unsigned index) {
        const auto target = part_transform(model, lod, 19 + index), body = part_transform(model, lod, 0);
        // Full vector copies include W; re-read body for the second effect so
        // valid shared transform identities see all preceding writes.
        target.vector(0x30).set(body.vector(0x30).value());
        target.vector(0x20).set(body.vector(0x20).value());
        const auto &point = anchors.effect_position[index];
        const double y = double(point.y) + vehicle.body_heave;
        target.scalar(0).set(
            vehicle_f32((double(basis[8]) * point.z + double(basis[0]) * point.x) + double(basis[4]) * y));
        target.scalar(4).set(
            vehicle_f32((double(basis[9]) * point.z + double(basis[1]) * point.x) + double(basis[5]) * y));
        target.scalar(8).set(
            vehicle_f32((double(basis[10]) * point.z + double(basis[2]) * point.x) + double(basis[6]) * y));
    };
    store(0);
    if (anchors.effect_available[1] != 0)
        store(1);
}
} // namespace
void prepare_vehicle_model(const VehicleState &vehicle, const VehicleTrailInputs &inputs,
                           std::span<const VehicleWheelAnimation, 4> wheels,
                           const VehiclePresentationState &presentation, const VehicleAnchorState &anchors,
                           const VehicleSegments &segments, SourcePmdModelView model, VehicleMatrix &basis,
                           contact::Vector4 &effect_position)
{
    body_4138a0(vehicle, inputs, model);
    wheels_4139f0(vehicle, wheels, model);
    basis_413b60(model, basis);
    segments_413e60(vehicle, presentation, anchors, segments, basis, model, effect_position);
    effects_414130(vehicle, anchors, basis, model);
}
void prepare_vehicle_model(const VehicleState &vehicle, const VehicleTrailInputs &inputs,
                           std::span<const VehicleWheelAnimation, 4> wheels,
                           const VehiclePresentationState &presentation, const VehicleAnchorState &anchors,
                           const VehicleSegments &segments, ModelResources &models, std::size_t handle,
                           VehicleMatrix &basis, contact::Vector4 &effect_position)
{
    if (models.source_backed(handle))
        prepare_vehicle_model(vehicle, inputs, wheels, presentation, anchors, segments, models.source_at(handle), basis,
                              effect_position);
    else
        prepare_vehicle_model(vehicle, inputs, wheels, presentation, anchors, segments, models.at(handle),
                              models.current_detail_level(handle), basis, effect_position);
}
} // namespace sgn::retail
