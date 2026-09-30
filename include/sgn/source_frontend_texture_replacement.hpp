#pragma once
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_world_material_loading.hpp"
#include "sgn/material_texture_replacement.hpp"
namespace sgn::retail
{
// NEW host binding of430D40 to the already-loaded complete source image bank.
// No names, images, source tables, material records or GPU resources are owned.
class SourceFrontendTextureReplacement final : public RaceTextureReplacement
{
    FrontendState &frontend_;
    const WorldMaterialLoadingState &images_;
    TextureReplacementDevice &device_;

  public:
    SourceFrontendTextureReplacement(FrontendState &, const WorldMaterialLoadingState &,
                                     TextureReplacementDevice &) noexcept;
    std::uint32_t selected_overlay_texture() const override;
    void replace_source_texture(std::uint32_t destination, std::uint32_t source) override;
};
} // namespace sgn::retail
