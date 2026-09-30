#include "sgn/source_world_strip.hpp"
#include "sgn/race_session.hpp"
#include "sgn/scene_resources.hpp"
#include "sgn/retail_model_matrices.hpp"
#include <bit>
#include <cstring>
#include <limits>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using H = std::uint16_t;
using B = std::uint8_t;
using Span = SourceWorldSpan<B>;
std::shared_ptr<RaceSessionPlatform> require(const std::weak_ptr<RaceSessionPlatform> &borrow)
{
    auto p = borrow.lock();
    if (!p)
        throw SourceAllocationError(0x41c530, "source strip platform expired");
    p->require_owner_thread();
    if (!p->source_construction_only_entry())
        throw SourceAllocationError(0x41c530, "strip requires actual source platform");
    return p;
}
struct Memory
{
    RaceSessionPlatform &platform;
    std::span<B> bytes(const Span &owner, W offset, W size, W site) const
    {
        const auto &binding = owner.source_binding();
        if (binding.platform.lock().get() != &platform || binding.bank != &platform.source_allocations())
            throw SourceAllocationError(site, "source strip buffer belongs to another platform");
        try
        {
            return owner.subspan(offset, size);
        }
        catch (const SourceAllocationError &e)
        {
            throw SourceAllocationError(site, e.what());
        }
    }
    template <class T> T read(const Span &owner, W offset, W site) const
    {
        const auto b = bytes(owner, offset, static_cast<W>(sizeof(T)), site);
        T value;
        std::memcpy(&value, b.data(), sizeof(T));
        return value;
    }
    template <class T> void write(const Span &owner, W offset, T value, W site) const
    {
        const auto b = bytes(owner, offset, static_cast<W>(sizeof(T)), site);
        std::memcpy(b.data(), &value, sizeof(T));
    }
    W token(const Span &owner) const
    {
        const auto &b = owner.source_binding();
        return platform.source_allocations().encode_address(b.allocation, b.byte_offset);
    }
    Span pointer(W token, W site) const
    {
        const auto at = platform.source_allocations().decode_address(token);
        if (!at.allocation)
            throw SourceAllocationError(site, "reached NULL strip pointer");
        const auto bytes = platform.source_allocations().bytes(at.allocation);
        if (bytes.size() > std::numeric_limits<W>::max())
            throw SourceAllocationError(site, "source strip allocation leaves DWORD extent");
        // Use an existing allocation's weak platform binding; no payload copy.
        const auto &any = platform.state().strip.source.polygons.source_binding();
        auto whole = Span::source(any.platform, at.allocation, static_cast<W>(bytes.size()));
        return whole.source_subspan(at.offset, bytes.size() - at.offset);
    }
};
void copy_words(const Memory &m, const Span &source, W source_at, const Span &target, W target_at, W words, W site)
{
    for (W i = 0; i < words; ++i)
        m.write<W>(target, target_at + i * 4, m.read<W>(source, source_at + i * 4, site), site);
}
void part_matrices_450150(const Memory &m, const Span &descriptor)
{
    auto &world = m.platform.state();
    const auto selected = m.read<W>(descriptor, 0x5c, 0x450150);
    if (selected >= 4)
        throw SourceAllocationError(0x450154, "strip selected LOD leaves four descriptor slots");
    const auto token = m.read<W>(descriptor, 0x48 + selected * 4, 0x450154);
    auto count = m.read<W>(descriptor, 0x24, 0x450158);
    if (!count)
        return;
    auto part = m.pointer(token, 0x450168);
    W offset = 0;
    do
    {
        const auto flags = m.read<H>(part, offset, 0x450168);
        const auto transform = m.read<W>(part, offset + 8, 0x44ff98);
        if (transform != 0x474820)
            throw SourceAllocationError(0x44ff98, "strip part transform leaves actual474820 owner");
        if (flags == 0)
        {
            world.strip.part_transform.tail[0] = shared_model_matrix_reference;
            world.strip.part_transform.tail[1] = shared_model_matrix_reference;
        }
        else
            allocate_source_static_part_matrices_44ff90(world.strip.part_transform, world.view_matrices);
        if (offset > std::numeric_limits<W>::max() - 64u)
            throw SourceAllocationError(0x450180, "strip part stride overflow");
        offset += 64;
        --count;
    } while (count);
}
} // namespace
void initialize_source_strip_geometry_41c530(const std::weak_ptr<RaceSessionPlatform> &borrow)
{
    const auto platform = require(borrow);
    auto &world = platform->state();
    auto &g = world.strip;
    auto &s = g.source;
    auto &bank = platform->source_allocations();
    const Memory m{*platform};
    // Each global is published only after its own call; the sixth result stays
    // local until41C5F4. Failed allocations are not globally preflighted.
    s.polygons = Span::source(platform, bank.allocate_42e8d0(0x400, 0), 0x400);
    s.indices = Span::source(platform, bank.allocate_42e8d0(0x400, 0), 0x400);
    s.vertices = Span::source(platform, bank.allocate_42e8d0(0x2000, 0), 0x2000);
    s.coordinates = Span::source(platform, bank.allocate_42e8d0(0x2000, 0), 0x2000);
    s.part = Span::source(platform, bank.allocate_42e8d0(0x40, 0), 0x40);
    const auto descriptor = Span::source(platform, bank.allocate_42e8d0(0x68, 0), 0x68);
    // Pinned474818 and4747D8 DWORD templates; literal writes, not native arrays.
    m.write<W>(s.polygons, 0, 0x50700504u, 0x41c5b2);
    m.write<W>(s.polygons, 4, 0, 0x41c5ba);
    constexpr W corners[]{0x3f800000, 0,          0x00370000, 0x3737, 0, 0,          0x00370001, 0x3737,
                          0x3f800000, 0x40000000, 0x00370002, 0x3737, 0, 0x40000000, 0x00370003, 0x3737};
    for (W i = 0; i < 16; ++i)
        m.write<W>(s.coordinates, i * 4, corners[i], 0x41c5c2);
    m.write<W>(s.vertices, 0x18, 0x42480000, 0x41c5ce);
    m.write<W>(s.vertices, 0x38, 0x42480000, 0x41c5d6);
    const auto part = s.part;
    m.write<W>(s.vertices, 8, 0xc2480000, 0x41c5df);
    m.write<W>(s.vertices, 0x20, 0x42c80000, 0x41c5e2);
    m.write<W>(s.vertices, 0x28, 0xc2480000, 0x41c5e5);
    m.write<W>(s.vertices, 0x30, 0x42c80000, 0x41c5e8);
    s.descriptor = descriptor; // 571314,41C5F4 before part clearing
    for (W offset = 0; offset < 64; offset += 4)
        m.write<W>(part, offset, 0, 0x41c5fa);
    m.write<H>(part, 0, 1, 0x41c5fc);
    m.write<H>(part, 2, 0, 0x41c607);
    m.write<H>(part, 4, 0, 0x41c60b);
    m.write<H>(part, 6, 0, 0x41c60f);
    m.write<W>(part, 8, 0x474820, 0x41c613);
    for (W offset = 0x20; offset <= 0x28; offset += 2)
        m.write<H>(part, offset, 0, 0x41c61a + (offset - 0x20) * 2);
    m.write<W>(part, 0x2c, m.token(s.coordinates), 0x41c62e);
    const auto polygons = m.token(s.polygons);
    m.write<W>(part, 0x30, m.token(s.vertices), 0x41c637);
    platform->source_scene_resources().bind_source_strip_41c63a(g, descriptor.source_binding().allocation);
    for (const W offset : {0u, 4u, 0x10u, 0x14u, 0x24u, 0x34u})
        m.write<W>(s.vertices, offset, 0, 0x41c640);
    g.bound_index = 0;
    g.texture_names[0] = 0;
    g.texture_names[1] = 0;
    g.texture_names[2] = 0;
    g.normals[0].x = 0;
    g.normals[0].y = 1;
    g.normals[0].z = 0; // normal W and5712EC survive
    m.write<W>(part, 0x34, polygons, 0x41c67e);
    for (W offset = 0; offset < 0x68; offset += 4)
        m.write<W>(descriptor, offset, 0, 0x41c688);
    m.write<H>(descriptor, 0, 0, 0x41c68f);
    m.write<H>(descriptor, 2, 1, 0x41c692);
    for (const W offset : {0xcu, 0x10u, 0x14u, 0x18u})
        m.write<W>(descriptor, offset, 0, 0x41c696);
    m.write<H>(descriptor, 0x20, 0, 0x41c6a2);
    m.write<H>(descriptor, 0x22, 0, 0x41c6a6);
    m.write<W>(descriptor, 0x24, 1, 0x41c6aa);
    m.write<W>(descriptor, 0x28, 0x570ad8, 0x41c6b3);
    m.write<W>(descriptor, 0x2c, m.token(s.polygons), 0x41c6ba);
    m.write<W>(descriptor, 0x30, m.token(s.coordinates), 0x41c6c3);
    const auto indices = m.token(s.indices);
    m.write<W>(descriptor, 0x34, m.token(s.vertices), 0x41c6cc);
    m.write<W>(descriptor, 0x38, indices, 0x41c6cf);
    m.write<W>(descriptor, 0x3c, 0x474820, 0x41c6d2);
    m.write<W>(descriptor, 0x40, 0x5712d8, 0x41c6d9);
    const auto part_token = m.token(part);
    m.write<W>(descriptor, 0x44, part_token, 0x41c6e0);
    m.write<W>(descriptor, 0x48, part_token, 0x41c6e3);
    m.write<W>(descriptor, 0x4c, part_token, 0x41c6e6);
    m.write<W>(part, 0x10, 0, 0x41c6e9);
    m.write<W>(descriptor, 0x50, part_token, 0x41c6ec);
    m.write<W>(part, 0x14, 0, 0x41c6ef);
    m.write<W>(descriptor, 0x54, part_token, 0x41c6f2);
    m.write<W>(part, 0x18, 0, 0x41c6f5);
    m.write<W>(descriptor, 0x58, 0, 0x41c6f8);
    m.write<W>(descriptor, 0x5c, 0, 0x41c6fc);
    m.write<W>(descriptor, 0x60, 0x5712fc, 0x41c700);
}
void expand_source_strip_geometry_41c9b0(const std::weak_ptr<RaceSessionPlatform> &borrow)
{
    const auto platform = require(borrow);
    initialize_source_strip_geometry_41c530(platform);
    const Memory m{*platform};
    auto &g = platform->state().strip;
    const auto descriptor = g.source.descriptor;
    const auto part_token = m.read<W>(descriptor, 0x48, 0x41c9cb);
    m.write<H>(descriptor, 0x20, 0, 0x41c9ce);
    m.write<H>(descriptor, 0x22, 0, 0x41c9d2);
    const auto indices_token = m.read<W>(descriptor, 0x38, 0x41c9d6);
    const auto part = m.pointer(part_token, 0x41c9d9);
    const auto polygon_token = m.read<W>(part, 0x34, 0x41c9d9);
    const auto coordinates_token = m.read<W>(part, 0x2c, 0x41c9dc);
    m.write<H>(part, 0x20, 0, 0x41c9e2);
    const auto polygon = m.pointer(polygon_token, 0x41c9e6);
    const auto initial_first = m.read<H>(polygon, 6, 0x41c9e6);
    m.write<H>(part, 0x22, 0, 0x41c9ea);
    const auto vertices_token = m.read<W>(part, 0x30, 0x41c9ee);
    m.write<B>(polygon, 1, 5, 0x41c9f1);
    // Only the reached writes resolve these payloads, after earlier stores.
    const auto coordinates = m.pointer(coordinates_token, 0x41ca00);
    for (W i = 0; i < 4; ++i)
        m.write<W>(coordinates, i * 16 + 12, 0xffffff, 0x41ca00 + i * 3);
    m.write<B>(polygon, 3, 0x70, 0x41ca19);
    for (W q = 1; q < 128; ++q)
    {
        copy_words(m, coordinates, 0, coordinates, q * 64, 16, 0x41ca56);
        const auto vertices = m.pointer(vertices_token, 0x41ca63);
        copy_words(m, vertices, 0, vertices, q * 64, 16, 0x41ca63);
        const auto first = static_cast<H>(q * 4);
        const W index_offset = W(initial_first) * 2 + q * 8;
        const auto indices = m.pointer(indices_token, 0x41ca71);
        for (unsigned repeat = 0; repeat < 4; ++repeat)
        { // source repeats these same four stores four times
            m.write<H>(indices, index_offset, static_cast<H>(first + 2), 0x41ca71);
            m.write<H>(indices, index_offset + 2, first, 0x41ca78);
            m.write<H>(indices, index_offset + 4, static_cast<H>(first + 1), 0x41ca7c);
            m.write<H>(indices, index_offset + 6, static_cast<H>(first + 3), 0x41ca82);
        }
        const auto live_descriptor = g.source.descriptor;
        const auto normal = m.read<W>(live_descriptor, 0x28, 0x41ca91);
        if (normal != 0x570ad8)
            throw SourceAllocationError(0x41ca96, "strip expansion normals leave actual570AD8 owner");
        g.normals[q].x = g.normals[0].x;
        g.normals[q].y = g.normals[0].y;
        g.normals[q].z = g.normals[0].z;
        g.normals[q].w = g.normals[0].w;
        copy_words(m, polygon, 0, polygon, q * 8, 2, 0x41cab4);
        m.write<H>(polygon, q * 8 + 4, static_cast<H>(q), 0x41cac2);
        m.write<H>(polygon, q * 8 + 6, first, 0x41caca);
        for (W lane = 0; lane < 4; ++lane)
            m.write<H>(coordinates, q * 64 + lane * 16 + 8, static_cast<H>(first + lane), 0x41cad2);
    }
    m.write<H>(part, 0x24, 0, 0x41cb2c);
    m.write<H>(part, 0x26, 0, 0x41cb30);
    m.write<H>(part, 0x28, 0, 0x41cb34);
}
void initialize_source_vehicle_strip_41d180(const std::weak_ptr<RaceSessionPlatform> &borrow)
{
    const auto platform = require(borrow);
    auto &world = platform->state();
    auto &g = world.strip;
    g.field_5712f8 = 1;
    g.field_5712f0 = 0;
    g.field_5712f4 = 1;
    expand_source_strip_geometry_41c9b0(platform);
    for (W i = 0; i < 4; ++i)
    {
        const auto current = world.current_slot; // fresh4D5BB8 on each41D19D
        if (current >= world.vehicles.strips.size())
            throw SourceAllocationError(0x41d1a2, "strip header current vehicle outside source bank");
        auto &h = world.vehicles.strips[current].items[i];
        h.field_00 = 0;
        h.field_04 = 0;
        h.field_08 = 0;
    }
}
bool append_source_vehicle_strip_draw_41d1c0(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    if (!platform.source_construction_only_entry())
        throw SourceAllocationError(0x41d1c0, "strip draw requires actual source platform");
    auto &world = platform.state();
    if (world.impact_particles.rate_selector < 80u)
        return false;
    if (world.strip.field_5712f8 == 0)
        return false;
    const auto descriptor = world.strip.source.descriptor; // 571314 capture, no payload dereference
    auto &pose = world.model_draws.template_pose;
    auto &queue = world.model_draws.background;
    const Memory m{platform};
    allocate_source_outer_model_matrices_44fdb0(pose, world.view_matrices);
    part_matrices_450150(m, descriptor);
    const auto captured = queue.cursor;
    pose.tail[0] = 0;
    if (!captured || *captured >= queue.records.size())
        throw SourceAllocationError(0x42b81f, "reached strip queue store outside owner");
    const auto identity = m.token(descriptor);
    if (identity >= strip_model_identity || identity <= platform.source_scene_resources().models().size())
        throw SourceAllocationError(0x42b81f, "source strip address token collides with model identity domain");
    queue.records[*captured].model_identity = identity;
    // Original REP MOVSD copy is part of the queue payload, not a shadow pose.
    std::memcpy(&queue.records[*captured].pose, &pose, sizeof(pose));
    m.write<H>(descriptor, 2, static_cast<H>(m.read<H>(descriptor, 2, 0x42b7e0) & 0xfff7u), 0x42b7e0);
    if (!queue.cursor)
        throw SourceAllocationError(0x42b835, "source strip queue cursor is NULL");
    queue.cursor = *queue.cursor + 1u; // publication precedes reached terminator store
    if (*queue.cursor >= queue.records.size())
        throw SourceAllocationError(0x42b845, "source strip terminator outside queue owner");
    queue.records[*queue.cursor].model_identity = 0;
    return true;
}
} // namespace sgn::retail
