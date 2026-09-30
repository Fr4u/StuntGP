#pragma once
#include "sgn/retail_world_model_producer.hpp"
#include "sgn/retail_vehicle_strip.hpp"

namespace sgn::retail
{
// Full42A260 over the real41C530/41C9B0 generated one-part descriptor.
// The caller owns mutable descriptor0C publication (42AF43). This synchronous
// producer borrows geometry and the retained queued outer pose without copies.
// The projection's clip_near must represent the live current camera198.
void produce_world_strip_unlit(const VehicleStripGeometry &, const CameraState &, const ViewMatrixArena &,
                               const CameraMatrix &identity, const ProjectionParameters &, bool clipped,
                               WorldModelViewScratch &, WorldTransformedVertexCursor &,
                               WorldPolygonProducerList &opaque, WorldPolygonProducerList &translucent);
} // namespace sgn::retail
