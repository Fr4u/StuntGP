#pragma once
#include "sgn/material_d3d12.hpp"
#include "sgn/race_overlay_textures.hpp"

namespace sgn
{
// Borrows the complete physical owner. Each430D40 call resolves its current
// next-free slot, including after fonts append; no detached tail-word storage.
class MaterialTextureReplacement final : public retail::TextureReplacementDevice
{
    MaterialD3D12 &backend_;
    retail::PhysicalMaterialStorage &materials_;
    std::uint64_t material_generation_;
    std::vector<std::uint32_t> primary_tokens_; // Immutable resource identities, not material aliases.
    void require_binding(std::uint32_t slot) const;

  public:
    MaterialTextureReplacement(MaterialD3D12 &, retail::PhysicalMaterialStorage &);
    std::uint16_t destination_width(std::uint32_t slot) const override;
    void upload_to_destination(std::uint32_t slot, const Image &) override;
};
} // namespace sgn
