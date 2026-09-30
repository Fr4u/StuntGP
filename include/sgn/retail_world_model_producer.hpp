#pragma once
#include "sgn/source_world_span.hpp"
#include "sgn/retail_model_matrices.hpp"
#include "sgn/retail_camera_transform.hpp"
#include "sgn/retail_world_polygon_producer.hpp"

namespace sgn::retail
{
// Single source owners61C4E8/61C4F8/61C428, retained between producer calls.
struct WorldModelViewScratch
{
    VehicleVector local_camera, normalized_camera;
    CameraMatrix model_view;
};
// Optional observation only. One means the source byte came from an
// uninitialized stack lane; it never affects arithmetic or branch selection.
struct WorldLocalCameraAudit
{
    std::array<std::uint8_t, 16> local{}, normalized{};
};
// Full42B2A0 including44ED40. A model with <=1 part copies one uninitialized
// source stack DWORD into W; native policy retains the existing opaque W.
// No source consumer in this path reads W for geometry or normalization.
void world_model_local_camera(VehicleVector &local, VehicleVector &normalized, Vec3 camera_position,
                              Vec3 outer_position, const CameraMatrix &inverse_outer, Vec3 part_position, Vec3 pivot,
                              const CameraMatrix *inverse_part, std::uint32_t part_count,
                              WorldLocalCameraAudit *audit = nullptr);

struct WorldTransformedVertexCursor
{
    SourceWorldSpan<WorldPolygonVertex> allocation;
    std::uint32_t count{}; // 61C510 relative to allocation61C50C, divided by32
};
// Complete42A260. Uses only source flags==1, selected descriptor5C slot and
// already allocated matrices. Packet spans borrow the same retained storage.
void produce_world_model_unlit(ModelResource &, unsigned source_lod, const ModelTransform &outer, const CameraState &,
                               const ViewMatrixArena &, const CameraMatrix &shared_identity,
                               const ProjectionParameters &, bool clipped, WorldModelViewScratch &,
                               WorldTransformedVertexCursor &, WorldPolygonProducerList &opaque,
                               WorldPolygonProducerList &translucent);
} // namespace sgn::retail
