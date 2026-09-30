#pragma once
#include <cstdint>
#include <memory>
namespace sgn::retail
{
class RaceSessionPlatform;
struct WorldPolygonFrame;
struct SourceSceneIndexedBaseResult
{
    std::uint32_t next_required_instruction{0x42e542};
};
// Synchronous read-only observer. An exception retains the completed store.
struct SourceSceneIndexedBaseObserver
{
    virtual ~SourceSceneIndexedBaseObserver() = default;
    virtual void cleared(std::uint32_t instruction, std::uint32_t address, const WorldPolygonFrame &) = 0;
};
// Exactly42E53C, following the separately callable registry prefix.
// The weak platform is resolved before any access. No source Session admission,
// owner replacement, bank action, keyboard store or whole-Warm wiring occurs.
SourceSceneIndexedBaseResult reset_source_scene_indexed_base_42e53c(const std::weak_ptr<RaceSessionPlatform> &,
                                                                    SourceSceneIndexedBaseObserver * = nullptr);
} // namespace sgn::retail
