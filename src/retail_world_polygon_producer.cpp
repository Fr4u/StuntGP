#include "sgn/retail_world_polygon_producer.hpp"
#include "sgn/detail/world_polygon_emission.hpp"

namespace sgn::retail
{
namespace
{
std::uint16_t halfword(const WorldPolygonHeader &header, std::size_t offset) noexcept
{
    return static_cast<std::uint16_t>(std::uint16_t(header[offset]) | (std::uint16_t(header[offset + 1]) << 8));
}
void store_float(std::uint32_t &output, float value) noexcept
{
    output = std::bit_cast<std::uint32_t>(value);
}
bool forced_translucency(const WorldPolygonPartSource &source) noexcept
{
    return (source.model_flags_02 & 8u) != 0 && (source.part_flags_02 & 0x80u) != 0 && (source.part_flags_02 >> 8) != 0;
}
std::uint16_t corner_vertex(const WorldPolygonPartSource &source, std::size_t corner)
{
    if (corner >= source.indices.size())
        throw FormatError("source world polygon corner index");
    return source.attributes.value(source.indices[corner]).vertex;
}
bool facing(const WorldPolygonPartSource &source, const WorldPolygonHeader &header, std::uint16_t vertex,
            const VehicleVector &camera)
{
    if (vertex >= source.vertices.size())
        throw FormatError("source world polygon facing vertex");
    // Both source functions load the authored vertex before testing two-sided.
    const auto &point = source.vertices[vertex];
    const double dx = double(point.x) - camera.x, dy = double(point.y) - camera.y, dz = double(point.z) - camera.z;
    if ((header[3] & 0x80u) != 0)
        return true;
    const auto index = halfword(header, 4);
    if (index >= source.normals.size())
        throw FormatError("source world polygon facing normal");
    const auto &normal = source.normals[index];
    const double dot = (dz * normal.z + dy * normal.y) + dx * normal.x;
    // FCOMP/FNSTSW tests C0, also set for unordered operands.
    return !(dot >= 0.0);
}
} // namespace
namespace detail
{
void PacketEmissionCursors::publish(PacketEmissionPath path, bool empty)
{
    std::uint32_t first{}, second{};
    switch (path)
    {
    case PacketEmissionPath::part_clipped:
        first = empty ? 0x429d3a : 0x429d2e;
        second = empty ? 0x429d3c : 0x429d30;
        break;
    case PacketEmissionPath::part_inside:
        first = empty ? 0x429e88 : 0x429e7a;
        second = empty ? 0x429e8a : 0x429e7c;
        break;
    case PacketEmissionPath::spatial_clipped:
        output_alpha.publish_current(alpha);
        observe(0x42a080, WorldPacketWriteKind::alpha_current, true, alpha);
        output_opaque.publish_current(opaque);
        observe(0x42a083, WorldPacketWriteKind::opaque_current, false, opaque);
        return;
    case PacketEmissionPath::spatial_inside:
        first = empty ? 0x42a20d : 0x42a1ff;
        second = empty ? 0x42a212 : 0x42a201;
        break;
    }
    output_opaque.publish_current(opaque);
    observe(first, WorldPacketWriteKind::opaque_current, false, opaque);
    output_alpha.publish_current(alpha);
    observe(second, WorldPacketWriteKind::alpha_current, true, alpha);
}
void emit_packet_polygon(const WorldPolygonPartSource &source, std::size_t i, std::span<WorldPolygonVertex> vertices,
                         const VehicleVector &camera, PacketEmissionCursors &cursors, PacketEmissionPath path)
{
    const bool clipped = path == PacketEmissionPath::part_clipped || path == PacketEmissionPath::spatial_clipped;
    const bool part = path == PacketEmissionPath::part_clipped || path == PacketEmissionPath::part_inside;
    const auto reference = source.headers.at(i);
    const auto header = reference.value();
    const auto first = halfword(header, 6);
    std::uint32_t common = 0x3f;
    if (clipped)
    {
        const auto count = header[0];
        if (count == 0)
            return;
        std::uint16_t last{};
        std::uint32_t any{};
        for (unsigned corner = 0; corner < count; ++corner)
        {
            last = corner_vertex(source, std::size_t(first) + corner);
            if (last >= vertices.size())
                throw FormatError("source world polygon transformed vertex");
            const auto mask = vertices[last].words[4] & 0xffffu;
            any |= mask;
            common &= mask;
        }
        if (any != 0x3f || !facing(source, header, last, camera))
            return;
    }
    else if (!facing(source, header, corner_vertex(source, first), camera))
        return;
    const bool alpha = (part && forced_translucency(source)) || (header[3] & 0x40u) != 0;
    auto &current = alpha ? cursors.alpha : cursors.opaque;
    // Source word PCs in header/indices/vertices/attributes/mask order.
    // 429D50 alone writes indices before header. Native views remain typed.
    std::array<std::uint32_t, 5> pc{};
    switch (path)
    {
    case PacketEmissionPath::part_clipped:
        pc = alpha ? std::array<std::uint32_t, 5>{0x429cf0, 0x429cf8, 0x429cfe, 0x429d01, 0x429d04}
                   : std::array<std::uint32_t, 5>{0x429cc8, 0x429cd0, 0x429cd6, 0x429cd9, 0x429cdc};
        break;
    case PacketEmissionPath::part_inside:
        pc = alpha ? std::array<std::uint32_t, 5>{0x429e5c, 0x429e53, 0x429e5e, 0x429e61, 0}
                   : std::array<std::uint32_t, 5>{0x429e43, 0x429e3a, 0x429e45, 0x429e48, 0};
        break;
    case PacketEmissionPath::spatial_clipped:
        pc = alpha ? std::array<std::uint32_t, 5>{0x42a000, 0x42a005, 0x42a00e, 0x42a014, 0x42a017}
                   : std::array<std::uint32_t, 5>{0x42a025, 0x42a02a, 0x42a033, 0x42a039, 0x42a03c};
        break;
    case PacketEmissionPath::spatial_inside:
        pc = alpha ? std::array<std::uint32_t, 5>{0x42a18b, 0x42a193, 0x42a196, 0x42a199, 0}
                   : std::array<std::uint32_t, 5>{0x42a1b1, 0x42a1b3, 0x42a1b6, 0x42a1b9, 0};
        break;
    }
    const auto indices = source.indices.subspan(first);
    const auto header_store = [&] {
        current.record().header = reference;
        cursors.observe(pc[0], WorldPacketWriteKind::header, alpha, current);
    };
    const auto indices_store = [&] {
        current.record().indices = indices;
        cursors.observe(pc[1], WorldPacketWriteKind::indices, alpha, current);
    };
    if (path == PacketEmissionPath::part_inside)
    {
        indices_store();
        header_store();
    }
    else
    {
        header_store();
        indices_store();
    }
    current.record().vertices = vertices;
    cursors.observe(pc[2], WorldPacketWriteKind::vertices, alpha, current);
    current.record().attributes = source.attributes;
    cursors.observe(pc[3], WorldPacketWriteKind::attributes, alpha, current);
    if (clipped)
    {
        current.record().inside_mask = common;
        cursors.observe(pc[4], WorldPacketWriteKind::inside_mask, alpha, current);
    }
    current.advance(); // Local register; global current remains unchanged here.
}
} // namespace detail
void transform_world_vertices_clipped(std::span<const VehicleVector> source, std::span<WorldPolygonVertex> output,
                                      const std::array<float, 16> &matrix, const ProjectionParameters &parameters)
{
    if (source.size() > output.size())
        throw FormatError("source clipped vertex capacity");
    for (std::size_t i = 0; i < source.size(); ++i)
    {
        const auto &vertex = source[i];
        const auto point = project_retail_vertex({vertex.x, vertex.y, vertex.z}, matrix, parameters);
        auto &words = output[i].words;
        store_float(words[0], point.x);
        store_float(words[1], point.y);
        store_float(words[2], point.z);
        words[4] = (words[4] & 0xffff0000u) | point.inside;
        store_float(words[7], point.camera_z);
    }
}
void transform_world_vertices_unclipped(std::span<const VehicleVector> source, std::span<WorldPolygonVertex> output,
                                        const std::array<float, 16> &matrix, const ProjectionParameters &parameters)
{
    if (source.size() > output.size())
        throw FormatError("source unclipped vertex capacity");
    for (std::size_t i = 0; i < source.size(); ++i)
    {
        const auto &vertex = source[i];
        const auto component = [&](unsigned c) {
            return ((double(matrix[c + 4]) * vertex.y + double(matrix[c + 8]) * vertex.z) +
                    double(matrix[c]) * vertex.x) +
                   matrix[c + 12];
        };
        const double x = component(0);
        const float y = static_cast<float>(component(1)), z = static_cast<float>(component(2));
        auto &words = output[i].words;
        store_float(words[0], static_cast<float>(double(parameters.x) * x));
        store_float(words[1], static_cast<float>(double(parameters.y) * y));
        store_float(words[2], static_cast<float>(double(parameters.z) * z + parameters.offset_z));
        store_float(words[7], z);
    }
}
void emit_world_polygons_clipped(const WorldPolygonPartSource &source, std::span<WorldPolygonVertex> vertices,
                                 const VehicleVector &camera, WorldPolygonProducerList &opaque,
                                 WorldPolygonProducerList &translucent, WorldPacketEmissionObserver *observer)
{
    detail::PacketEmissionCursors cursors(opaque, translucent, observer);
    for (std::size_t i = 0; i < source.headers.size(); ++i)
        detail::emit_packet_polygon(source, i, vertices, camera, cursors, detail::PacketEmissionPath::part_clipped);
    cursors.publish(detail::PacketEmissionPath::part_clipped, source.headers.empty());
}
void emit_world_polygons_unclipped(const WorldPolygonPartSource &source, std::span<WorldPolygonVertex> vertices,
                                   const VehicleVector &camera, WorldPolygonProducerList &opaque,
                                   WorldPolygonProducerList &translucent, WorldPacketEmissionObserver *observer)
{
    detail::PacketEmissionCursors cursors(opaque, translucent, observer);
    for (std::size_t i = 0; i < source.headers.size(); ++i)
        detail::emit_packet_polygon(source, i, vertices, camera, cursors, detail::PacketEmissionPath::part_inside);
    cursors.publish(detail::PacketEmissionPath::part_inside, source.headers.empty());
}
} // namespace sgn::retail
