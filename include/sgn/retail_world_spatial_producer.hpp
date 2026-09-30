#pragma once
#include "sgn/retail_world_model_producer.hpp"
#include "sgn/world_indexed_vertex_binding.hpp"
#include "sgn/source_world_span.hpp"

namespace sgn::retail
{
// Original8-byte pair, expressed as two borrowed sentinel-terminated lists.
struct WorldSpatialBatch
{
    std::span<const std::uint16_t> polygons, vertices;
};
// One active binding; source records are exactly two DWORDs, never host spans.
class WorldSpatialRecords
{
    using Raw = std::array<std::uint32_t, 2>;
    std::variant<std::span<WorldSpatialBatch>, SourceWorldSpan<Raw>> records_{std::span<WorldSpatialBatch>{}};
    std::span<WorldSpatialBatch> native() const
    {
        if (source_bound())
            throw SourceAllocationError(0x42e4ed, "source spatial records require an address-token consumer");
        return std::get<0>(records_);
    }

  public:
    WorldSpatialRecords() = default;
    WorldSpatialRecords(std::span<WorldSpatialBatch> value) : records_(value) {}
    template <std::size_t N>
    WorldSpatialRecords(std::array<WorldSpatialBatch, N> &value) : records_(std::span<WorldSpatialBatch>{value})
    {
    }
    static WorldSpatialRecords source(SourceWorldSpan<Raw> value)
    {
        if (!value.source_bound())
            throw SourceAllocationError(0x42e4ed, "spatial source binding required");
        WorldSpatialRecords result;
        result.records_.emplace<1>(std::move(value));
        return result;
    }
    bool source_bound() const noexcept
    {
        return records_.index() == 1;
    }
    const SourceWorldBufferBinding &source_binding() const
    {
        if (!source_bound())
            throw SourceAllocationError(0x42abb0, "spatial source binding required");
        return std::get<1>(records_).source_binding();
    }
    bool is_null() const noexcept
    {
        return source_bound() ? !static_cast<bool>(std::get<1>(records_).source_binding().allocation)
                              : std::get<0>(records_).data() == nullptr;
    }
    std::size_t size() const
    {
        return source_bound() ? std::get<1>(records_).size() : native().size();
    }
    WorldSpatialBatch &operator[](std::size_t i) const
    {
        const auto value = native();
        if (i >= value.size())
            throw SourceAllocationError(0x42e4ed, "spatial record outside owner");
        return value[i];
    }
    std::span<WorldSpatialBatch> first(std::size_t count) const
    {
        const auto value = native();
        if (count > value.size())
            throw SourceAllocationError(0x42e4ed, "spatial range outside owner");
        return value.first(count);
    }
    operator std::span<WorldSpatialBatch>() const
    {
        return native();
    }
    std::uint32_t source_word(std::size_t record, std::size_t lane) const
    {
        if (!source_bound() || lane >= 2)
            throw SourceAllocationError(0x42e4ed, "invalid spatial DWORD read");
        return std::get<1>(records_)[record][lane];
    }
    void store_source_word(std::size_t record, std::size_t lane, std::uint32_t value)
    {
        if (!source_bound() || lane >= 2)
            throw SourceAllocationError(0x42e4ed, "invalid spatial DWORD store");
        std::get<1>(records_)[record][lane] = value;
    }
};
struct WorldSpatialBatchList
{
    WorldSpatialRecords records;
    std::uint32_t count{};
};
// Per-point42AA9B..42AB58 kernel, shared with the lazy Source PMD reader.
// No W read and no eager copy of the remaining seven bounds.
std::uint32_t world_spatial_point_mask_42aa9b(Vec3 point, Vec3 camera, std::span<const CameraVector, 4> planes);
// Full42AA70: return0 inside,1 rejected,2 intersecting; the source integer
// packing of float words is preserved, including its early intersecting exit.
std::uint32_t classify_world_spatial_bounds(std::span<const VehicleVector, 8>, Vec3 camera,
                                            std::span<const CameraVector, 4> planes);
// Full42ABB0, including ordered recursion and repeated references. Cycles in
// malformed input are a new host error; valid nodes are never deduplicated.
void gather_world_spatial_batches(const ModelSpatialData &, std::uint32_t root, Vec3 camera,
                                  std::span<const CameraVector, 4> planes, WorldSpatialBatchList &clipped,
                                  WorldSpatialBatchList &inside);
// Full429880/429730. Stamp equality compares the source16-bit stamp to the
// whole32-bit frame counter, then stores its low halfword on each new visit.
void transform_world_spatial_vertices_clipped(std::span<const WorldSpatialBatch>, std::span<const VehicleVector>,
                                              std::span<WorldPolygonVertex>, WorldIndexedVertexCursor &,
                                              std::uint32_t frame, const CameraMatrix &, const ProjectionParameters &);
void transform_world_spatial_vertices_inside(std::span<const WorldSpatialBatch>, std::span<const VehicleVector>,
                                             std::span<WorldPolygonVertex>, WorldIndexedVertexCursor &,
                                             std::uint32_t frame, const CameraMatrix &, const ProjectionParameters &);
// Full429EA0/42A090.61C550 counts batches, not emitted polygons. These track
// emitters route only by header flags, without the model/part alpha override.
void emit_world_spatial_polygons_clipped(std::span<const WorldSpatialBatch>, const WorldPolygonPartSource &,
                                         std::span<WorldPolygonVertex>, const VehicleVector &,
                                         std::uint32_t &batch_count, WorldPolygonProducerList &opaque,
                                         WorldPolygonProducerList &translucent,
                                         WorldPacketEmissionObserver * = nullptr);
void emit_world_spatial_polygons_inside(std::span<const WorldSpatialBatch>, const WorldPolygonPartSource &,
                                        std::span<WorldPolygonVertex>, const VehicleVector &,
                                        std::uint32_t &batch_count, WorldPolygonProducerList &opaque,
                                        WorldPolygonProducerList &translucent, WorldPacketEmissionObserver * = nullptr);
// Full42A220 preserves the call order and uses two separate opaque/alpha pairs.
void emit_world_spatial_polygons(std::span<const WorldSpatialBatch> clipped, std::span<const WorldSpatialBatch> inside,
                                 const WorldPolygonPartSource &, std::span<WorldPolygonVertex>, const VehicleVector &,
                                 std::uint32_t &batch_count, WorldPolygonProducerList &clipped_opaque,
                                 WorldPolygonProducerList &clipped_translucent, WorldPolygonProducerList &inside_opaque,
                                 WorldPolygonProducerList &inside_translucent);
} // namespace sgn::retail
