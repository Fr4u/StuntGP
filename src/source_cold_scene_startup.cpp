#include "sgn/source_cold_scene_startup.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session.hpp"
#include "sgn/session_world_materials.hpp"

namespace sgn::retail
{
SourceColdFrontendAssetsResult load_source_cold_scene_through_materials_42e6e0(SourceColdSceneStartupContext &context)
{
    if (!context.scene.platform)
        throw FormatError("42E6E0 requires its retained source platform");
    const auto &platform = context.scene.platform;
    platform->require_owner_thread();
    auto &frontend = context.renderer.frontend;
    if (frontend.shared_platform() != platform || &frontend.world() != &platform->state() ||
        &context.renderer.materials != &context.scene.materials)
        throw FormatError("42E6E0 renderer must borrow the same frontend/world/material owner");
    (void)initialize_source_cold_scene_prefix_42e6e0(context.scene); // 42E6E1..42E6F5
    auto &world = platform->state();
    // 42E6FA captures EAX once. A nonzero value other than1 also takes431300.
    const auto initialized = world.outer.device.initialized_6244f8;
    if (initialized == 0)
    {
        initialize_legacy_driver_restriction(world.view_quality); // 42E708 ->42E6D0
        initialize_source_renderer_431360(context.renderer);      // 42E70D, actual mandatory children
        world.outer.device.initialized_6244f8 = 1;                // 42E712, only AFTER431360 returned
        world.view_quality.first = 1;                             // 42E718:624980
        world.view_quality.second = 1;                            // 42E71E:624984
    }
    // NEW borrow of the renderer actually constructed/selected by the child.
    // The getter cannot reset owners; the returned references live through
    // font loading. It also supplies the same real backend for the nonzero path.
    auto renderer = context.renderer.calls.reset_context(frontend);
    if (&renderer.frontend != &frontend)
        throw FormatError("42E6E0 renderer reset borrow has a foreign frontend");
    renderer.frames.require_source_frontend(frontend);
    if (initialized != 0)
        reset_source_renderer_431300(renderer); // 42E726
    return load_source_cold_frontend_assets_42e72b(context.scene, renderer.materials, context.loading_presentation,
                                                   context.font_observer);
}
} // namespace sgn::retail
