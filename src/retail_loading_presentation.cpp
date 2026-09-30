#include "sgn/retail_loading_presentation.hpp"
#include "sgn/retail_loading_tables.hpp"
#include "sgn/session_outer_frame_services.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
std::uint32_t source_physical_count(const WorldMaterialLoadingState &state)
{
    // 445A62..445A92 /4459EF..445A1F: signed source pointer difference /28.
    const auto difference = std::bit_cast<std::int32_t>(state.names.materials.source_cursor() - 0x6227d4u);
    return std::bit_cast<std::uint32_t>(difference / std::int32_t{28});
}
} // namespace
RetailLoadingPresentation::RetailLoadingPresentation(LoadingPresentationWorld world,
                                                     FrontendPerformanceCounter &counter, PolygonSubmissionSink &sink,
                                                     LoadingFrameDevice &device)
    : world_(world), counter_(counter), sink_(sink), device_(device)
{
}
RetailLoadingPresentation::RetailLoadingPresentation(SessionOuterFrameServices &frames,
                                                     FrontendPerformanceCounter &counter, PolygonSubmissionSink &sink,
                                                     LoadingFrameDevice &device)
    : frames_(&frames), counter_(counter), sink_(sink), device_(device)
{
}
void RetailLoadingPresentation::validate() const
{
    if (world_ && &world_->tile_material_base != &world_->sequence.frame.quads.tile_material_base)
        throw FormatError("loading presentation must borrow the same tile material base owner");
}
std::uint32_t &RetailLoadingPresentation::tile_base(WorldMaterialLoadingState &materials,
                                                    MaterialTextureBackend &backend)
{
    return frames_ ? frames_->source_loading_tile_base(materials, backend) : world_->tile_material_base;
}
void RetailLoadingPresentation::play(WorldMaterialLoadingState &materials, MaterialTextureBackend &backend,
                                     std::uint32_t frames, std::uint32_t hold, std::uint32_t fade_out)
{
    // Fresh synchronous view after reset/upload. No span survives a reset call.
    auto world = frames_ ? frames_->borrow_source_loading_presentation(materials, backend) : *world_;
    world.sequence.frame.quads.materials = materials.names.materials.active_materials();
    run_loading_sequence_445940(world.sequence, frames, hold, fade_out, counter_, sink_, device_);
}
void RetailLoadingPresentation::finish_4459e0(WorldMaterialLoadingState &materials,
                                              WorldMaterialLoadingContext &context, MaterialFileSource &files,
                                              MaterialTextureBackend &backend)
{
    validate();
    reset_world_material_loading(materials, backend); // 4459E0
    load_world_material_prelude(source_loading_prelude(LoadingPrelude::loading), materials, context, files,
                                backend); // 4459EA
    const auto count = source_physical_count(materials);
    tile_base(materials, backend) = 1;          // 445A10
    materials.published_material_count = count; // 445A1F
    play(materials, backend, 30, 15, 0);
    backend.release_all(); // 445A2A,445A2F ->431810
}
void RetailLoadingPresentation::run(WorldMaterialLoadingState &materials, WorldMaterialLoadingContext &context,
                                    MaterialFileSource &files, MaterialTextureBackend &backend)
{
    auto &shown = frames_ ? frames_->source_loading_shown_word(materials, backend) : context.loading_already_shown;
    if (shown == 1)
    {
        context.loading_already_shown = shown;
        return;
    } // 445A40..445A47
    shown = 1; // 445A49, persistent canonical60F34C BEFORE reset/upload; never undone on failure
    context.loading_already_shown = shown; // synchronous caller snapshot, not another retained owner
    validate();
    reset_world_material_loading(materials, backend); // 445A53
    load_world_material_prelude(source_loading_prelude(LoadingPrelude::boot), materials, context, files,
                                backend); // 445A5D
    const auto count = source_physical_count(materials);
    tile_base(materials, backend) = 1;                 // 445A83
    materials.published_material_count = count;        // 445A92
    play(materials, backend, 300, 60, 120);            // 445A9D
    tile_base(materials, backend) = 5;                 // 445AAE
    play(materials, backend, 300, 60, 120);            // 445AB8
    finish_4459e0(materials, context, files, backend); // 445ABD tail
}
} // namespace sgn::retail
