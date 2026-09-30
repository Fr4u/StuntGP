#pragma once
#include "sgn/retail_world_model_visibility.hpp"
#include "sgn/retail_world_polygon_frame.hpp"
#include "sgn/retail_world_sprite_producer.hpp"
#include "sgn/retail_world_strip_producer.hpp"

namespace sgn::retail
{
// Native42AF30 over imported PMDs, sprites and generated strips. The source
// branch requires its actual SceneResources and consumes canonical PMDs and
// sprite allocations; type1 source strip remains an explicit boundary.
// Descriptor0C names the actual retained command pose after publication.
void produce_world_background_models(std::span<const ModelDrawCommand>, ModelResources &, WorldModelDrawState &,
                                     const SourceSpritePacketStorage &, VehicleStripGeometry &, const CameraState &,
                                     const CameraPresentationState &, const CameraLifecycle &, const ViewMatrixArena &,
                                     const CameraMatrix &, const ProjectionParameters &, WorldPolygonFrame &,
                                     const std::uint32_t &source_rate, const SceneResources *source_scene = nullptr);
} // namespace sgn::retail
