#pragma once
#include "sgn/model_resources.hpp"
#include "sgn/retail_world_materials.hpp"
#include <memory>

namespace sgn::retail
{
// Invocation input in the actual source registry order, starting at slot2.
// Flags must come from its owner. A null model terminates the source walk;
// generated strip/sprite resources do not enter this PMD-only registry.
struct ResourceMaterialBinding
{
    ModelResource *model{};
    std::uint32_t flags04{};
};

// New host adapter. Owns only pointer/span metadata, never model headers,
// names, texture IDs or material state. Source LOD slots and aliased part
// header addresses remain visible to42B8B0/42B900/42BA00/42C510.
// Both this adapter and every model must outlive a synchronous view borrow.
// A loading namespace can retain name pointers beyond the adapter lifetime:
// clear that namespace before destroying or replacing its model owners.
class ResourceMaterialRegistry final
{
    struct Storage;
    std::unique_ptr<Storage> storage_;

  public:
    explicit ResourceMaterialRegistry(std::span<const ResourceMaterialBinding>);
    ~ResourceMaterialRegistry();
    ResourceMaterialRegistry(const ResourceMaterialRegistry &) = delete;
    ResourceMaterialRegistry &operator=(const ResourceMaterialRegistry &) = delete;
    ResourceMaterialRegistry(ResourceMaterialRegistry &&) = delete;
    ResourceMaterialRegistry &operator=(ResourceMaterialRegistry &&) = delete;
    WorldMaterialRegistry view() const & noexcept;
    WorldMaterialRegistry view() const && = delete;
};
} // namespace sgn::retail
