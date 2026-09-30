#pragma once
#include "sgn/world_model_draw.hpp"
#include "sgn/retail_world_model_producer.hpp"

namespace sgn::retail
{
// Source-backed calls require source_scene; the canonical descriptor fields
// are consumed directly, independently of inactive native preview counters.
// 42A260 for434090's generated one-part descriptor. descriptor0C must borrow
// the live queue pose before this call. All source LOD slots alias the same
// part. The zero bounds return2 from428040; the caller selects its clipped pool.
// Uses part+24/+28, independently of append cursors; source does not read+26.
// projection.clip_near is a call-time binding of the current lifecycle+198,
// not a retained projection-cache value (the same contract as the PMD producer).
// Packets borrow headers/indices/attributes from sprites and output vertices
// from cursor. No temporary position conversion survives the call.
void produce_world_sprite_unlit(const WorldModelDrawState &, const SourceSpritePacketStorage &, const CameraState &,
                                const ViewMatrixArena &, const CameraMatrix &shared_identity,
                                const ProjectionParameters &, bool clipped, WorldModelViewScratch &,
                                WorldTransformedVertexCursor &, WorldPolygonProducerList &opaque,
                                WorldPolygonProducerList &translucent, const SceneResources *source_scene = nullptr);
} // namespace sgn::retail
