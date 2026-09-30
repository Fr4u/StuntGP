#include "sgn/retail_vehicle_strip_draw.hpp"

namespace sgn::retail
{
bool append_vehicle_strip_draw(VehicleStripGeometry &geometry, std::uint32_t quality, ModelTransform &pose,
                               ModelDrawQueue &queue, ViewMatrixArena &matrices)
{
    if (quality < 80 || geometry.field_5712f8 == 0)
        return false; // 41D1C0..41D1D0
    if (!queue.cursor || *queue.cursor >= queue.records.size() - 1)
        throw FormatError("generated strip queue lacks command and terminator");
    allocate_outer_model_matrices(pose, matrices);
    // Temporary marshaling of identical80-byte source records between native
    // types. The geometry remains the sole persistent owner of474820.
    auto part = std::bit_cast<ModelTransform>(geometry.part_transform);
    const Vec3 pivot{};
    const std::array<ModelPartBinding, 1> parts{{{geometry.part_flags, part, pivot}}};
    allocate_model_part_matrices(parts, matrices);
    geometry.part_transform = std::bit_cast<VehicleStripTransform>(part);
    pose.tail[0] = 0;
    queue.records[*queue.cursor] = {strip_model_identity, pose};
    clear_model_draw_flags(geometry.model_flags_02); // caller always supplies flags0
    ++*queue.cursor;
    queue.records[*queue.cursor].model_identity = 0;
    return true;
}
} // namespace sgn::retail
