#pragma once
#include <cstdint>
#include <span>

namespace sgn::retail
{
class SceneResources;
class SessionWorldMaterials;
struct SourceResourceStore
{
    std::uint32_t instruction, address;
    unsigned slot, offset;
};
// Synchronous read-only diagnostic seam. Do not retain borrows or mutate
// owners from callbacks. Exceptions preserve exactly the completed stores.
struct SourceResourceRegistryObserver
{
    virtual ~SourceResourceRegistryObserver() = default;
    virtual void published(SourceResourceStore, const SceneResources &) {}
    virtual void cleared(SourceResourceStore, const SceneResources &, std::span<const std::uint32_t, 16>) {}
};
struct SourceSceneResourcePrefixResult
{
    std::uint32_t next_required_instruction{0x42e53c};
    // This is NOT successful completion of42E510,42E6E0 or a warm load.
};
// Only42E510..42E52B including41C710; stop before42E53C's first later write.
// Same-world/thread preflight precedes every source store. No allocation
// release, geometry reset, flags reset or physical material reset occurs.
SourceSceneResourcePrefixResult reset_source_scene_resource_prefix_42e510(SceneResources &, SessionWorldMaterials &,
                                                                          SourceResourceRegistryObserver * = nullptr);
} // namespace sgn::retail
