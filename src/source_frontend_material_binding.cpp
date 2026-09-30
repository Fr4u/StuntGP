#include "sgn/source_frontend_material_binding.hpp"
#include "sgn/race_session.hpp"
namespace sgn::retail
{
SourceFrontendMaterialBinding::SourceFrontendMaterialBinding(FrontendState &frontend, SessionWorldMaterials &materials,
                                                             MaterialD3D12 &backend)
    : frontend_(frontend), materials_(materials), backend_(backend), resource_epoch_(materials.source_resource_epoch()),
      replacement_(std::in_place, backend, materials.source_frontend_replacement_storage(backend)),
      device_(*replacement_),
      eager_textures_(
          std::in_place, frontend,
          materials.source_material_state_for(frontend.shared_platform()->source_scene_resources(), backend), device_)
{
}

SourceFrontendMaterialBinding::SourceFrontendMaterialBinding(FrontendState &frontend, SessionWorldMaterials &materials,
                                                             MaterialD3D12 &backend, TextureReplacementDevice &device)
    : frontend_(frontend), materials_(materials), backend_(backend), resource_epoch_(materials.source_resource_epoch()),
      replacement_(std::nullopt), device_(device), eager_textures_(std::nullopt)
{
}

SourceFrontendMaterialBinding::SourceFrontendMaterialBinding(FrontendState &frontend, SessionWorldMaterials &materials,
                                                             MaterialD3D12 &backend, TextureReplacementDevice &device,
                                                             const SourceNextSceneAdapterIdentity &identity)
    : frontend_(frontend), materials_(materials), backend_(backend), resource_epoch_(identity.scene_epoch),
      next_scene_(identity), replacement_(std::nullopt), device_(device), eager_textures_(std::nullopt)
{
}

RaceTextureReplacement &SourceFrontendMaterialBinding::textures() & noexcept
{
    // Keep acquisition inert, but never hand out the eager object which
    // bypasses this adapter's per-call epoch/font/material validation.
    return *this;
}
void SourceFrontendMaterialBinding::require_font_identity(const RetailFontResources &fonts) const
{
    frontend_.shared_platform()->require_owner_thread();
    if (next_scene_)
        materials_.require_next_scene_adapter_identity(*next_scene_, SourceMaterialEpochPhase::adapters_ready);
    else
        materials_.require_source_resource_epoch(resource_epoch_);
    if (&fonts != &frontend_.world().ui.fonts)
        throw FormatError("frontend material binding refers to another font/world owner");
}
RetailFontResources &SourceFrontendMaterialBinding::fonts() &
{
    if (!next_scene_)
        materials_.require_source_resource_epoch(resource_epoch_);
    auto &fonts =
        next_scene_ ? materials_.next_scene_fonts(*next_scene_, backend_) : materials_.source_frontend_fonts(backend_);
    require_font_identity(fonts);
    return fonts;
}
const RetailFontResources &SourceFrontendMaterialBinding::fonts() const &
{
    if (!next_scene_)
        materials_.require_source_resource_epoch(resource_epoch_);
    const auto &owner = static_cast<const SessionWorldMaterials &>(materials_);
    const auto &fonts =
        next_scene_ ? owner.next_scene_fonts(*next_scene_, backend_) : owner.source_frontend_fonts(backend_);
    require_font_identity(fonts);
    return fonts;
}
const WorldMaterialLoadingState &SourceFrontendMaterialBinding::current_images() const
{
    // Cold retains scene2 admission. Fresh next-scene bindings require Core's
    // committed identities; neither branch retains an invocation's state view.
    (void)fonts();
    const auto &scene = frontend_.shared_platform()->source_scene_resources();
    return next_scene_ ? materials_.next_scene_material_state_for(*next_scene_, scene, backend_)
                       : materials_.source_material_state_for(scene, backend_);
}
std::uint32_t SourceFrontendMaterialBinding::selected_overlay_texture() const
{
    SourceFrontendTextureReplacement receiver(frontend_, current_images(), device_);
    return receiver.selected_overlay_texture();
}
void SourceFrontendMaterialBinding::replace_source_texture(std::uint32_t destination, std::uint32_t source)
{
    SourceFrontendTextureReplacement receiver(frontend_, current_images(), device_);
    receiver.replace_source_texture(destination, source);
}
} // namespace sgn::retail
