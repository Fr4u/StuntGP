#include "sgn/session_world_materials.hpp"
#include "sgn/race_session.hpp"
#include "sgn/font_material_d3d12.hpp"
#include "sgn/source_material_files.hpp"

namespace sgn::retail
{
void SessionWorldMaterials::initialize_source_fonts_42e74e(WorldMaterialLoadingContext &context,
                                                           SourceBootFilePlatform &files, MaterialD3D12 &backend,
                                                           FontBankObserver *observer)
{
    const auto platform = require_registry_live();
    // Same private state, models and actual backend which completed42C510.
    (void)source_material_state_for(*scene_, backend);
    if (source_fonts_attempted_ || fonts_loaded_)
        throw FormatError("source font cold initialization already completed");
    if (!context.source_graphics_roots)
        throw FormatError("source fonts require source graphics roots");
    auto &world = platform->state();
    if (context.scene != world.scene_mode)
        throw FormatError("source font context belongs to another scene");
    auto &bank = platform->source_allocations();
    // Both the font bank and WAD cache are the existing world owners. No implicit
    // allocation boot, cache release, material reset or second42C510 occurs here.
    world.ui.fonts.bind_source_bank(bank);
    source_fonts_attempted_ = true; // reject callback reentry; survives partial failure
    try
    {
        SourceMaterialFiles source_files(world.boot_config, files, *context.source_graphics_roots);
        FontMaterialD3D12Target target(backend, state_.names.materials);
        initialize_retail_font_resources(world.ui.fonts, {bank, state_.source_tga_decode, observer}, source_files,
                                         {state_.width, state_.height, state_.tga_width, state_.tga_height},
                                         font_reload_views(state_.reload, state_.names.physical_upload_count),
                                         context.source_graphics_roots->normal, world.viewport.width, state_.format,
                                         target);
        fonts_loaded_ = true;
    }
    catch (...)
    {
        failed_ = true;
        throw;
    }
    // 42E753..42E772 publishes the physical count later, after font resources.
    // Do not set published_ or make the source world preview/render-ready here.
}
} // namespace sgn::retail
