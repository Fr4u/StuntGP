// NEW HOST rendering (gpu_world R19). Track-base (42AC70) inside-list polygons
// on the GPU. The CPU keeps every decision (42ABB0 traversal, 42A090 backface
// and emission order, 433F40 state sequence and batching); this file only
// supplies the per-level geometry, the per-view constants and the draw payload.
// See source-work/subagents/gpu-world-r19/DESIGN.md for the stage map.
#include "sgn/gpu_world.hpp"
#include "sgn/detail/gpu_world_ticket.hpp"
#include "sgn/assets.hpp"
#include "sgn/format_require.hpp"
#include "sgn/retail_world_polygon_frame.hpp"
#include "sgn/retail_projection.hpp"
#include "sgn/retail_world_polygons.hpp"
#include "sgn/detail/world_vertex_kernels.hpp"
#include "sgn/source_allocation_bank.hpp"
#include <bit>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>

namespace sgn::gpu_world
{
namespace
{
using W = std::uint32_t;
using format_check::require;

// ---- Loaded PMD records (plain typed views over the loaded bytes) ----------
// Part record fields read by 429730/42A090/433F40 for the track base part.
struct PmdPartCounts
{
    std::uint16_t coordinates; // +0x26 (42A39E coordinate count)
    std::uint16_t vertices;    // +0x28 (42AD09 transformed tail)
    W coordinate_offset;       // +0x2C (42A0D1 attributes)
    W vertex_offset;           // +0x30 (4297A3 vertices)
};
struct PmdCoordinate
{                                 // 16 bytes: 4335E8..433620 / 433B07..433B50
    float u, v;                   // +0,+4
    std::uint16_t vertex, normal; // +8,+A
    W color;                      // +C
};
static_assert(sizeof(PmdCoordinate) == 16);
struct PmdVertex
{
    float x, y, z;
    W w;
}; // 16 bytes; 42979A reads XYZ only
static_assert(sizeof(PmdVertex) == 16);

template <class T> T load(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    require(offset <= bytes.size() && sizeof(T) <= bytes.size() - offset, "gpu_world PMD field outside loaded bytes");
    T value;
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}
PmdPartCounts part_counts(std::span<const std::uint8_t> bytes, W part)
{
    return {load<std::uint16_t>(bytes, std::size_t(part) + 0x26), load<std::uint16_t>(bytes, std::size_t(part) + 0x28),
            load<W>(bytes, std::size_t(part) + 0x2c), load<W>(bytes, std::size_t(part) + 0x30)};
}
bool env_flag(const char *name) noexcept
{
    char *value = nullptr;
    std::size_t length = 0;
    bool set = false;
    if (_dupenv_s(&value, &length, name) == 0 && value)
    {
        set = std::strcmp(value, "1") == 0;
        std::free(value);
    }
    return set;
}
// Parse once per level. Returns null when the part is outside the qualified
// domain (NULL arrays, zero vertices, a coordinate naming a vertex >= +28,
// arrays outside the allocation): that level then stays on the CPU path.
std::shared_ptr<detail::StaticMesh> parse_mesh(std::span<const std::uint8_t> bytes, const PmdPartCounts &counts)
{
    if (counts.vertices == 0 || counts.vertex_offset == 0 || counts.coordinate_offset == 0)
        return nullptr;
    const auto vertex_bytes = std::size_t(counts.vertices) * sizeof(PmdVertex);
    const auto coordinate_bytes = std::size_t(counts.coordinates) * sizeof(PmdCoordinate);
    if (counts.vertex_offset > bytes.size() || vertex_bytes > bytes.size() - counts.vertex_offset)
        return nullptr;
    if (counts.coordinate_offset > bytes.size() || coordinate_bytes > bytes.size() - counts.coordinate_offset)
        return nullptr;
    auto mesh = std::make_shared<detail::StaticMesh>();
    mesh->positions.resize(counts.vertices);
    for (std::size_t i = 0; i < counts.vertices; ++i)
    {
        PmdVertex v;
        std::memcpy(&v, bytes.data() + counts.vertex_offset + i * sizeof(PmdVertex), sizeof v);
        mesh->positions[i] = {v.x, v.y, v.z, 0.0f};
    }
    mesh->coordinates.resize(counts.coordinates);
    for (std::size_t i = 0; i < counts.coordinates; ++i)
    {
        PmdCoordinate c;
        std::memcpy(&c, bytes.data() + counts.coordinate_offset + i * sizeof(PmdCoordinate), sizeof c);
        if (c.vertex >= counts.vertices)
            return nullptr;
        mesh->coordinates[i] = {std::bit_cast<W>(c.u), std::bit_cast<W>(c.v), c.vertex, c.color};
    }
    return mesh;
}
bool same_content(const detail::StaticMesh &a, const detail::StaticMesh &b)
{
    return a.coordinates == b.coordinates && a.positions.size() == b.positions.size() &&
           std::memcmp(a.positions.data(), b.positions.data(), a.positions.size() * sizeof(a.positions[0])) == 0;
}

// Per-level cache. The base part (descriptor+44) is never stored to after
// load: 426ED0 animates only parts +48+64*k, k>=1 (0x426EEB/0x426F04), and
// lighting 42A320/42A570 runs only for 42ADD0 parts (ordinal>=1). The key
// includes the allocation identity (bank generation), so a level reload or
// release always reparses. SGN_GPU_WORLD_VERIFY=1 reparses every view.
struct MeshCache
{
    retail::SourceAllocationRef allocation;
    W part{};
    PmdPartCounts counts{};
    bool parsed{};
    std::shared_ptr<const detail::StaticMesh> mesh;
};
std::uint64_t next_serial() noexcept
{
    static std::uint64_t serial = 0;
    return ++serial;
}
std::shared_ptr<const detail::StaticMesh> track_mesh(retail::SourceAllocationBank &bank,
                                                     const retail::SourcePmdLocation &part)
{
    static MeshCache cache;
    const auto bytes = std::span<const std::uint8_t>(bank.bytes(part.allocation));
    const auto counts = part_counts(bytes, part.offset);
    const bool same_key = cache.parsed && cache.allocation == part.allocation && cache.part == part.offset &&
                          std::memcmp(&cache.counts, &counts, sizeof counts) == 0;
    static const bool verify = env_flag("SGN_GPU_WORLD_VERIFY");
    if (same_key && !verify)
        return cache.mesh;
    auto parsed = parse_mesh(bytes, counts);
    if (same_key && verify)
    {
        require(bool(parsed) == bool(cache.mesh) && (!parsed || same_content(*parsed, *cache.mesh)),
                "gpu_world verify: track base PMD geometry changed after its GPU upload");
        return cache.mesh;
    }
    if (parsed)
        parsed->serial = next_serial();
    cache = {part.allocation, part.offset, counts, true, std::move(parsed)};
    return cache.mesh;
}

bool overlaps(std::uint64_t a0, std::uint64_t a1, std::uint64_t b0, std::uint64_t b1) noexcept
{
    return a0 < b1 && b0 < a1;
}
// 426ED0 walks parts +48+64*k for 1<=k<count of the incoming descriptor
// (0x426EE7..0x426FBA). A base part equal to one of them would be animated.
bool base_part_is_animated(std::span<const std::uint8_t> bytes, W descriptor, W part)
{
    const auto count = load<W>(bytes, std::size_t(descriptor) + 0x24);
    const auto list = load<W>(bytes, std::size_t(descriptor) + 0x48);
    if (list == 0 || count <= 1)
        return false;
    if (part < list || (part - list) % 64u != 0)
        return false;
    const auto index = (part - list) / 64u;
    return index >= 1 && index < count;
}
} // namespace

namespace detail
{
Ticket &ticket() noexcept
{
    static Ticket value;
    return value;
}
} // namespace detail
bool begin_track_frame_42acf2(retail::SourceAllocationBank &bank, retail::WorldPolygonFrame &frame,
                              const retail::ProjectionParameters &projection)
{
    auto &t = detail::ticket();
    t = detail::Ticket{};
    if (requested_mode() != Mode::hybrid_exact || !backend_available())
        return false;
    if (!frame.active_track.source_bound() || !frame.active_track_base_part.source_bound() ||
        frame.active_track_base_part.is_null())
        return false;
    const auto &vertices = frame.transformed.allocation;
    if (!vertices.source_bound())
        return false;
    const auto &binding = vertices.source_binding();
    if (binding.bank != &bank)
        return false;
    auto platform = binding.platform.lock();
    if (!platform)
        return false;
    // 61C530 resolved exactly as 429EB9/42A0A1 emission resolves it (relocated part view).
    const auto part = frame.active_track_base_part.source_part().location();
    const auto descriptor = frame.active_track.source_descriptor().location(); // 61C52C
    const auto mesh = track_mesh(bank, part);
    if (!mesh)
        return false;
    if (descriptor.allocation == part.allocation &&
        base_part_is_animated(std::span<const std::uint8_t>(bank.bytes(part.allocation)), descriptor.offset,
                              part.offset))
        return false;
    const auto region_start = frame.transformed.count; // 61C510 at 42ACF2
    const auto slots = vertices.view().size();
    const auto n = mesh->positions.size();
    if (region_start > slots || n > slots - region_start)
        return false;
    // 427270 aliases the clip vertices into 61C50C (at +0x60000). 4277D0 runs
    // after emission and could overwrite reference slots before 433F40; such a
    // level stays on the CPU path instead of drawing different data.
    const std::uint64_t r0 = std::uint64_t(binding.byte_offset) + std::uint64_t(region_start) * 32u;
    const std::uint64_t r1 = r0 + std::uint64_t(n) * 32u;
    if (frame.clip.vertices.source_bound())
    {
        const auto &clip = frame.clip.vertices.source_binding();
        if (clip.allocation == binding.allocation)
        {
            const std::uint64_t c0 = clip.byte_offset;
            const std::uint64_t c1 =
                clip.byte_count == std::numeric_limits<W>::max() ? clip.bytes : c0 + clip.byte_count;
            if (overlaps(r0, r1, c0, c1))
                return false;
        }
    }
    if (r0 > std::numeric_limits<W>::max())
        return false;
    const auto counts = part_counts(std::span<const std::uint8_t>(bank.bytes(part.allocation)), part.offset);
    // Same (allocation, offset) pairs that 42A1AC/42A18D and 42A0D1 encode.
    t.vertices_token = bank.encode_address(binding.allocation, static_cast<W>(r0));
    t.coordinates_token = bank.encode_address(part.allocation, counts.coordinate_offset);
    t.thread = std::this_thread::get_id();
    t.platform = platform;
    t.frame = &frame;
    t.stamp = frame.frame_stamp;
    t.region_start = region_start;
    t.mesh = mesh;
    for (std::size_t i = 0; i < 16; ++i)
        t.pending.constants[i] = std::bit_cast<W>(frame.model_view.model_view[i]);
    t.pending.constants[16] = std::bit_cast<W>(projection.x);
    t.pending.constants[17] = std::bit_cast<W>(projection.y);
    t.pending.constants[18] = std::bit_cast<W>(projection.z);
    t.pending.constants[19] = std::bit_cast<W>(projection.offset_z);
    t.active = true;
    return true;
}

std::array<std::uint32_t, 3> reference_project(const std::array<std::uint32_t, 16> &model_view,
                                               const std::array<std::uint32_t, 2> &projection,
                                               const std::array<std::uint32_t, 4> &viewport,
                                               const std::array<float, 4> &position) noexcept
{
    // The CPU path itself: perf_world's 429730 kernel, then 429A40's function.
    std::array<float, 16> m{};
    for (std::size_t i = 0; i < 16; ++i)
        m[i] = std::bit_cast<float>(model_view[i]);
    const retail::detail::WorldVertexProjection p{std::bit_cast<float>(projection[0]),
                                                  std::bit_cast<float>(projection[1]), 0.0f, 0.0f, 0.0f};
    retail::WorldPolygonVertex vertex{};
    retail::detail::world_track_vertex_inside_42979a(m.data(), position.data(), p, vertex.words.data());
    retail::ViewportState view{};
    view.scale_x = std::bit_cast<float>(viewport[0]);
    view.scale_y = std::bit_cast<float>(viewport[1]);
    view.center_x = std::bit_cast<float>(viewport[2]);
    view.center_y = std::bit_cast<float>(viewport[3]);
    retail::project_world_polygon_vertex(vertex, view);
    return {vertex.words[0], vertex.words[1], vertex.words[7]};
}
} // namespace sgn::gpu_world