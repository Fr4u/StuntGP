#include "sgn/source_cold_frontend_assets.hpp"
#include "sgn/race_session.hpp"
#include "sgn/session_world_materials.hpp"
#include "sgn/source_numeric_win64.hpp"
#include "sgn/source_application_literals.hpp"

namespace sgn::retail
{
SourceColdFrontendAssetsResult load_source_cold_frontend_assets_42e72b(SourceColdSceneResourceContext &context,
                                                                       MaterialD3D12 &backend,
                                                                       WorldMaterialLoadingPresentation *presentation,
                                                                       FontBankObserver *observer)
{
    if (!context.platform)
        throw FormatError("42E72B requires the actual cold platform");
    auto &platform = *context.platform;
    platform.require_owner_thread();
    auto &world = platform.state();
    if (!platform.source_construction_only_entry() || world.scene_mode != 2)
        throw FormatError("42E72B cold entry requires source construction and the actual scene2 word");
    (void)context.materials.registry_flags_for(platform.source_scene_resources());
    // The caller must have reached this instruction through actual renderer
    // startup. These are real shared implementations, not optional callbacks.
    prepare_source_numeric_win64_429510();                                     // 42E72B, actual current-thread control
    context.materials.initialize_source_material_roots_42e3e0();               // 42E730, Roots full call
    update_viewport_ui_scale(world.viewport);                                  // 42E735 ->425070
    randomize_vehicle_liveries(world.vehicle_texture_selection, world.random); // 413190 first413110
    remap_level_vehicle_textures(
        platform.source_scene_resources().models(), world.cars,
        world.vehicle_texture_selection.random_liveries); // 42E73A/73F, full livery pass then tyre pass
    // Roots object is a synchronous borrow of the SAME boot-config buffers.
    // Keep the object alive because loading stores its ADDRESS in the context.
    const auto roots = context.materials.source_graphics_roots();
    // 441180 returns480134: the SAME App owner supplies its addressable null
    // terminal. No caller default-empty list and no duplicated string bank.
    // App must outlive this synchronous material/font-loading invocation.
    const auto literals = platform.source_application_literals();
    const auto names = literals.transient_material_names_441180();
    auto loading = context.materials.source_frontend_loading_context(roots, names);
    context.materials.load_source_frontend_assets_42e744(loading, context.files, backend, presentation, observer);
    return {}; // 42E772 was reached; no full scene/race/menu readiness is published
}
} // namespace sgn::retail
