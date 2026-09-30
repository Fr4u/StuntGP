#include "sgn/session_world_materials.hpp"
#include "sgn/source_pmd_material_loading.hpp"
#include "sgn/source_material_files.hpp"
#include "sgn/source_application_literals.hpp"
#include "sgn/race_session.hpp"
#include <algorithm>
#include <bit>

namespace sgn::retail
{
void SessionWorldMaterials::load_source_next_scene_materials_42e744(SourceBootFilePlatform &files,
                                                                    MaterialTextureBackend &backend,
                                                                    WorldMaterialLoadingPresentation &presentation)
{
    const auto platform = require_registry_live();
    if (!source_registry_only_ || !platform->source_construction_only_entry() ||
        source_epoch_phase_ != SourceMaterialEpochPhase::materials_reserved || failed_ ||
        !source_scene_tail_completed_ || !attempted_ || !loaded_ || !fonts_loaded_ || !published_ ||
        loaded_backend_ != &backend)
        throw FormatError("next42C510 requires the retained completed source owner and its Core reservation");
    auto &world = platform->state();
    if (state_.effect_animation_storage != &world.effect_resources.animations)
        throw FormatError("next42C510 effect-animation owner differs from this source world");
    // Invocation context only. Source owners survive; no second material bank
    // or decoded geometry is constructed, and no cold claim is reset.
    const auto roots = source_graphics_roots(); // actual48491C/48499C, kept alive through every file call
    if (!source_graphics_roots_borrow_config(roots, world.boot_config))
        throw FormatError("next42C510 roots must borrow this world's actual config buffers");
    const auto end = std::find(world.level.track_texture.begin(), world.level.track_texture.end(), '\0');
    if (end == world.level.track_texture.end())
        throw FormatError("next42C510 track prefix lacks its source terminator");
    const auto literals = platform->source_application_literals();
    WorldMaterialLoadingContext context;
    context.scene = world.scene_mode; // actual2 selects frontend table; every other value selects normal
    context.quality = state_.source_quality_6249a0;
    context.maximum_dimension = std::bit_cast<std::int32_t>(state_.source_maximum_dimension_6249a4);
    context.loading_presentation_enabled = world.boot_config.display_boot_graphics;
    context.loading_already_shown = state_.source_loading_shown_60f34c;
    context.track_prefix.assign(world.level.track_texture.begin(), end);
    context.transient_image_names = literals.transient_material_names_441180();
    context.source_graphics_roots = &roots;
    SourceMaterialFiles source_files(world.boot_config, files, roots);
    try
    {
        // Actual42BDD0, optional445A40,42C480, source-selected42C340, live
        //  registry/flags traversal42C440/42B900 and ordered42BA90 remapping.
        //  Existing reset releases the backend but retains first-use cache
        //  state exactly as42C1D0 specifies. No old PMD names are dereferenced
        //  before42C480 clears their namespace containers.
        load_source_pmd_world_materials_42c510(*scene_, registry_flags_, state_, context, source_files, backend,
                                               &presentation);
        if (source_epoch_phase_ != SourceMaterialEpochPhase::materials_reserved)
            throw FormatError("nested material epoch failure cannot publish next42C510 bindings");
        // Refresh host validation metadata only after the source remap returns.
        // Keep fonts_loaded_/published_ as prior cold history; epoch guards
        // still reject every ordinary material/font/draw borrower.
        for (unsigned slot = 2; slot < 16; ++slot)
            loaded_models_[slot - 2] = scene_->model_handle(slot);
        loaded_backend_ = &backend;
        loaded_ = true;
    }
    catch (...)
    {
        failed_ = true;
        throw; // no cache reset, PMD rollback, compensating release or retry
    }
    // 42E749/44B1A0 is the owning Core's next call, not an unconditional cleanup.
}
} // namespace sgn::retail
