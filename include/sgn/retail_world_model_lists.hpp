#pragma once
#include "sgn/retail_model_draw_queue.hpp"
#include "sgn/retail_world_polygon_frame.hpp"
#include "sgn/retail_world_model_visibility.hpp"
#include "sgn/retail_world_lit_model_producer.hpp"

namespace sgn::retail
{
// Full42AF90 for imported model records. The zero identity terminates the
// source array, irrespective of the queue's write cursor. Every descriptor0C
// remains bound to its actual retained command pose after this call.
// Full-call gate: tools/qualify_world_model_lists.py. Clipped producers borrow
// the live lifecycle.clip_near (camera198), overriding the projection copy.
void produce_world_regular_models(std::span<const ModelDrawCommand>, ModelResources &, const CameraState &,
                                  const CameraPresentationState &, const CameraLifecycle &, const ViewMatrixArena &,
                                  const CameraMatrix &identity, const ProjectionParameters &, WorldPolygonFrame &,
                                  std::uint32_t source_rate, std::uint32_t lighting_enabled, WorldModelLightingBinding);
// Full42AD30: bind the sky pose, set far5000000 (4A989680), begin group0, full42A260
// clipped route (source argument2), finish group0. Retains the new far field.
// The same full-call gate covers retained group, pose and far fields.
void produce_world_sky(const ModelDrawCommand &, ModelResources &, const CameraState &, CameraLifecycle &,
                       const ViewMatrixArena &, const CameraMatrix &identity, const ProjectionParameters &,
                       WorldPolygonFrame &);
} // namespace sgn::retail
