#include "sgn/session_world_materials.hpp"
#include "sgn/race_session.hpp"
#include "sgn/font_material_d3d12.hpp"
#include "sgn/retail_material_filesystem.hpp"

namespace sgn::retail
{
void SessionWorldMaterials::load_with_source_fonts(WorldMaterialLoadingContext &context, MaterialD3D12 &backend,
                                                   WorldMaterialLoadingPresentation *presentation,
                                                   FontBankObserver *observer)
{
    const auto platform = require_live();
    auto &bank = platform->source_allocations();
    auto &world = platform->state();
    if (!context.source_graphics_roots)
        throw FormatError("Session source fonts require explicit source graphics roots");
    world.ui.fonts.bind_source_bank(bank); // preflight before the existing material load's writes
    load(context, backend, presentation);  // earlier material producers remain explicitly unmigrated
    try
    {
        RetailMaterialFileSystem files(platform->assets(), platform->retail_root());
        FontMaterialD3D12Target target(backend, state_.names.materials);
        initialize_retail_font_resources(world.ui.fonts, {bank, state_.source_tga_decode, observer}, files,
                                         {state_.width, state_.height, state_.tga_width, state_.tga_height},
                                         font_reload_views(state_.reload, state_.names.physical_upload_count),
                                         context.source_graphics_roots->normal, world.viewport.width, state_.format,
                                         target);
        fonts_loaded_ = true;
    }
    catch (...)
    {
        failed_ = true;
        detach_world_polygon_level(world.polygon_frame);
        throw;
    }
}
void SessionWorldMaterials::load_with_fonts(WorldMaterialLoadingContext &context, MaterialD3D12 &backend,
                                            WorldMaterialLoadingPresentation *presentation)
{
    const auto platform = require_live();
    if (!context.source_graphics_roots)
        throw FormatError("Session fonts require the loader's explicit source graphics roots");
    // One synchronous backend and source-root lifetime covers both operations.
    // load owns its existing cold reservation, binding checks and failure path.
    load(context, backend, presentation);
    try
    {
        auto &world = platform->state();
        RetailMaterialFileSystem files(platform->assets(), platform->retail_root());
        FontMaterialD3D12Target target(backend, state_.names.materials);
        // 42E749's general-file-cache cleanup has no native cache owner in this
        // file adapter. It is not a reset of material images, reloads or fonts.
        // This is the composed cold loading/font slice, not full42E6E0.
        initialize_retail_font_resources(
            world.ui.fonts, files, {state_.width, state_.height, state_.tga_width, state_.tga_height},
            font_reload_views(state_.reload, state_.names.physical_upload_count), context.source_graphics_roots->normal,
            world.viewport.width, state_.format, target);
        fonts_loaded_ = true;
    }
    catch (...)
    {
        failed_ = true;
        detach_world_polygon_level(platform->state().polygon_frame);
        throw;
    }
    // 6243BC publication remains the caller's subsequent42E772 operation.
}
const RetailFontResources &SessionWorldMaterials::validate_fonts_for(const RaceSession &session) const &
{
    (void)validate_for(session);
    if (!fonts_loaded_)
        throw FormatError("Session UI requires completed font initialization before publication");
    return session.state().ui.fonts;
}
} // namespace sgn::retail
