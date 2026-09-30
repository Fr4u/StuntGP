#pragma once
#include "sgn/world_model_draw.hpp"

namespace sgn::retail
{
// 434090 typed allocation initialization, including full42E8D0 zeroing.
// The generated descriptor has one part, zero bounds/pivot/LOD, and all five
// LOD pointers alias this same storage. The global4788C0 pose and sprite axes
// are retained; this is startup initialization, not434030's per-frame reset.
class SceneResources;
// Standalone leaf entry has no Scene registry; actual owners use the overload.
void initialize_world_sprite_model(WorldModelDrawState &, SourceSpritePacketStorage &);
void initialize_world_sprite_model(WorldModelDrawState &, SourceSpritePacketStorage &, SceneResources &);
} // namespace sgn::retail
