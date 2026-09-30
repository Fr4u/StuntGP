#pragma once
#include "sgn/model_resources.hpp"
#include "sgn/retail_projection.hpp"
#include "sgn/retail_world_polygons.hpp"
#include "sgn/world_polygon_packet_binding.hpp"

namespace sgn::retail
{
// Borrowed fields of one relocated descriptor/part. Mutable coordinates remain
// owned by ModelResource; the attribute view reads their current bytes.
struct WorldPolygonPartSource
{
    WorldPolygonHeaderView headers;
    WorldPolygonAttributeView attributes;
    std::span<const VehicleVector> vertices, normals;
    std::span<const std::uint16_t> indices;
    std::uint16_t model_flags_02{}, part_flags_02{};
};
inline WorldPolygonPartSource world_polygon_part_source(ModelResource &model, unsigned lod, unsigned part)
{
    return {model.source_attributes(lod, part),   WorldPolygonAttributeView(model.source_coordinates(lod, part)),
            model.source_vertices(lod, part),     model.source_normals(),
            model.source_corner_indices(),        model.source_model_flags_02(),
            model.source_part_flags_02(lod, part)};
}
enum class WorldPacketWriteKind
{
    header,
    indices,
    vertices,
    attributes,
    inside_mask,
    opaque_current,
    alpha_current
};
struct WorldPacketWriteEvent
{
    std::uint32_t instruction{};
    WorldPacketWriteKind kind{};
    bool alpha{};
    WorldPolygonPacketPointer pointer;
};
// NEW read-only synchronous diagnostics. Throwing retains completed writes.
// Packet fields use the source word boundaries, not native structure offsets.
struct WorldPacketEmissionObserver
{
    virtual ~WorldPacketEmissionObserver() = default;
    virtual void stored(const WorldPacketWriteEvent &, const WorldPolygonProducerList &opaque,
                        const WorldPolygonProducerList &alpha) = 0;
};

// Full429520/429660. Output begins at live61C510; neither function advances it.
// Writes only source-defined fields, including429520's low clip-mask halfword.
void transform_world_vertices_clipped(std::span<const VehicleVector>, std::span<WorldPolygonVertex>,
                                      const std::array<float, 16> &, const ProjectionParameters &);
void transform_world_vertices_unclipped(std::span<const VehicleVector>, std::span<WorldPolygonVertex>,
                                        const std::array<float, 16> &, const ProjectionParameters &);

// Full429B70/429D50. Lists are caller+0 and caller+8, preserving authored order.
// The clipped emitter uses the last corner for facing and writes inside_mask;
// the unclipped emitter uses the first corner and retains that fifth DWORD.
void emit_world_polygons_clipped(const WorldPolygonPartSource &, std::span<WorldPolygonVertex>,
                                 const VehicleVector &local_camera, WorldPolygonProducerList &opaque,
                                 WorldPolygonProducerList &translucent, WorldPacketEmissionObserver * = nullptr);
void emit_world_polygons_unclipped(const WorldPolygonPartSource &, std::span<WorldPolygonVertex>,
                                   const VehicleVector &local_camera, WorldPolygonProducerList &opaque,
                                   WorldPolygonProducerList &translucent, WorldPacketEmissionObserver * = nullptr);
} // namespace sgn::retail
