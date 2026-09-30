#include "sgn/retail_model_matrices.hpp"
#include "sgn/retail_vehicle_strip.hpp"
#include <bit>
#include <cmath>
#include <limits>

namespace sgn::retail
{
namespace
{
float f32(double v)
{
    return static_cast<float>(v);
}
void capacity(const ViewMatrixArena &arena, std::size_t count)
{
    if (count == 0)
        return;
    if (!arena.cursor || *arena.cursor % 64 != 0 || *arena.cursor > ViewMatrixArena::capacity * 64 ||
        count > (ViewMatrixArena::capacity * 64 - *arena.cursor) / 64)
        throw FormatError("model matrix allocation outside source arena");
}
std::array<CameraMatrix, 2> rotations(VehicleVector z, VehicleVector y, bool outer)
{
    const auto dot = [&] { return (double(y.z) * z.z + double(y.y) * z.y) + double(y.x) * z.x; };
    if (std::abs(dot()) > std::bit_cast<float>(0x3f7fff58u))
    {
        y.z = f32(double(y.z) + 1);
        y = vehicle_normalize(y);
    }
    const double projection = dot();
    // 44FDB0 stores X/Y products before subtraction; Z stays in x87.
    // 44FF90 retains all three products until each subtraction.
    const double px = outer ? double(f32(projection * z.x)) : projection * z.x;
    const double py = outer ? double(f32(projection * z.y)) : projection * z.y;
    y.x = f32(double(y.x) - px);
    y.y = f32(double(y.y) - py);
    y.z = f32(double(y.z) - projection * z.z);
    y = vehicle_normalize(y);
    const float x = f32(-(double(y.z) * z.y - double(y.y) * z.z));
    const float b = -f32(double(y.x) * z.z - double(y.z) * z.x);
    const float c = -f32(double(y.y) * z.x - double(y.x) * z.y);
    CameraMatrix first{x, b, c, 0, y.x, y.y, y.z, 0, z.x, z.y, z.z, 0, 0, 0, 0, 1};
    CameraMatrix second{x, y.x, z.x, 0, b, y.y, z.y, 0, c, y.z, z.z, 0, 0, 0, 0, 1};
    return {first, second};
}
void publish(ViewMatrixArena &arena, const std::array<CameraMatrix, 2> &matrices, std::uint32_t &first,
             std::uint32_t &second)
{
    const auto offset = *arena.cursor;
    const auto index = offset / 64;
    arena.matrices[index] = matrices[0];
    arena.matrices[index + 1] = matrices[1];
    first = offset + 2;
    second = offset + 66;
    arena.cursor = offset + 128;
    arena.count += 2;
}
template <class Store>
void publish_source_part(ViewMatrixArena &arena, const std::array<CameraMatrix, 2> &values, Store store)
{
    const auto offset = *arena.cursor;
    const auto index = offset / 64;
    auto &first = arena.matrices[index];
    auto &second = arena.matrices[index + 1];
    for (const auto lane : {2u, 0u, 3u, 1u, 4u, 5u, 6u, 8u, 9u})
        first[lane] = values[0][lane];
    second[0] = values[1][0];
    first[10] = values[0][10];      // 4500C0,4500C3
    const auto count = arena.count; // 4500C6, before tail references
    for (const auto lane : {7u, 11u, 12u, 13u, 14u, 15u})
        first[lane] = values[0][lane];
    store(0, offset + 2u); // 4500E2, actual transform+40
    for (const auto lane : {1u, 2u, 3u, 4u, 7u, 5u, 6u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u})
        second[lane] = values[1][lane];
    store(1, offset + 66u); // 450132, actual transform+44
    arena.count = count + 2u;
    arena.cursor = offset + 128u; // 45013A,450140
}
} // namespace
const CameraMatrix &resolve_arena_matrix(const ViewMatrixArena &arena, std::uint32_t reference)
{
    if (reference < 2 || (reference - 2) % 64 != 0 || (reference - 2) / 64 >= ViewMatrixArena::capacity)
        throw FormatError("invalid native model matrix reference");
    return arena.matrices[(reference - 2) / 64];
}
const CameraMatrix &resolve_model_matrix(const ViewMatrixArena &arena, const CameraMatrix &identity,
                                         std::uint32_t reference)
{
    if (reference == shared_model_matrix_reference)
        return identity;
    return resolve_arena_matrix(arena, reference);
}
void allocate_camera_view_matrices(CameraState &pose, ViewMatrixArena &arena, float current_camera_twist)
{
    capacity(arena, 2);
    // 4502C4..4502D1 reads the global current camera, not ECX's pose owner.
    auto values = pose;
    values.twist = current_camera_twist;
    camera_view_basis(values);
    // 450334/450390 publish first camera+50, then camera+4C. Matrices are
    // written at450337..4503D9; count/cursor at4503E4/4503EA retain wrap.
    publish(arena, {values.camera_to_world_rotation, values.world_to_camera_rotation}, pose.camera_to_world_reference,
            pose.world_to_camera_reference);
    pose.camera_to_world_rotation = resolve_arena_matrix(arena, pose.camera_to_world_reference);
    pose.world_to_camera_rotation = resolve_arena_matrix(arena, pose.world_to_camera_reference);
}
void allocate_outer_model_matrices(ModelTransform &pose, ViewMatrixArena &arena)
{
    capacity(arena, 2);
    auto forward =
        vehicle_normalize({f32(double(pose.position.x) - pose.forward.x), f32(double(pose.position.y) - pose.forward.y),
                           f32(double(pose.position.z) - pose.forward.z), 0});
    const auto result = rotations(forward, {pose.up.x, pose.up.y, pose.up.z, 0}, true);
    publish(arena, result, pose.tail[1], pose.tail[2]);
}
void allocate_model_part_matrices(std::span<const ModelPartBinding> parts, ViewMatrixArena &arena)
{
    std::size_t needed{};
    for (const auto &part : parts)
        if (part.flags != 0)
        {
            if (needed > ViewMatrixArena::capacity - 2)
                throw FormatError("too many active model parts for source matrix arena");
            needed += 2;
        }
    capacity(arena, needed);
    for (const auto &part : parts)
    {
        auto &t = part.transform;
        if (part.flags == 0)
        {
            t.tail[0] = shared_model_matrix_reference;
            t.tail[1] = shared_model_matrix_reference;
        }
        else
        {
            const auto result =
                rotations({t.forward.x, t.forward.y, t.forward.z, 0}, {t.up.x, t.up.y, t.up.z, 0}, false);
            publish(arena, result, t.tail[0], t.tail[1]);
        }
    }
}
void allocate_model_part_matrices(SourcePmdModelView model, unsigned source_lod, ViewMatrixArena &arena)
{
    const auto lod = model.source_lod_index(source_lod), count = model.part_count();
    std::size_t needed = 0;
    for (unsigned i = 0; i < count; ++i)
    {
        const auto part = model.part(lod, i);
        (void)part.transform().location();
        if (part.flags().value() != 0)
        {
            if (needed > ViewMatrixArena::capacity - 2)
                throw FormatError("too many active source PMD parts for matrix arena");
            needed += 2;
        }
    }
    capacity(arena, needed);
    for (unsigned i = 0; i < count; ++i)
    {
        const auto part = model.part(lod, i);
        const auto field = part.transform();
        const auto at = field.location().offset;
        std::uint32_t first = shared_model_matrix_reference, second = shared_model_matrix_reference;
        if (part.flags().value() != 0)
        {
            const auto transform = field.value();
            publish(arena, rotations(transform.forward, transform.up, false), first, second);
        }
        // 450150/44FF90 stores only these two words. Shared transform aliases
        // observe the preceding write immediately, including disabled parts.
        model.bytes().field<std::uint32_t>(at + 0x40).set(first);
        model.bytes().field<std::uint32_t>(at + 0x44).set(second);
    }
}
void allocate_model_part_matrices(ModelResource &model, unsigned source_lod, ViewMatrixArena &arena)
{
    const auto lod = model.source_lod_index(source_lod);
    const auto count = model.part_count(lod);
    std::vector<ModelPartBinding> parts;
    parts.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        parts.push_back(model.part(lod, static_cast<unsigned>(i)));
    allocate_model_part_matrices(parts, arena);
}
void allocate_source_outer_model_matrices_44fdb0(ModelTransform &pose, ViewMatrixArena &arena)
{
    capacity(arena, 2); // NEW host extent admission for the reached pair only
    const auto forward =
        vehicle_normalize({f32(double(pose.position.x) - pose.forward.x), f32(double(pose.position.y) - pose.forward.y),
                           f32(double(pose.position.z) - pose.forward.z), 0});
    const auto values = rotations(forward, {pose.up.x, pose.up.y, pose.up.z, 0}, true);
    const auto offset = *arena.cursor;
    const auto index = offset / 64;
    auto &first = arena.matrices[index];
    auto &second = arena.matrices[index + 1];
    for (const auto lane : {2u, 0u, 3u, 1u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u})
        first[lane] = values[0][lane];
    second[0] = values[1][0];
    pose.tail[1] = offset + 2u; // 44FF10,44FF16
    const auto count = arena.count;
    pose.tail[2] = offset + 66u; // 44FF1C,44FF23
    for (unsigned lane = 1; lane < 16; ++lane)
        second[lane] = values[1][lane];
    arena.count = count + 2u;
    arena.cursor = offset + 128u; // 44FF71,44FF77
}
void allocate_source_static_part_matrices_44ff90(ModelTransform &pose, ViewMatrixArena &arena)
{
    capacity(arena, 2);
    const auto values =
        rotations({pose.forward.x, pose.forward.y, pose.forward.z, 0}, {pose.up.x, pose.up.y, pose.up.z, 0}, false);
    publish_source_part(arena, values, [&](unsigned lane, std::uint32_t value) { pose.tail[lane] = value; });
}
void allocate_source_static_part_matrices_44ff90(VehicleStripTransform &pose, ViewMatrixArena &arena)
{
    capacity(arena, 2);
    const auto values =
        rotations({pose.forward.x, pose.forward.y, pose.forward.z, 0}, {pose.up.x, pose.up.y, pose.up.z, 0}, false);
    publish_source_part(arena, values, [&](unsigned lane, std::uint32_t value) { pose.tail[lane] = value; });
}
void allocate_source_model_part_matrices_450150(SourcePmdModelView model, ViewMatrixArena &arena)
{
    const auto descriptor = model.descriptor();
    const auto selector = descriptor.field<std::uint32_t>(0x5c).value(); // 450150
    if (selector >= 4u)
        throw SourceAllocationError(0x450154, "source matrix selector leaves the four descriptor slots");
    auto current = descriptor.field<std::uint32_t>(0x48u + selector * 4u).value(); // 450154, even count0
    auto count = descriptor.field<std::uint32_t>(0x24).value();                    // 450158, captured once
    while (count != 0)
    {
        // This existing projection checks immutable42ECA0 relocation history,
        // not current descriptor58 or a preview/reread selected-LOD table.
        const auto part = model.captured_base_part_42b055(current);
        const auto flags = part.flags().value(); // 450168 before transform pointer
        const auto transform = part.transform(); // one current part+08 capture
        const auto location = transform.location();
        const auto bytes = model.bytes().subview(location.offset, 80);
        if (flags == 0)
        {
            bytes.field<std::uint32_t>(0x40).set(shared_model_matrix_reference);
            bytes.field<std::uint32_t>(0x44).set(shared_model_matrix_reference);
        }
        else
        {
            capacity(arena, 2); // no preflight/reads of later source parts
            // Actual44FF90 operand order. W is copied to dead source locals;
            // it remains transport metadata and never becomes a numeric scalar.
            VehicleVector forward{}, up{}; // numeric XYZ operands only; not copies of source W
            forward.x = bytes.field<float>(0x20).value(0x44ff9e);
            forward.y = bytes.field<float>(0x24).value(0x44ffa4);
            forward.z = bytes.field<float>(0x28).value(0x44ffaa);
            const auto forward_w = bytes.load_transport_word(0x2c, 0x44ffb0);
            up.x = bytes.field<float>(0x30).value(0x44ffb9);
            up.y = bytes.field<float>(0x34).value(0x44ffbe);
            up.z = bytes.field<float>(0x38).value(0x44ffc4);
            const auto up_w = bytes.load_transport_word(0x3c, 0x44ffd6);
            (void)forward_w;
            (void)up_w; // 44FFB6/44FFE1 have no reached observer
            const auto values = rotations(forward, up, false);
            publish_source_part(arena, values, [&](unsigned lane, std::uint32_t reference) {
                bytes.field<std::uint32_t>(0x40u + lane * 4u).set(reference);
            });
        }
        if (current > std::numeric_limits<std::uint32_t>::max() - 0x40u)
            throw SourceAllocationError(0x450180, "source part increment leaves represented PMD offsets");
        current += 0x40u;
        --count;
    }
}
} // namespace sgn::retail
