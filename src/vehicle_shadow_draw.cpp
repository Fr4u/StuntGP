#include "sgn/vehicle_shadow_draw.hpp"

namespace sgn::retail
{
bool append_vehicle_shadow_draw(const VehiclePresentationState &presentation, const VehicleShadowState &shadow,
                                ModelResource &vehicle_model, ModelResource &model, unsigned lod,
                                std::uint32_t identity, ModelDrawQueue &queue, ViewMatrixArena &matrices,
                                const ModelTransform &template_pose)
{
    if (presentation.field_3928 == 0)
        return false;
    auto attributes = model.source_attribute_table();
    if (attributes.empty())
        throw FormatError("shadow descriptor has no first polygon attribute");
    auto pose = template_pose; // 41C44A: live copy, scale W and tail retained
    pose.position = {shadow.position.x, shadow.position.y, shadow.position.z};
    pose.position_w = shadow.position.opaque;
    pose.scale = {shadow.factor_392c, shadow.factor_392c, shadow.factor_392c};
    pose.forward = {vehicle_f32(double(shadow.forward.x) + shadow.position.x),
                    vehicle_f32(double(shadow.forward.y) + shadow.position.y),
                    vehicle_f32(double(shadow.forward.z) + shadow.position.z)};
    pose.forward_w = shadow.forward.opaque;
    pose.up = {shadow.normal.x, shadow.normal.y, shadow.normal.z};
    pose.up_w = shadow.normal.opaque;
    attributes[0][3] = static_cast<std::uint8_t>((attributes[0][3] & 0xf7u) | 0x70u);
    append_model_draw(queue, model, lod, identity, pose, 0, matrices);
    model.source_model_vector_10() = vehicle_model.source_model_vector_10();
    return true;
}
bool append_vehicle_shadow_draw(const VehiclePresentationState &presentation, const VehicleShadowState &shadow,
                                SourcePmdModelView vehicle_model, SourcePmdModelView model, std::uint32_t identity,
                                ModelDrawQueue &queue, ViewMatrixArena &matrices, const ModelTransform &template_pose)
{
    if (presentation.field_3928 == 0)
        return false;          // 41C42B..41C433, before any PMD access
    auto pose = template_pose; // 41C44A: current4864E8, including retained scale W/tail
    pose.position = {shadow.position.x, shadow.position.y, shadow.position.z};
    pose.position_w = shadow.position.opaque;
    pose.scale = {shadow.factor_392c, shadow.factor_392c, shadow.factor_392c};
    pose.forward = {vehicle_f32(double(shadow.forward.x) + shadow.position.x),
                    vehicle_f32(double(shadow.forward.y) + shadow.position.y),
                    vehicle_f32(double(shadow.forward.z) + shadow.position.z)};
    pose.forward_w = shadow.forward.opaque;
    pose.up = {shadow.normal.x, shadow.normal.y, shadow.normal.z};
    pose.up_w = shadow.normal.opaque;
    const auto attribute = model.first_polygon_attribute_flags();
    attribute.set(static_cast<std::uint8_t>((attribute.value() & 0xf7u) | 0x70u)); // 41C4E3..41C4EF: one byte
    append_model_draw(queue, model, model.current_detail_level().value(), identity, pose, 0, matrices); // 41C4F7
    // 41C503..41C528 resolves and copies the current four DWORDs after enqueue.
    //  Each read/write checks the actual bank generation and retains bit values.
    for (unsigned at = 0x10; at < 0x20; at += 4)
        model.descriptor().field<std::uint32_t>(at).set(vehicle_model.descriptor().field<std::uint32_t>(at).value());
    return true;
}
bool append_vehicle_shadow_draw(const VehiclePresentationState &presentation, const VehicleShadowState &shadow,
                                ModelResources &models, std::size_t vehicle_handle, std::size_t shadow_handle,
                                ModelDrawQueue &queue, ViewMatrixArena &matrices, const ModelTransform &template_pose)
{
    if (presentation.field_3928 == 0)
        return false;
    if (models.source_backed(shadow_handle))
        return append_vehicle_shadow_draw(presentation, shadow, models.source_at(vehicle_handle),
                                          models.source_at(shadow_handle), models.identity(shadow_handle), queue,
                                          matrices, template_pose);
    return append_vehicle_shadow_draw(presentation, shadow, models.at(vehicle_handle), models.at(shadow_handle),
                                      models.current_detail_level(shadow_handle), models.identity(shadow_handle), queue,
                                      matrices, template_pose);
}
} // namespace sgn::retail
