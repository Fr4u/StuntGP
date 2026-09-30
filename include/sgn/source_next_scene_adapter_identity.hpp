#pragma once
#include "sgn/source_allocation_bank.hpp"

namespace sgn
{
class MaterialD3D12;
}
namespace sgn::retail
{
// NEW HOST captured identities, not retail state or another resource owner.
// Only Core can install one in stable receivers. Copies retain weak allocation
// identities, never font payloads or GPU resources. The GPU epoch is a lower
// bound: real430D40 replacements advance it without changing resource tokens.
struct SourceNextSceneAdapterIdentity
{
    const MaterialD3D12 *backend{};
    const SourceAllocationBank *allocations{};
    std::uint64_t scene_epoch{}, backend_epoch_floor{}, physical_generation{};
    std::uint32_t font_count{};
    std::array<SourceAllocationRef, 2> font_scratch;
    std::array<std::array<std::uint32_t, 2>, 2> font_materials{}, font_resource_slots{}, font_tokens{};
};
} // namespace sgn::retail
