#pragma once
#include "sgn/scene_resources.hpp"
#include "sgn/retail_world_polygon_frame.hpp"
#include "sgn/retail_camera_lifecycle.hpp"
#include "sgn/world_model_draw.hpp"

namespace sgn::retail
{
// Full42AF30 for actual source PMDs,434090 sprites and41C530 strip descriptors.
// commands must be a subspan of this same draw owner's real retained queue.
// Generated strips retain the captured allocation token in each queue word.
// No native ModelResource getters, geometry copies, owner rebinding or warm
// admission. All owners remain alive/stable on their thread for the call.
void produce_source_world_background_models_42af30(std::span<const ModelDrawCommand>, const SceneResources &,
                                                   const WorldModelDrawState &, const SourceSpritePacketStorage &,
                                                   const CameraState &, const CameraPresentationState &,
                                                   const CameraLifecycle &, const ViewMatrixArena &,
                                                   const CameraMatrix &, const ProjectionParameters &,
                                                   WorldPolygonFrame &, const std::uint32_t &source_rate);
// Complete42A260 for the actual generated descriptor, including the current
// selector, part loop and live count reload. descriptor+0C must already name
// this owner's retained pose; this call does not invent a queue publication.
void produce_source_world_sprite_unlit_42a260(const SceneResources &, const WorldModelDrawState &,
                                              const SourceSpritePacketStorage &, const CameraState &,
                                              const ViewMatrixArena &, const CameraMatrix &,
                                              const ProjectionParameters &, bool clipped, WorldModelViewScratch &,
                                              WorldTransformedVertexCursor &, WorldPolygonProducerList &opaque,
                                              WorldPolygonProducerList &alpha);
} // namespace sgn::retail
