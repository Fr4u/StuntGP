#pragma once
#include "sgn/retail_world_lit_model_producer.hpp"
#include "sgn/retail_world_model_visibility.hpp"

namespace sgn::retail
{
// Borrow the real descriptor0C pointer word and5C. Its target may be the
// retained queue pose or the persistent4864E8 template; never a local copy.
struct WorldTrackModelBinding
{
    ModelResource &model;
    const ModelTransform *&outer;
    const std::uint32_t &current_lod;
};
// Full42AD70.57B41C is reset by42E6E0, not per frame. This function uses the
// existing61C4F8 normalized camera without recomputing it for each part.
void initialize_world_track_shading(WorldTrackModelBinding, const ViewMatrixArena &, const CameraMatrix &,
                                    WorldModelViewScratch &, std::uint32_t quality_first,
                                    std::uint32_t &low_quality_initialized, WorldModelLightingBinding);
// Full42ADD0 including42AD70/428040 and actual geometry/lighting callees.
// The four packet lists are clipped opaque/alpha then inside opaque/alpha.
// Candidate pending its complete original/native comparison.
void produce_world_track_parts(WorldTrackModelBinding, const ModelTransform &persistent_template, const CameraState &,
                               const CameraPresentationState &, const CameraLifecycle &, const ViewMatrixArena &,
                               const CameraMatrix &, const ProjectionParameters &, WorldModelViewScratch &,
                               CameraMatrix &bound_matrix, WorldTransformedVertexCursor &,
                               std::array<WorldPolygonProducerList, 4> &, std::uint32_t quality_first,
                               std::uint32_t &quality_second, std::uint32_t rate_selector,
                               std::uint32_t &low_quality_initialized, std::uint32_t &accepted_models,
                               WorldModelLightingBinding);
} // namespace sgn::retail
