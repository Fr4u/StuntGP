#pragma once
#include "sgn/world_model_draw.hpp"

namespace sgn::retail
{
// Complete42B740/42B6E0/434030 source-owner path. Track/sky/sprite are captured
// from the actual scene slots3/2/0 at their reached source sites, never supplied
// preview handles. The existing queue owns only source's copied80-byte poses.
// Caller borrows the same scene, draw/sprite world and matrix arena; all remain
// stable on the owner thread. Source allocations/PMD metadata are not retained.
// Numeric basis uses the existing PC53/round-nearest value contract. Host extent
// guards retain earlier reached stores; invalid-pointer/FP-trap parity is not
// claimed. No world draw/lighting/warm-admission gate is bypassed by this API.
void seed_source_world_model_draws_42b740(WorldModelDrawState &, ModelResources &, SourceSpritePacketStorage &,
                                          float ground_level, std::uint32_t scene_mode, ViewMatrixArena &,
                                          const class SceneResources &);
} // namespace sgn::retail
