#include "sgn/retail_light_effects.hpp"
#include "sgn/retail_model_matrices.hpp"
#include <bit>
#include <cstring>
#include <limits>
#include "sgn/source_screen_transport.hpp"
#include "sgn/source_bound_buffer_access.hpp"

// Only pointers to actual invocation owners. No geometry/input value mirror.
struct SourceSpriteVertexInputs
{
    const sgn::retail::SourceSpriteRecord *sprite;
    const std::uint32_t *rectangle;
    const sgn::retail::CameraVector *right;
    const sgn::retail::CameraVector *up;
    const sgn::retail::CameraVector *forward;
    const float *camera_matrix;
};
static_assert(sizeof(SourceSpriteVertexInputs) == 48 && offsetof(SourceSpriteVertexInputs, camera_matrix) == 40);
extern "C" void sgn_source_sprite_vertex_4346cd(const SourceSpriteVertexInputs *, void *) noexcept;
extern "C" void sgn_source_sprite_cross_43462f(const void *, const void *, void *) noexcept;

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
template <class T> W remaining(const SourceWorldSpan<T> &pointer)
{
    const auto &p = pointer.source_binding();
    const W count = p.byte_count == std::numeric_limits<W>::max() ? p.bytes : p.byte_count;
    if (count % sizeof(T))
        throw SourceAllocationError(0x434590, "source sprite pointer stride");
    return count / static_cast<W>(sizeof(T));
}
template <class T> SourceWorldSpan<T> advance(const SourceWorldSpan<T> &pointer)
{
    const auto count = remaining(pointer);
    if (count == 0)
        throw SourceAllocationError(0x434590, "source sprite pointer leaves its actual allocation");
    return pointer.source_subspan(1, count - 1);
}
// Pointer subtraction uses allocation identity/offset, never host address bits
// or token arithmetic. No pointed bytes are read by this metadata operation.
template <class T> W difference(const SourceWorldSpan<T> &pointer, const SourceWorldSpan<T> &base)
{
    const auto &p = pointer.source_binding();
    const auto &b = base.source_binding();
    if (p.bank != b.bank || p.allocation != b.allocation || p.bytes != b.bytes || p.byte_offset < b.byte_offset ||
        p.platform.owner_before(b.platform) || b.platform.owner_before(p.platform))
        throw SourceAllocationError(0x434590, "source sprite pointer difference needs its actual common owner");
    const W bytes = p.byte_offset - b.byte_offset;
    if (bytes % sizeof(T))
        throw SourceAllocationError(0x434590, "source sprite pointer difference has invalid alignment");
    return bytes / static_cast<W>(sizeof(T));
}
template <class T>
void require_platform(const SourceWorldSpan<T> &pointer, const std::shared_ptr<RaceSessionPlatform> &platform)
{
    const auto &p = pointer.source_binding();
    if (p.platform.owner_before(platform) || platform.owner_before(p.platform))
        throw SourceAllocationError(0x434590, "source sprite current pointer belongs to another platform");
}
// R19 (perf_entities): every span reaching store/read was admitted by
// require_platform()/difference() against the platform locked at entry, so the
// former per-access SourceTransportBuffer lock cannot fail; bound_buffer keeps
// the identical extent/generation checks and bank call.
template <class T, class V> void store(const SourceWorldSpan<T> &pointer, W offset, V value)
{
    bound_buffer::known(pointer.source_binding(), offset,
                        std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(&value), sizeof(V)),
                        0x434590);
}
template <class V> V read(const SourceWorldSpan<std::uint8_t> &pointer, W offset)
{
    const auto bytes = bound_buffer::read(pointer.source_binding(), offset, static_cast<W>(sizeof(V)), 0x4345c3);
    V value;
    std::memcpy(&value, bytes.data(), sizeof(V));
    return value;
}
} // namespace
bool append_source_sprite_packet_434590(SourceSpritePacketStorage &pool, const SourceSpriteRecord &sprite,
                                        LightCameraView view)
{
    // Invocation lifetime only. The actual allocator checks its owner thread
    // at each reached byte access. No storage or preview counter is reset.
    const auto platform = pool.headers.source().source_binding().platform.lock();
    if (!platform)
        throw SourceAllocationError(0x434590, "source sprite platform expired");
    const auto header_count = difference(pool.current_headers, pool.headers.source());
    if (header_count >= 512)
        return false; // 43459E..4345A9, before camera/input reads
    if (classify_sprite(sprite, view) == 1)
        return false; // only required child4280A0

    const auto part = pool.current_part;
    require_platform(part, platform);
    if (difference(part, pool.source_part) != 0)
        throw SourceAllocationError(0x4345c3, "source sprite current part leaves its actual record");
    const auto texture = static_cast<std::uint8_t>(sprite.texture);
    store(part, 0x24, static_cast<std::uint16_t>(read<std::uint16_t>(part, 0x24) + 1u));
    store(part, 0x26, static_cast<std::uint16_t>(read<std::uint16_t>(part, 0x26) + 4u));
    store(part, 0x28, static_cast<std::uint16_t>(read<std::uint16_t>(part, 0x28) + 4u));
    const auto header = pool.current_headers;
    store(header, 0, std::uint8_t{4});
    store(header, 3, static_cast<std::uint8_t>(sprite.flags | 0x80u));
    const auto alpha = static_cast<std::uint8_t>(sprite.alpha);
    store(header, 1, texture);
    const auto captured_indices = pool.current_indices;
    require_platform(captured_indices, platform);
    store(header, 2, alpha);
    const auto first_index = difference(captured_indices, pool.indices.source());
    store(header, 6, static_cast<std::uint16_t>(first_index));

    pool.up.x = sprite.up.x; // 434615, actual5DA1F0
    auto positions = pool.current_positions;
    require_platform(positions, platform); // retained EDI
    pool.up.y = sprite.up.y;
    const auto next_header = advance(header); // register advance, not global publication
    pool.up.z = sprite.up.z;
    pool.current_headers = next_header; // 434638
    const auto position_base = pool.positions.source();
    const auto *captured_camera = &view.camera; // 434655, independent of cached rotation value
    const auto opaque_up = sprite.up.w;
    auto position_ordinal = difference(positions, position_base);
    pool.up.w = opaque_up;                                                    // 43465F
    const auto matrix_reference = captured_camera->camera_to_world_reference; // 434667
    auto attributes = pool.current_attributes;
    require_platform(attributes, platform); // held ECX, published only at return
    sgn_source_sprite_cross_43462f(&pool.forward, &pool.up, &pool.right);

    for (unsigned i = 0; i < 4; ++i)
    {
        const auto &rectangle = pool.source_rectangle[i]; // actual mutable478910+i*28
        store(attributes, 8, static_cast<std::uint16_t>(position_ordinal));
        store(attributes, 12, sprite.colors[0]);
        store(attributes, 0, rectangle[4]);
        store(attributes, 4, rectangle[5]);
        const auto attribute_base = pool.attributes.source(); // 434714, captured before vertex writes
        // 43475E first matrix payload read. A bad matrix reference preserves
        // the header/counters/axes/attribute writes already performed.
        if (!view.source_matrices || !view.source_identity)
            throw SourceAllocationError(0x43475e, "source sprite matrix owners were not bound");
        const auto &matrix = resolve_model_matrix(*view.source_matrices, *view.source_identity, matrix_reference);
        const SourceSpriteVertexInputs inputs{&sprite,  rectangle.data(), &pool.right,
                                              &pool.up, &pool.forward,    matrix.data()};
        auto position_bytes =
            bound_buffer::writable_known(positions.source_binding(), 0, 12, 0x4347a6); // admitted positions
        if (position_bytes.size() < 12)
            throw SourceAllocationError(0x4347a6, "source sprite position leaves actual allocation");
        sgn_source_sprite_vertex_4346cd(&inputs, position_bytes.data()); // XYZ only; W retained
        const auto attribute_ordinal = difference(attributes, attribute_base);
        auto indices = pool.current_indices;
        require_platform(indices, platform); // fresh5DA250 each vertex
        attributes = advance(attributes);    // held ECX+10; global5DA258 unchanged
        store(indices, 0, static_cast<std::uint16_t>(attribute_ordinal));
        ++position_ordinal;
        indices = advance(indices);
        positions = pool.current_positions; // 4347D7 fresh5DA254
        pool.current_indices = indices;     // 4347DD
        positions = advance(positions);
        pool.current_positions = positions; // 4347F2
    }
    pool.current_attributes = attributes; // 434802, only after all four vertices
    return true;
}
} // namespace sgn::retail
