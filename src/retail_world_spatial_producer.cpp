#include "sgn/retail_world_spatial_producer.hpp"
#include "sgn/detail/world_polygon_emission.hpp"

namespace sgn::retail
{
namespace
{
float f32(double v) noexcept
{
    return static_cast<float>(v);
}
void store(std::uint32_t &destination, float value) noexcept
{
    destination = std::bit_cast<std::uint32_t>(value);
}
template <class F> void sentinel_list(std::span<const std::uint16_t> list, F action)
{
    for (const auto index : list)
    {
        if (index == 0xffff)
            return;
        action(index);
    }
    throw FormatError("unterminated source world spatial list");
}
void append_batch(const ModelSpatialData &spatial, std::uint16_t polygons, std::uint16_t vertices,
                  WorldSpatialBatchList &list)
{
    if (list.records.source_bound())
        throw FormatError("native spatial producer cannot publish source address tokens");
    if (list.count >= list.records.size())
        throw FormatError("source world spatial batch capacity");
    if (polygons >= spatial.polygon_indices.size() || vertices >= spatial.vertex_indices.size())
        throw FormatError("source spatial list offset");
    list.records[list.count++] = {std::span(spatial.polygon_indices).subspan(polygons),
                                  std::span(spatial.vertex_indices).subspan(vertices)};
}
template <bool clipped>
void transform_batches(std::span<const WorldSpatialBatch> batches, std::span<const VehicleVector> source,
                       std::span<WorldPolygonVertex> vertices, WorldIndexedVertexCursor &indexed, std::uint32_t frame,
                       const CameraMatrix &matrix, const ProjectionParameters &p)
{
    indexed.require_owner_thread();   // host admission before reading/writing geometry
    auto current = indexed.current(); // 42975B /4298AC, independent of61C5EC
    for (const auto &batch : batches)
        sentinel_list(batch.vertices, [&](std::uint16_t index) {
            if (index >= vertices.size())
                throw FormatError("source indexed transformed vertex");
            auto &words = vertices[index].words;
            if ((words[4] >> 16) == frame)
                return;
            if (index >= source.size())
                throw FormatError("source indexed vertex capacity");
            current.append(&vertices[index]);
            const auto &v = source[index];
            // Both indexed source paths use XYZ order for X/Y, and YZX for Z.
            const auto component = [&](unsigned c) {
                return ((double(matrix[c]) * v.x + double(matrix[c + 4]) * v.y) + double(matrix[c + 8]) * v.z) +
                       matrix[c + 12];
            };
            const double camera_z =
                ((double(matrix[6]) * v.y + double(matrix[10]) * v.z) + double(matrix[2]) * v.x) + matrix[14];
            const double x = component(0);
            const float y = f32(component(1));
            const double px = double(p.x) * (clipped ? double(f32(x)) : x), pz = double(p.z) * camera_z + p.offset_z;
            const float py = f32(double(p.y) * y);
            store(words[0], f32(px));
            store(words[1], py);
            store(words[2], f32(pz));
            store(words[7], f32(camera_z));
            words[4] = (words[4] & 0xffffu) | ((frame & 0xffffu) << 16);
            if constexpr (clipped)
                words[4] = (words[4] & 0xffff0000u) | projection_mask(px, py, pz, camera_z, p.clip_near);
        });
    indexed.publish_current(std::move(current)); // 429868 /429A30, after the complete traversal
}
template <bool clipped>
void emit_batches(std::span<const WorldSpatialBatch> batches, const WorldPolygonPartSource &source,
                  std::span<WorldPolygonVertex> vertices, const VehicleVector &camera, std::uint32_t &batch_count,
                  WorldPolygonProducerList &opaque, WorldPolygonProducerList &translucent,
                  WorldPacketEmissionObserver *observer)
{
    detail::PacketEmissionCursors cursors(opaque, translucent, observer);
    constexpr auto path =
        clipped ? detail::PacketEmissionPath::spatial_clipped : detail::PacketEmissionPath::spatial_inside;
    batch_count += static_cast<std::uint32_t>(batches.size());
    for (const auto &batch : batches)
        sentinel_list(batch.polygons, [&](std::uint16_t index) {
            if (index >= source.headers.size())
                throw FormatError("source spatial polygon index");
            detail::emit_packet_polygon(source, index, vertices, camera, cursors, path);
        });
    cursors.publish(path, batches.empty());
}
} // namespace
std::uint32_t world_spatial_point_mask_42aa9b(Vec3 point, Vec3 camera, std::span<const CameraVector, 4> planes)
{
    const double x = double(point.x) - camera.x, y = double(point.y) - camera.y, z = double(point.z) - camera.z;
    const auto xyz = [&](const CameraVector &p) {
        return std::bit_cast<std::uint32_t>(f32((x * p.x + y * p.y) + z * p.z));
    };
    const auto yzx = [&](const CameraVector &p) {
        return std::bit_cast<std::uint32_t>(f32((y * p.y + z * p.z) + x * p.x));
    };
    std::uint32_t mask = (xyz(planes[0]) | 0x60000000u) >> 1;
    mask = (mask | (yzx(planes[1]) & 0xbfffffffu)) >> 1;
    mask = (mask | (xyz(planes[2]) & 0x9fffffffu)) >> 1;
    mask = (mask | (yzx(planes[3]) & 0x8fffffffu)) >> 26;
    return mask;
}
std::uint32_t classify_world_spatial_bounds(std::span<const VehicleVector, 8> bounds, Vec3 camera,
                                            std::span<const CameraVector, 4> planes)
{
    std::uint32_t common = 63, any = 0;
    for (const auto &point : bounds)
    {
        const auto mask = world_spatial_point_mask_42aa9b({point.x, point.y, point.z}, camera, planes);
        if (mask == 63 && common != mask)
            return 2;
        common &= mask;
        any |= mask;
    }
    return common == 63 ? 0 : any == 63 ? 2 : 1;
}
void gather_world_spatial_batches(const ModelSpatialData &spatial, std::uint32_t root, Vec3 camera,
                                  std::span<const CameraVector, 4> planes, WorldSpatialBatchList &clipped,
                                  WorldSpatialBatchList &inside)
{
    if (root >= spatial.nodes.size())
        throw FormatError("source spatial root outside node owner");
    struct Visit
    {
        std::uint32_t node;
        unsigned child;
    };
    std::vector<Visit> stack{{root, 0}};
    std::vector<bool> ancestors(spatial.nodes.size());
    ancestors[root] = true;
    while (!stack.empty())
    {
        auto &visit = stack.back();
        if (visit.child == 8)
        {
            ancestors[visit.node] = false;
            stack.pop_back();
            continue;
        }
        const auto &node = spatial.nodes[visit.node];
        const auto offset = visit.child++ * 4;
        const auto child = node[offset];
        if (child == 0xffff)
            continue;
        const auto first = node[offset + 3];
        if (first > spatial.bounds.size() || 8 > spatial.bounds.size() - first)
            throw FormatError("source spatial bound range");
        const auto classification =
            classify_world_spatial_bounds(std::span(spatial.bounds).subspan(first, 8).first<8>(), camera, planes);
        if (classification == 1)
            continue;
        if (classification == 0)
        {
            append_batch(spatial, node[offset + 1], node[offset + 2], inside);
            continue;
        }
        if (child == 0)
        {
            append_batch(spatial, node[offset + 1], node[offset + 2], clipped);
            continue;
        }
        if (child >= spatial.nodes.size() || ancestors[child])
            throw FormatError("cyclic or invalid source spatial child");
        ancestors[child] = true;
        stack.push_back({child, 0});
    }
}
void transform_world_spatial_vertices_clipped(std::span<const WorldSpatialBatch> batches,
                                              std::span<const VehicleVector> source,
                                              std::span<WorldPolygonVertex> vertices, WorldIndexedVertexCursor &indexed,
                                              std::uint32_t frame, const CameraMatrix &matrix,
                                              const ProjectionParameters &p)
{
    transform_batches<true>(batches, source, vertices, indexed, frame, matrix, p);
}
void transform_world_spatial_vertices_inside(std::span<const WorldSpatialBatch> batches,
                                             std::span<const VehicleVector> source,
                                             std::span<WorldPolygonVertex> vertices, WorldIndexedVertexCursor &indexed,
                                             std::uint32_t frame, const CameraMatrix &matrix,
                                             const ProjectionParameters &p)
{
    transform_batches<false>(batches, source, vertices, indexed, frame, matrix, p);
}
void emit_world_spatial_polygons_clipped(std::span<const WorldSpatialBatch> batches,
                                         const WorldPolygonPartSource &source, std::span<WorldPolygonVertex> vertices,
                                         const VehicleVector &camera, std::uint32_t &count,
                                         WorldPolygonProducerList &opaque, WorldPolygonProducerList &translucent,
                                         WorldPacketEmissionObserver *observer)
{
    emit_batches<true>(batches, source, vertices, camera, count, opaque, translucent, observer);
}
void emit_world_spatial_polygons_inside(std::span<const WorldSpatialBatch> batches,
                                        const WorldPolygonPartSource &source, std::span<WorldPolygonVertex> vertices,
                                        const VehicleVector &camera, std::uint32_t &count,
                                        WorldPolygonProducerList &opaque, WorldPolygonProducerList &translucent,
                                        WorldPacketEmissionObserver *observer)
{
    emit_batches<false>(batches, source, vertices, camera, count, opaque, translucent, observer);
}
void emit_world_spatial_polygons(std::span<const WorldSpatialBatch> clipped, std::span<const WorldSpatialBatch> inside,
                                 const WorldPolygonPartSource &source, std::span<WorldPolygonVertex> vertices,
                                 const VehicleVector &camera, std::uint32_t &count,
                                 WorldPolygonProducerList &clipped_opaque,
                                 WorldPolygonProducerList &clipped_translucent, WorldPolygonProducerList &inside_opaque,
                                 WorldPolygonProducerList &inside_translucent)
{
    emit_world_spatial_polygons_clipped(clipped, source, vertices, camera, count, clipped_opaque, clipped_translucent);
    emit_world_spatial_polygons_inside(inside, source, vertices, camera, count, inside_opaque, inside_translucent);
}
} // namespace sgn::retail
