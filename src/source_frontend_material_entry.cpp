#include "sgn/session_world_materials.hpp"
#include "sgn/race_session.hpp"
#include "sgn/material_d3d12.hpp"
namespace sgn::retail
{
void SessionWorldMaterials::load_source_frontend_assets_42e744(WorldMaterialLoadingContext &context,
                                                               SourceBootFilePlatform &files, MaterialD3D12 &backend,
                                                               WorldMaterialLoadingPresentation *presentation,
                                                               FontBankObserver *observer)
{
    const auto platform = require_registry_live();
    if (context.scene != 2 || platform->state().scene_mode != 2)
        throw FormatError("frontend assets require the actual source scene2");
    // Existing301-row pinned frontend prelude, source archive/files, real D3D12
    // uploads. Any PMD names are handled by the same42C510 registry traversal.
    load_source_materials_42e744(context, files, backend, presentation); // 42E744 CALL42C510
    release_platform_source_archive_42e749(*platform);                   // actual44B1A0
    initialize_source_fonts_42e74e(context, files, backend, observer);   // 428FA0
    publish_source_count_42e772(backend);
}
PhysicalMaterialStorage &SessionWorldMaterials::source_frontend_replacement_storage(
    const MaterialTextureBackend &backend) &
{
    const auto platform = require_registry_live();
    (void)source_material_state_for(*scene_, backend);
    if (platform->state().scene_mode != 2 || !fonts_loaded_ || !published_)
        throw FormatError("frontend replacement requires completed scene2 materials/fonts/publication");
    return state_.names.materials;
}
RetailFontResources &SessionWorldMaterials::source_frontend_fonts(const MaterialTextureBackend &backend) &
{
    const auto platform = require_registry_live();
    (void)source_material_state_for(*scene_, backend);
    if (platform->state().scene_mode != 2 || !fonts_loaded_ || !published_)
        throw FormatError("frontend font borrow requires completed source scene2 resources");
    return platform->state().ui.fonts;
}
const RetailFontResources &SessionWorldMaterials::source_frontend_fonts(const MaterialTextureBackend &backend) const &
{
    const auto platform = require_registry_live();
    (void)source_material_state_for(*scene_, backend);
    if (platform->state().scene_mode != 2 || !fonts_loaded_ || !published_)
        throw FormatError("frontend font borrow requires completed source scene2 resources");
    return platform->state().ui.fonts;
}
} // namespace sgn::retail
