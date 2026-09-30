#pragma once
#include "sgn/session_world_materials.hpp"
#include "sgn/source_frontend_texture_replacement.hpp"
#include <optional>
namespace sgn::retail
{
// All borrowed owners outlive this stable receiver. The four-argument path
// attaches before cold initialization; only actual resource use admits a borrow.
class SourceFrontendMaterialBinding : private RaceTextureReplacement
{
    friend class SourceApplicationCore;
    FrontendState &frontend_;
    SessionWorldMaterials &materials_;
    MaterialD3D12 &backend_;
    const std::uint64_t resource_epoch_;                             // lifetime identity, never source data
    const std::optional<SourceNextSceneAdapterIdentity> next_scene_; // never mutates an old binding
    // Core-only inert construction during its post-tail adapter reservation.
    SourceFrontendMaterialBinding(FrontendState &, SessionWorldMaterials &, MaterialD3D12 &, TextureReplacementDevice &,
                                  const SourceNextSceneAdapterIdentity &);
    std::optional<sgn::MaterialTextureReplacement> replacement_;
    TextureReplacementDevice &device_;
    std::optional<SourceFrontendTextureReplacement> eager_textures_;
    void require_font_identity(const RetailFontResources &) const;
    const WorldMaterialLoadingState &current_images() const;
    std::uint32_t selected_overlay_texture() const override;
    void replace_source_texture(std::uint32_t, std::uint32_t) override;

  public:
    // Existing eager behavior: validates and constructs replacement immediately.
    SourceFrontendMaterialBinding(FrontendState &, SessionWorldMaterials &, MaterialD3D12 &);
    // Inert attachment. The capture device and actual backend may be prepared
    // later by the source cold sequence, but their C++ owners must already exist.
    SourceFrontendMaterialBinding(FrontendState &, SessionWorldMaterials &, MaterialD3D12 &,
                                  TextureReplacementDevice &);
    SourceFrontendMaterialBinding(const SourceFrontendMaterialBinding &) = delete;
    SourceFrontendMaterialBinding &operator=(const SourceFrontendMaterialBinding &) = delete;
    SourceFrontendMaterialBinding(SourceFrontendMaterialBinding &&) = delete;
    SourceFrontendMaterialBinding &operator=(SourceFrontendMaterialBinding &&) = delete;
    // Receiver acquisition is inert, including in navigation constructors.
    // Deferred operations revalidate live resources on EVERY invocation.
    RaceTextureReplacement &textures() & noexcept;
    RetailFontResources &fonts() &;
    const RetailFontResources &fonts() const &;
    const RetailFontResources &fonts() const && = delete;
};
} // namespace sgn::retail
