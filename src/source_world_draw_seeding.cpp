#include "sgn/source_world_draw_seeding.hpp"
#include "sgn/source_sprite_scene.hpp"
#include "sgn/scene_resources.hpp"
#include <cstring>
#include <limits>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
template <class T> T read(const SourceWorldSpan<std::uint8_t> &memory, W offset)
{
    const auto bytes = memory.subspan(offset, sizeof(T));
    T value;
    std::memcpy(&value, bytes.data(), sizeof(T));
    return value;
}
template <class T> void write(const SourceWorldSpan<std::uint8_t> &memory, W offset, T value)
{
    const auto bytes = memory.subspan(offset, sizeof(T));
    std::memcpy(bytes.data(), &value, sizeof(T));
}
bool sprite_slot(const SceneResources &registry, WorldModelDrawState &state, SourceSpritePacketStorage &sprites)
{
    const auto &descriptor = registry.source_records()[0].descriptor; // 42B792
    if (std::holds_alternative<std::monostate>(descriptor))
        return false;
    const auto *binding = std::get_if<SceneSpriteBinding>(&descriptor);
    if (!binding || binding->model != &state || binding->packets != &sprites ||
        binding->source_descriptor != sprites.source_descriptor.source_binding().allocation)
        throw SourceAllocationError(0x42b792, "source sprite row names another descriptor or world");
    return bool(binding->source_descriptor); // source NULL store remains representable
}
void allocate_sprite_parts_450150(WorldModelDrawState &state, SourceSpritePacketStorage &sprites,
                                  ViewMatrixArena &matrices)
{
    const auto descriptor = sprites.source_descriptor; // the captured scene0 descriptor, not current_part
    const auto selector = read<W>(descriptor, 0x5c);   // 450150
    if (selector >= 4u)
        throw SourceAllocationError(0x450154, "generated source LOD leaves four actual descriptor slots");
    const auto token = read<W>(descriptor, 0x48u + selector * 4u); // 450154 even with count0
    auto count = read<W>(descriptor, 0x24);                        // 450158
    if (count == 0)
        return; // do not dereference a null/unreadable selected pointer
    const auto &descriptor_binding = descriptor.source_binding();
    auto &bank = *descriptor_binding.bank;
    auto current = bank.decode_address(token);
    const auto &actual = sprites.source_part.source_binding();
    while (count != 0)
    {
        // 434090 creates a single64-byte part. A captured selected pointer must
        // name its actual allocation, never a synthetic ModelPartBinding copy.
        if (current.allocation != actual.allocation || current.offset < actual.byte_offset)
            throw SourceAllocationError(0x450168, "generated selected part names an unrepresented owner");
        const auto offset = current.offset - actual.byte_offset;
        const auto part = sprites.source_part.source_subspan(offset, 0x40);
        const auto flags = read<std::uint16_t>(part, 0);
        const auto transform = read<W>(part, 8); // 45016E /44FF98
        if (transform != 0x4788c0u)
            throw SourceAllocationError(0x44ff98, "generated part transform does not name actual4788C0");
        if (flags == 0)
        {
            state.sprite_part_transform.tail[0] = shared_model_matrix_reference;
            state.sprite_part_transform.tail[1] = shared_model_matrix_reference;
        }
        else
            allocate_source_static_part_matrices_44ff90(state.sprite_part_transform, matrices);
        if (current.offset > std::numeric_limits<W>::max() - 0x40u)
            throw SourceAllocationError(0x450180, "generated part increment exceeds represented offset");
        current.offset += 0x40u;
        --count;
    }
}
void append_sprite_42b7f0(WorldModelDrawState &state, SourceSpritePacketStorage &sprites, ViewMatrixArena &matrices)
{
    auto &pose = state.template_pose;
    auto &queue = state.background;
    allocate_source_outer_model_matrices_44fdb0(pose, matrices); // 42B7FC
    allocate_sprite_parts_450150(state, sprites, matrices);      // 42B803
    const auto cursor = queue.cursor;
    pose.tail[0] = 0; // 42B808,42B815
    if (!cursor || *cursor >= queue.records.size() - 1)
        throw FormatError("source sprite queue lacks command/terminator");
    queue.records[*cursor].model_identity = sprite_model_identity; // actual scene0 identity
    static_assert(sizeof(ModelTransform) == 80);
    std::memcpy(&queue.records[*cursor].pose, &pose, sizeof(pose)); // 42B821, source's actual pose copy
    const auto flags = read<std::uint16_t>(sprites.source_descriptor, 2);
    write<std::uint16_t>(sprites.source_descriptor, 2, static_cast<std::uint16_t>(flags & 0xfff7u)); // 42B830 ->42B7E0
    if (!queue.cursor || *queue.cursor >= queue.records.size() - 1)
        throw FormatError("source sprite cursor advance outside owner");
    queue.cursor = *queue.cursor + 1u;
    queue.records[*queue.cursor].model_identity = 0;
}
} // namespace
void reset_source_sprite_cursors_434030(WorldModelDrawState &m, SourceSpritePacketStorage &s)
{
    s.current_headers = s.headers.source();       // 434041
    s.current_indices = s.indices.source();       // 43404B
    s.current_attributes = s.attributes.source(); // 434057
    s.current_positions = s.positions.source();   // 434061
    s.current_descriptor = s.source_descriptor;   // 434067
    m.sprite_model_flags.bind_source(s.current_descriptor, 2);
    m.sprite_outer_transform.bind_source(s.current_descriptor);
    const W previous = read<std::uint16_t>(s.source_part, 0x24); // 43406F
    write<std::uint16_t>(s.source_part, 0x24, 0);                // 434075
    write<std::uint16_t>(s.source_part, 0x26, 0);                // 434079
    s.current_part = s.source_part;                              // 43407D
    s.polygons.bind_source(s.current_part, 0x24);
    s.vertices.bind_source(s.current_part, 0x26);
    s.corners.bind_source(s.current_part, 0x28);
    m.sprite_part_flags.bind_source(s.current_part, 0);
    m.sprite_part_flags_02.bind_source(s.current_part, 2);
    m.previous_sprite_polygons = previous;        // 434082
    write<std::uint16_t>(s.source_part, 0x28, 0); // 434088
}
void seed_source_world_model_draws_42b740(WorldModelDrawState &state, ModelResources &models,
                                          SourceSpritePacketStorage &sprites, float ground, W scene,
                                          ViewMatrixArena &matrices, const SceneResources &registry)
{
    registry.require_owner_thread();
    if (&registry.models() != &models)
        throw SourceAllocationError(0x42b740, "source seed models belong to another registry");
    // NEW host owner admission only; do not dereference descriptor/part payloads
    // or validate future models here. Keep actual platform alive for this borrow.
    const auto &binding = sprites.source_part.source_binding();
    const auto platform = binding.platform.lock();
    if (!platform || !binding.bank)
        throw SourceAllocationError(0x42b740, "source seed has no live bank owner");
    reset_model_draw_queues(state.regular, state.background); // 42B745..42B766
    if (scene == 2)
        return;                                  // 42B770, no model or sprite work
    const auto track = registry.model_handle(3); // 42B772, not the caller's cached track handle
    if (track)
        append_source_model_draw_42b7f0(state.background, models.source_at(*track), models.identity(*track),
                                        state.template_pose, 0, matrices);
    const auto sky = registry.model_handle(2); // 42B6E6, after the track append
    if (sky)
    {
        auto pose = state.template_pose; // 42B700: all20 current DWORDs, including prior matrix refs
        pose.forward.x = vehicle_f32(double(pose.forward.x) + pose.position.x); // 42B702..708
        pose.position.y = ground;                                               // 42B717, not before the X addition
        pose.forward.y = vehicle_f32(double(pose.forward.y) + pose.position.y);
        pose.forward.z = vehicle_f32(double(pose.forward.z) + pose.position.z);
        append_source_model_draw_42b7f0(state.background, models.source_at(*sky), models.identity(*sky), pose, 0,
                                        matrices);
    }
    reset_source_sprite_cursors_434030(state, sprites); // 42B78D
    if (sprite_slot(registry, state, sprites))
        append_sprite_42b7f0(state, sprites, matrices); // 42B7A3
}
} // namespace sgn::retail
