#include "sgn/source_trail_packet_writer.hpp"
#include "sgn/retail_model_matrices.hpp"
#include <bit>
#include <cstring>
#include <limits>
#include "sgn/source_screen_transport.hpp"
#include "sgn/source_bound_buffer_access.hpp"
#include "sgn/host_profiler.hpp"

struct SourceTrailBillboardInputs
{
    const sgn::retail::SourceTrailPolygon *trail;
    const std::uint32_t *rectangle;
    const float *matrix;
};
static_assert(sizeof(SourceTrailBillboardInputs) == 24);
static_assert(offsetof(SourceTrailBillboardInputs, rectangle) == 8 &&
              offsetof(SourceTrailBillboardInputs, matrix) == 16);
static_assert(offsetof(sgn::retail::SourceTrailPolygon, position) == 0 &&
              offsetof(sgn::retail::SourceTrailPolygon, scale) == 0x10);
extern "C" void sgn_source_trail_vertex_4344ad(const SourceTrailBillboardInputs *, void *) noexcept;

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
template <class T> W count(const SourceWorldSpan<T> &p)
{
    const auto &b = p.source_binding();
    const W n = b.byte_count == std::numeric_limits<W>::max() ? b.bytes : b.byte_count;
    if (n % sizeof(T))
        throw SourceAllocationError(0x4342e0, "source trail pointer stride");
    return n / static_cast<W>(sizeof(T));
}
template <class T> SourceWorldSpan<T> next(const SourceWorldSpan<T> &p)
{
    const W n = count(p);
    if (n == 0)
        throw SourceAllocationError(0x4342e0, "source trail pointer leaves allocation");
    return p.source_subspan(1, n - 1);
}
template <class T> W difference(const SourceWorldSpan<T> &p, const SourceWorldSpan<T> &base)
{
    const auto &a = p.source_binding();
    const auto &b = base.source_binding();
    if (a.bank != b.bank || a.allocation != b.allocation || a.bytes != b.bytes || a.byte_offset < b.byte_offset ||
        a.platform.owner_before(b.platform) || b.platform.owner_before(a.platform))
        throw SourceAllocationError(0x4342e0, "source trail pointer subtraction needs common actual owner");
    const W bytes = a.byte_offset - b.byte_offset;
    if (bytes % sizeof(T))
        throw SourceAllocationError(0x4342e0, "source trail pointer subtraction alignment");
    return bytes / static_cast<W>(sizeof(T));
}
template <class T> void same(const SourceWorldSpan<T> &p, const std::shared_ptr<RaceSessionPlatform> &platform)
{
    const auto &b = p.source_binding();
    if (b.platform.owner_before(platform) || platform.owner_before(b.platform))
        throw SourceAllocationError(0x4342e0, "source trail pointer belongs to another platform");
}
// R19: every span reaching these three helpers was admitted against the one
// locked platform of quad()/billboard() (difference()/same()), so the former
// per-access SourceTransportBuffer lock cannot fail; bound_buffer performs the
// identical extent/generation checks and bank call (source_bound_buffer_access.hpp).
template <class T, class V> void store(const SourceWorldSpan<T> &p, W at, V value)
{
    bound_buffer::known(p.source_binding(), at,
                        std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(&value), sizeof(V)),
                        0x4342e0);
}
template <class T> void transport(const SourceWorldSpan<T> &p, W at, const SourceTransportWord &value, W site)
{
    bound_buffer::store(p.source_binding(), at, value, site);
}
template <class V> V read(const SourceWorldSpan<std::uint8_t> &p, W at)
{
    const auto range = bound_buffer::read(p.source_binding(), at, static_cast<W>(sizeof(V)), 0x434311);
    V value;
    std::memcpy(&value, range.data(), sizeof(V));
    return value;
}
struct TrailInput
{
    const SourceTrailPolygon *native{};
    const SourceTransportStorage<SourceTrailPolygon> *source{};
    SourceTransportWord word(W at, W site) const
    {
        if (source)
            return source->load(at, site);
        if (at > sizeof(SourceTrailPolygon) - 4)
            throw SourceAllocationError(site, "trail input leaves160-byte owner");
        W value;
        std::memcpy(&value, reinterpret_cast<const std::uint8_t *>(native) + at, 4);
        return SourceTransportWord::known(value);
    }
    W number(W at, W site) const
    {
        return word(at, site).numeric(site);
    }
    W narrow(W at, W n, W site) const
    {
        W result = 0;
        for (W i = 0; i < n; ++i)
        {
            const auto byte =
                source ? source->byte(at + i, site) : reinterpret_cast<const std::uint8_t *>(native)[at + i];
            result |= W(byte) << (i * 8);
        }
        return result;
    }
    const SourceTrailPolygon *xyz(W site) const
    {
        static constexpr std::array<unsigned, 6> fields = {0, 4, 8, 16, 20, 24};
        return source ? static_cast<const SourceTrailPolygon *>(source->qualified(fields, site)) : native;
    }
};
} // namespace
static bool quad(SourceSpritePacketStorage &pool, const TrailInput &s)
{
    SGN_PROFILE_SCOPE("entities.trail_quad_4342e0");
    auto header = pool.current_headers;
    if (difference(header, pool.headers.source()) >= 512)
        return false; // 4342E4..4342F8
    const auto platform = pool.headers.source().source_binding().platform.lock();
    if (!platform)
        throw SourceAllocationError(0x4342fe, "source trail platform expired");
    const auto part = pool.current_part;
    same(part, platform);
    const auto low_count = static_cast<std::uint16_t>(s.narrow(0x2c, 2, 0x434304)); // 434304
    const auto index_base = pool.indices.source();
    const auto advanced_header = next(header);
    store(part, 0x24, static_cast<std::uint16_t>(read<std::uint16_t>(part, 0x24) + 1u));
    store(part, 0x26, static_cast<std::uint16_t>(read<std::uint16_t>(part, 0x26) + low_count));
    store(part, 0x28, static_cast<std::uint16_t>(read<std::uint16_t>(part, 0x28) + low_count));
    store(header, 0, static_cast<std::uint8_t>(s.narrow(0x2c, 1, 0x43431d)));
    const auto flags = static_cast<std::uint8_t>(s.narrow(0x24, 1, 0x434323) | 0x80u);
    pool.current_headers = advanced_header; // 434329 BEFORE remaining header fields
    store(header, 3, flags);
    store(header, 1, static_cast<std::uint8_t>(s.narrow(0x20, 1, 0x434331)));
    store(header, 2, static_cast<std::uint8_t>(s.narrow(0x28, 1, 0x434337)));
    store(header, 6, static_cast<std::uint16_t>(difference(pool.current_indices, index_base)));
    const auto vertices = s.number(0x2c, 0x43434b);
    if (vertices == 0)
        return true; // 43434B..350, retains header/counter publications
    auto positions = pool.current_positions;
    same(positions, platform);
    auto attributes = pool.current_attributes;
    same(attributes, platform);
    for (W i = 0; i < vertices; ++i)
    {
        const auto ordinal = difference(positions, pool.positions.source());
        store(attributes, 8, static_cast<std::uint16_t>(ordinal)); // 434377
        // Reached source input only now, after the ordinal output write.
        if (i >= 4)
            throw SourceAllocationError(0x43437b, "trail input leaves actual160-byte staging");
        const W in = 0x30u + i * 28u;
        transport(attributes, 0, s.word(in + 16, 0x43437b), 0x43437e);
        transport(attributes, 4, s.word(in + 20, 0x434380), 0x434383);
        transport(attributes, 12, s.word(in + 24, 0x434386), 0x434388);
        const auto destination = positions;
        positions = next(positions); // heldEDX, global unchanged
        transport(destination, 0, s.word(in + 0, 0x434394), 0x434396);
        transport(destination, 4, s.word(in + 4, 0x434398), 0x43439b);
        transport(destination, 8, s.word(in + 8, 0x43439e), 0x4343a1);
        const auto attribute_base = pool.attributes.source();            // 4343A4 before W load/store
        transport(destination, 12, s.word(in + 12, 0x4343aa), 0x4343ad); // copies actual retained571330 W
        auto indices = pool.current_indices;
        same(indices, platform);
        const auto old_indices = indices;
        indices = next(indices);
        const auto attribute_ordinal = difference(attributes, attribute_base);
        pool.current_indices = indices; // 4343BD BEFORE index write
        store(old_indices, 0, static_cast<std::uint16_t>(attribute_ordinal));
        attributes = next(attributes);
    }
    pool.current_attributes = attributes; // 4343D6
    pool.current_positions = positions;   // 4343DB, only after all vertices
    return true;
}
static bool billboard(SourceSpritePacketStorage &pool, const TrailInput &s, LightCameraView view)
{
    SGN_PROFILE_SCOPE("entities.trail_billboard_4343f0");
    if (difference(pool.current_headers, pool.headers.source()) >= 512)
        return false;
    // 4280A0 consumes only the pointed position/scale. These are local call
    // arguments, never a scene/model/geometry replacement or a retained owner.
    SourceSpriteRecord classification;
    const auto *geometric = s.xyz(0x4280a0);
    // 4280A0 reads XYZ only. This transient ABI argument is not a source W owner.
    classification.position.x = geometric->position.x;
    classification.position.y = geometric->position.y;
    classification.position.z = geometric->position.z;
    classification.scale = {geometric->scale.x, geometric->scale.y, geometric->scale.z};
    if (classify_sprite(classification, view) == 1)
        return false;
    const auto platform = pool.headers.source().source_binding().platform.lock();
    if (!platform)
        throw SourceAllocationError(0x434422, "source trail billboard platform expired");
    const auto part = pool.current_part;
    same(part, platform);
    const auto texture = static_cast<std::uint8_t>(s.narrow(0x20, 1, 0x434331));
    store(part, 0x24, static_cast<std::uint16_t>(read<std::uint16_t>(part, 0x24) + 1u));
    store(part, 0x26, static_cast<std::uint16_t>(read<std::uint16_t>(part, 0x26) + 4u));
    store(part, 0x28, static_cast<std::uint16_t>(read<std::uint16_t>(part, 0x28) + 4u));
    const auto header = pool.current_headers;
    auto positions = pool.current_positions;
    same(positions, platform);
    store(header, 0, std::uint8_t{4});
    store(header, 3, static_cast<std::uint8_t>(s.narrow(0x24, 1, 0x434323) | 0x80u));
    const auto alpha = static_cast<std::uint8_t>(s.narrow(0x28, 1, 0x434337));
    store(header, 1, texture);
    const auto captured_indices = pool.current_indices;
    same(captured_indices, platform);
    store(header, 2, alpha);
    store(header, 6, static_cast<std::uint16_t>(difference(captured_indices, pool.indices.source())));
    const auto position_base = pool.positions.source();
    pool.current_headers = next(header); // 43447B
    const auto *current_camera = &view.camera;
    auto ordinal = difference(positions, position_base);
    const auto matrix_reference = current_camera->camera_to_world_reference; // 43448A
    auto attributes = pool.current_attributes;
    same(attributes, platform);
    for (W i = 0; i < 4; ++i)
    {
        const auto &rectangle = pool.source_rectangle[i];
        store(attributes, 8, static_cast<std::uint16_t>(ordinal)); // 4344BE
        store(attributes, 12, s.number(0x48 + i * 28u, 0x4344c2)); // 4344D3, distinct per corner
        store(attributes, 0, rectangle[4]);
        store(attributes, 4, rectangle[5]);
        const auto attribute_base = pool.attributes.source();
        if (!view.source_matrices || !view.source_identity)
            throw SourceAllocationError(0x4344ca, "source billboard current camera matrix owners not bound");
        const auto &matrix = resolve_model_matrix(*view.source_matrices, *view.source_identity, matrix_reference);
        auto bytes =
            bound_buffer::writable_known(positions.source_binding(), 0, 12, 0x43451f); // positions admitted by same()
        if (bytes.size() < 12)
            throw SourceAllocationError(0x43451f, "source billboard XYZ leaves actual allocation");
        const SourceTrailBillboardInputs inputs{s.xyz(0x4344ad), rectangle.data(), matrix.data()};
        sgn_source_trail_vertex_4344ad(&inputs, bytes.data()); // XYZ only; destinationW retained
        const auto index = difference(attributes, attribute_base);
        attributes = next(attributes);
        auto indices = pool.current_indices;
        same(indices, platform);
        store(indices, 0, static_cast<std::uint16_t>(index));
        indices = next(indices);
        ++ordinal;
        pool.current_indices = indices;     // 43454A
        positions = pool.current_positions; // 434559 fresh working pointer
        positions = next(positions);
        pool.current_positions = positions; // 434569
    }
    pool.current_attributes = attributes; // 434579 after four complete iterations
    return true;
}
bool append_source_trail_quad_4342e0(SourceSpritePacketStorage &pool, const SourceTrailPolygon &s)
{
    return quad(pool, {&s, nullptr});
}
bool append_source_trail_quad_4342e0(SourceSpritePacketStorage &pool,
                                     const SourceTransportStorage<SourceTrailPolygon> &s)
{
    return quad(pool, {nullptr, &s});
}
bool append_source_trail_billboard_4343f0(SourceSpritePacketStorage &pool, const SourceTrailPolygon &s,
                                          LightCameraView view)
{
    return billboard(pool, {&s, nullptr}, view);
}
bool append_source_trail_billboard_4343f0(SourceSpritePacketStorage &pool,
                                          const SourceTransportStorage<SourceTrailPolygon> &s, LightCameraView view)
{
    return billboard(pool, {nullptr, &s}, view);
}

} // namespace sgn::retail
