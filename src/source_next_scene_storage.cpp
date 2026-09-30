#include "sgn/session_world_materials.hpp"
#include "sgn/race_session.hpp"
#include "sgn/scene_resources.hpp"
#include "sgn/source_polygon_vertex_clip.hpp"
#include "sgn/source_polygon_indexed.hpp"
#include "sgn/source_polygon_packet_pools.hpp"
#include "sgn/source_screen_base.hpp"
#include "sgn/source_spatial_light_allocation.hpp"
#include "sgn/source_clip_initialization.hpp"
#include "sgn/source_model_resource_preparation.hpp"

namespace sgn::retail
{
void SessionWorldMaterials::prepare_source_next_scene_storage_42e778()
{
    const auto platform = require_registry_live();
    if (!source_registry_only_ || !platform->source_construction_only_entry() || failed_ ||
        source_epoch_phase_ != SourceMaterialEpochPhase::scene_storage_reserved || !source_scene_tail_completed_ ||
        !attempted_ || !loaded_ || !source_fonts_attempted_ || !fonts_loaded_ || !published_)
        throw FormatError("next42E430 requires the retained source owner and Core's current storage reservation");
    for (unsigned slot = 2; slot < 16; ++slot)
        if (scene_->model_handle(slot) != loaded_models_[slot - 2])
            throw FormatError("next42E430 registry changed after this epoch's material/font binding");
    const auto &bank = platform->source_allocations();
    if (!bank.initialized() || bank.source_words().active != 1)
        throw FormatError("next42E430 requires this world's current active allocation bank");
    try
    {
        // 42E778 ->42E430: reuse every reached allocator and actual publication.
        // No detached pool, cursor preclear, free or rollback is introduced.
        (void)allocate_source_polygon_vertex_clip_42e430(platform);  // 4 calls, through6244D0
        (void)allocate_source_polygon_indexed_42e47b(platform);      // 61C5EC, retains61C520
        (void)allocate_source_polygon_packet_pools_42e48c(platform); // 4 actual base stores
        (void)allocate_source_screen_base_42e4d0(platform);          // 61C5F0, retains count
        allocate_source_spatial_light_tail_42e4e1(platform);         // last3 calls, through624530
        initialize_source_clip_storage_427270(platform);             // 42E77D, alias then cursor/index stores
        prepare_source_model_resources_42e380(platform);             // 42E782, actual descriptors and light strength
    }
    catch (...)
    {
        failed_ = true;
        throw; // retain reached allocations/publications/partial source writes
    }
    // 42E787 ->402420 is NOT called. Historical cold completion remains history;
    // only Core can publish this bounded stage's current-epoch observation.
}
} // namespace sgn::retail
