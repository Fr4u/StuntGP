#include "sgn/retail_world_polygons.hpp"
#include "sgn/detail/world_polygon_clip_kernel.hpp"
#include "sgn/world_indexed_vertex_binding.hpp"
#include "sgn/retail_projection.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/format_require.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_word;
using sgn::format_check::require;
double source_float_product(double left, float right) noexcept
{
    // 429A99/429AA5 and the two linear-loop counterparts multiply an x87
    // register by a FLOAT memory operand. Preserve that operand's signaling
    // NaN classification before a native float-to-double conversion quiets it.
    constexpr std::uint64_t sign = 0x8000000000000000ull, exponent = 0x7ff0000000000000ull,
                            quiet = 0x0008000000000000ull;
    const auto a = std::bit_cast<std::uint64_t>(left);
    const auto b = as_word(right);
    const auto ma = a & ~sign;
    const auto mb = b & 0x7fffffffu;
    const bool an = ma > exponent, bn = mb > 0x7f800000u;
    if (an || bn)
    {
        const std::uint64_t converted =
            (std::uint64_t(b & 0x80000000u) << 32) | exponent | (std::uint64_t(b & 0x7fffffu) << 29);
        auto chosen = an ? a : converted;
        if (an && bn)
        {
            const bool aq = (a & quiet) != 0, bq = (b & 0x400000u) != 0;
            const auto other = converted & ~sign;
            if (aq != bq)
                chosen = aq ? a : converted;
            else
                chosen = ma == other ? std::min(a, converted) : ma > other ? a : converted;
        }
        return std::bit_cast<double>(chosen | quiet);
    }
    if ((ma == exponent && mb == 0) || (ma == 0 && mb == 0x7f800000u))
        return std::bit_cast<double>(sign | exponent | quiet);
    return left * double(right);
}
void project(WorldPolygonVertex &point, const ViewportState &view)
{
    auto &v = point.words;
    const float x = as_float(v[0]), y = as_float(v[1]), depth = as_float(v[7]);
    // 429A40 uses masked x87 exceptions. In particular a finite depth of-1
    // produces infinity/indefinite values, including for vertices not referenced
    // by any surviving packet. Rejecting that input would abort a valid frame.
    const double inverse = 1.0 / (double(depth) + 1.0);
    v[0] = as_word(static_cast<float>(source_float_product(inverse, x) * view.scale_x + view.center_x));
    v[1] = as_word(static_cast<float>(source_float_product(inverse, y) * view.scale_y + view.center_y));
    v[7] = as_word(static_cast<float>(inverse * 32.0));
}
WorldPolygonPacketAudit borrowed_audit(const WorldPolygonPacket &packet, const WorldPolygonClipStorage &storage,
                                       const WorldPolygonClipAudit &audit)
{
    // Diagnostic-only identity resolution. A later list can name the same
    // packet already replaced by an earlier list; its attributes/vertices now
    // borrow clip output, including source-undefined retained bytes.
    WorldPolygonPacketAudit result;
    for (std::size_t i = 0; i < storage.headers.size(); ++i)
        if (packet.header == world_polygon_header_ref(storage.headers[i]))
        {
            result.header = &audit.headers[i];
            break;
        }
    for (std::size_t i = 0; i < storage.attributes.size(); ++i)
        if (packet.attributes.bytes().data() == std::as_bytes(storage.attributes.subspan(i)).data())
        {
            require(packet.attributes.size() <= storage.attributes.size() - i, "aliased clip attribute extent");
            result.attributes = audit.attributes.subspan(i, packet.attributes.size());
            break;
        }
    for (std::size_t i = 0; i < storage.vertices.size(); ++i)
        if (packet.vertices.data() == &storage.vertices[i])
        {
            require(packet.vertices.size() <= storage.vertices.size() - i, "aliased clip vertex extent");
            result.vertices = audit.vertices.subspan(i, packet.vertices.size());
            break;
        }
    return result;
}
} // namespace
void reset_world_polygon_clip_storage(WorldPolygonClipStorage &storage)
{
    const auto header = SourceClipCurrentPointer::base(storage.headers);         // 427240 EAX
    const auto attributes = SourceClipCurrentPointer::base(storage.attributes);  // 427245 ECX
    const auto vertices = SourceClipCurrentPointer::base(storage.vertices);      // 42724B EDX
    storage.header_count = header;                                               // 427251 ->6244DC
    const auto indices = SourceClipCurrentPointer::base(storage.unused_indices); // 427256 EAX
    storage.attribute_count = attributes;                                        // 42725B ->6244D8
    storage.vertex_count = vertices;                                             // 427261 ->6244E4
    storage.unused_index_count = indices;                                        // 427267 ->6244E0
}
void initialize_world_polygon_clip_storage(WorldPolygonClipStorage &storage)
{
    reset_world_polygon_clip_storage(storage);
    for (unsigned i = 0; i < storage.sequential_indices.size(); ++i)
        storage.sequential_indices[i] = static_cast<std::uint16_t>(i);
}
void reject_world_polygon(WorldPolygonPacket &packet) noexcept
{
    packet.header = {};
}
namespace
{
struct NativeClipAccess
{
    static constexpr bool source = false;
    WorldPolygonPacket &packet;
    float near_plane;
    WorldPolygonHeaderRef capture_header() const
    {
        return packet.header;
    }
    float near_distance() const
    {
        return near_plane;
    }
    void validate_count(unsigned count) const
    {
        require(packet.indices.size() >= count, "source polygon input exceeds sixteen scratch vertices");
    }
    std::size_t attribute_size() const
    {
        return packet.attributes.size();
    }
    std::size_t vertex_size() const
    {
        return packet.vertices.size();
    }
    std::uint16_t index(unsigned i) const
    {
        return packet.indices[i];
    }
    WorldPolygonAttribute attribute(unsigned i) const
    {
        require(i < packet.attributes.size(), "source polygon attribute index");
        return packet.attributes.value(i);
    }
    WorldPolygonVertex vertex(unsigned i) const
    {
        require(i < packet.vertices.size(), "source polygon transformed vertex index");
        return packet.vertices[i];
    }
    std::uint32_t mask() const
    {
        return packet.inside_mask;
    }
    void reject() const
    {
        reject_world_polygon(packet);
    }
    detail::ClipOutputPublication publish_output(WorldPolygonClipStorage &storage, unsigned count) const
    {
        const auto header_start = storage.header_count.count_from(storage.headers);
        const auto attribute_start = storage.attribute_count.count_from(storage.attributes);
        const auto vertex_start = storage.vertex_count.count_from(storage.vertices);
        require(header_start < storage.headers.size() && attribute_start <= storage.attributes.size() &&
                    count <= storage.attributes.size() - attribute_start && vertex_start <= storage.vertices.size() &&
                    count <= storage.vertices.size() - vertex_start,
                "source polygon clip output capacity");
        const auto attributes = storage.attribute_count;
        const auto header = storage.header_count;
        packet.attributes = storage.attributes.subspan(attributes.count_from(storage.attributes));
        const auto vertices = storage.vertex_count;
        packet.header = world_polygon_header_ref(storage.headers[header_start]);
        packet.indices = storage.sequential_indices;
        packet.vertices = storage.vertices.subspan(vertices.count_from(storage.vertices));
        return {header, attribute_start, vertex_start};
    }
};
} // namespace
void clip_world_polygon(WorldPolygonPacket &packet, WorldPolygonClipStorage &storage, float near_distance,
                        const WorldPolygonPacketAudit *input_audit, WorldPolygonClipAudit *output_audit)
{
    NativeClipAccess access{packet, near_distance};
    detail::clip_polygon_kernel(access, storage, input_audit, output_audit);
}

