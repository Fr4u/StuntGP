#pragma once
#include <memory>
namespace sgn::retail
{
class RaceSessionPlatform;
struct WorldModelDrawState;
struct SourceSpritePacketStorage;
// Exact434030 current-pointer publications and three part-counter stores.
void reset_source_sprite_cursors_434030(const std::weak_ptr<RaceSessionPlatform> &);
// Same concrete producer on synchronous existing world owners. Caller keeps
// their actual platform/bank alive; source pointer copies do not dereference.
void reset_source_sprite_cursors_434030(WorldModelDrawState &, SourceSpritePacketStorage &);
// Full434090: six allocations, registry publication,434030, rectangle and records.
void initialize_source_sprite_allocations_434090(const std::weak_ptr<RaceSessionPlatform> &);
// Full4342C0:434090, another434030, then all16 DWORD stores44EE10.
void initialize_source_sprite_scene_4342c0(const std::weak_ptr<RaceSessionPlatform> &);
} // namespace sgn::retail
