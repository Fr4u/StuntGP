#include "sgn/session_world_materials.hpp"
#include "sgn/race_session.hpp"
#include "sgn/font_material_d3d12.hpp"
#include "sgn/source_material_files.hpp"
#include <bit>

namespace sgn::retail
{
void SessionWorldMaterials::initialize_source_next_scene_fonts_42e74e(SourceBootFilePlatform &files,
                                                                      MaterialD3D12 &backend)
{
    const auto platform = require_registry_live();
    if (!source_registry_only_ || !platform->source_construction_only_entry() || failed_ ||
        source_epoch_phase_ != SourceMaterialEpochPhase::fonts_reserved || !source_scene_tail_completed_ ||
        !attempted_ || !loaded_ || !source_fonts_attempted_ || !fonts_loaded_ || !published_ ||
        loaded_backend_ != &backend)
        throw FormatError("next428FA0 requires the retained source owner and Core's current font reservation");
    // Same current models which completed42C510, without asking the cold-only
    // getter to admit another epoch or using old completion flags as readiness.
    for (unsigned slot = 2; slot < 16; ++slot)
        if (scene_->model_handle(slot) != loaded_models_[slot - 2])
            throw FormatError("next428FA0 registry changed after this epoch's material binding");
    auto &world = platform->state();
    auto &bank = platform->source_allocations();
    if (!bank.initialized() || bank.source_words().active != 1)
        throw FormatError("next428FA0 requires the same active source allocation bank");
    const auto roots = source_graphics_roots(); // local views survive all synchronous file calls
    if (!source_graphics_roots_borrow_config(roots, world.boot_config))
        throw FormatError("next428FA0 roots must borrow this world's actual config buffers");
    // Binding is idempotent owner validation, not42EAB0/42EA80, allocation or
    // record clearing. Old scratch references remain generation-checked history.
    world.ui.fonts.bind_source_bank(bank);
    try
    {
        SourceMaterialFiles source_files(world.boot_config, files, roots);
        FontMaterialD3D12Target target(backend, state_.names.materials);
        initialize_retail_font_resources(world.ui.fonts, {bank, state_.source_tga_decode, nullptr}, source_files,
                                         {state_.width, state_.height, state_.tga_width, state_.tga_height},
                                         font_reload_views(state_.reload, state_.names.physical_upload_count),
                                         roots.normal, world.viewport.width, state_.format, target); // 42E74E ->428FA0
        if (source_epoch_phase_ != SourceMaterialEpochPhase::fonts_reserved)
            throw FormatError("nested font failure cannot continue to source42E753 count read");
        // 42E753 reads the physical cursor AFTER428FA0's atlas uploads. The
        // ordinary count58271C is a distinct owner and is not incremented here.
        const auto cursor = state_.names.materials.source_cursor();
        const auto difference = std::bit_cast<std::int32_t>(cursor - std::uint32_t{0x6227d4});
        const auto count = difference / std::int32_t{28};                      // source signed division toward zero
        state_.published_material_count = std::bit_cast<std::uint32_t>(count); // 42E772, actual6243BC
    }
    catch (...)
    {
        failed_ = true;
        throw; // retain reached allocations, records, uploads and cache; no compensating cleanup
    }
    // Cold font/attempt/publication flags remain historical. Only Core may
    // publish current fonts_count_completed, and42E778 is still unexecuted.
}
} // namespace sgn::retail