void clip_world_polygon_list(std::span<WorldPolygonPacket> packets, WorldPolygonClipStorage &storage,
                             float near_distance, WorldPolygonClipAudit *output_audit)
{
    if (output_audit)
        require(output_audit->headers.size() == storage.headers.size() &&
                    output_audit->attributes.size() == storage.attributes.size() &&
                    output_audit->vertices.size() == storage.vertices.size(),
                "clip list audit extent");
    for (auto &packet : packets)
        if (packet.inside_mask != 63)
        {
            if (output_audit)
            {
                const auto input_audit = borrowed_audit(packet, storage, *output_audit);
                clip_world_polygon(packet, storage, near_distance, &input_audit, output_audit);
            }
            else
                clip_world_polygon(packet, storage, near_distance);
        }
}
void clip_world_polygon_lists(std::array<std::span<WorldPolygonPacket>, 7> lists, WorldPolygonClipStorage &storage,
                              float near_distance, float &far_distance, WorldPolygonClipAudit *output_audit)
{
    reset_world_polygon_clip_storage(storage);
    if (!lists[0].empty())
    {
        far_distance = std::bit_cast<float>(0x4a989680u);
        clip_world_polygon_list(lists[0], storage, near_distance, output_audit);
    }
    far_distance = std::bit_cast<float>(0x48f42400u);
    if (!lists[1].empty())
        clip_world_polygon_list(lists[1], storage, near_distance, output_audit);
    if (!lists[2].empty())
        clip_world_polygon_list(lists[2], storage, near_distance, output_audit);
    far_distance = std::bit_cast<float>(0x48f42400u);
    for (unsigned i = 3; i < lists.size(); ++i)
        if (!lists[i].empty())
            clip_world_polygon_list(lists[i], storage, near_distance, output_audit);
}
void project_world_polygon_vertex(WorldPolygonVertex &point, const ViewportState &captured_view)
{
    project(point, captured_view);
}
void project_world_polygon_vertices(std::span<WorldPolygonVertex *const> indexed, std::span<WorldPolygonVertex> clipped,
                                    std::span<WorldPolygonVertex> remaining, const ViewportState &view)
{
    for (float value : {view.center_x, view.center_y, view.scale_x, view.scale_y})
        require(std::isfinite(value), "nonfinite final polygon viewport");
    for (auto *point : indexed)
    {
        require(point != nullptr, "null indexed polygon vertex");
        project(*point, view);
    }
    for (auto &point : clipped)
        project(point, view);
    for (auto &point : remaining)
        project(point, view);
}

void project_world_polygon_indexed_cursor(const WorldIndexedVertexCursor &indexed,
                                          std::span<WorldPolygonVertex> clipped,
                                          std::span<WorldPolygonVertex> remaining, const ViewportState &view)
{
    for (float value : {view.center_x, view.center_y, view.scale_x, view.scale_y})
        require(std::isfinite(value), "nonfinite final polygon viewport");
    const auto count = indexed.count();
    for (std::uint32_t i = 0; i < count; ++i)
    {
        auto *point = indexed.projection_vertex(i);
        require(point != nullptr, "null indexed polygon vertex");
        project(*point, view); // same429A40 arithmetic, same order, repeated identities are not deduplicated
    }
    for (auto &point : clipped)
        project(point, view);
    for (auto &point : remaining)
        project(point, view);
}
} // namespace sgn::retail
