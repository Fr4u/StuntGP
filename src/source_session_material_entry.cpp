#include "sgn/session_world_materials.hpp"
#include "sgn/source_pmd_material_loading.hpp"
#include "sgn/race_session.hpp"
#include "sgn/source_material_files.hpp"
#include <algorithm>

namespace sgn::retail
{
void SessionWorldMaterials::load_source_materials_42e744(WorldMaterialLoadingContext &context,
                                                         SourceBootFilePlatform &files, MaterialTextureBackend &backend,
                                                         WorldMaterialLoadingPresentation *presentation)
{
    const auto platform = require_registry_live();
    require_source_cold_resource_epoch();
    if (!source_registry_only_ || !platform->source_construction_only_entry())
        throw FormatError("source material entry requires the canonical source world owner");
    if (failed_ || attempted_ || platform->cold_materials_claimed_)
        throw FormatError("source material cold load already attempted in this world");
    auto &world = platform->state();
    if (context.scene != world.scene_mode)
        throw FormatError("source material context belongs to another scene mode");
    const auto &prefix = world.level.track_texture;
    const auto end = std::find(prefix.begin(), prefix.end(), '\0');
    if (end == prefix.end() || context.track_prefix != std::string(prefix.begin(), end))
        throw FormatError("source material prefix differs from this world's loaded level");
    if (!context.source_graphics_roots)
        throw FormatError("source material entry requires roots selected by source42E3E0");
    if (state_.effect_animation_storage != &world.effect_resources.animations)
        throw FormatError("source material animations have another world owner");

    SourceMaterialFiles source_files(world.boot_config, files, *context.source_graphics_roots);
    // Host lifetime/reentry reservation, not a retail initializer. It persists
    // on failure because PMD headers and physical state may already be changed.
    attempted_ = true;
    platform->cold_materials_claimed_ = true;
    try
    {
        load_source_pmd_world_materials_42c510(*scene_, registry_flags_, state_, context, source_files, backend,
                                               presentation);
        for (unsigned slot = 2; slot < 16; ++slot)
            loaded_models_[slot - 2] = scene_->model_handle(slot);
        loaded_backend_ = &backend;
        loaded_ = true;
    }
    catch (...)
    {
        failed_ = true;
        throw; // retain actual partial writes; no fabricated release/rollback
    }
    // 42E749,42E74E and42E772 remain later source operations. No published_,
    // fonts_loaded_, polygon ticket or preview/render admission is manufactured.
}

const WorldMaterialLoadingState &SessionWorldMaterials::source_material_state_for(
    const SceneResources &scene, const MaterialTextureBackend &backend) const &
{
    const auto platform = require_registry_live(); // hold owner during all borrowed accesses
    require_source_cold_resource_epoch();
    if (!source_registry_only_ || scene_ != &scene)
        throw FormatError("source material state requested for another scene owner");
    if (failed_ || !loaded_ || loaded_backend_ != &backend)
        throw FormatError("source material state requires its completed load and actual backend");
    for (unsigned slot = 2; slot < 16; ++slot)
    {
        if (scene_->model_handle(slot) != loaded_models_[slot - 2])
            throw FormatError("source material registry changed after loading");
    }
    return state_; // same state, read-only; completion of42C510 alone is not render readiness
}
} // namespace sgn::retail
