#pragma once
#include "sgn/source_cold_frontend_assets.hpp"
#include "sgn/source_renderer_bootstrap.hpp"

namespace sgn::retail
{
// Invocation-only borrows of the actual implementations. The renderer calls
// must have concrete enumeration/device/surface bindings; this context cannot
// manufacture their completion or replace them with a Renderer constructor.
struct SourceColdSceneStartupContext
{
    SourceColdSceneResourceContext scene;
    SourceRendererBootstrapContext renderer;
    WorldMaterialLoadingPresentation *loading_presentation{};
    FontBankObserver *font_observer{};
};

// Concrete42E6E0 slice through42E772. Each mandatory child runs synchronously;
// throwing preserves reached state, including a successfully returned431360's
// 6244F8/quality stores. Does not call42EAB0 or publish menu/race readiness.
// The returned42E778 boundary is NOT full42E6E0/442030 completion.
[[nodiscard]] SourceColdFrontendAssetsResult load_source_cold_scene_through_materials_42e6e0(
    SourceColdSceneStartupContext &);
} // namespace sgn::retail
