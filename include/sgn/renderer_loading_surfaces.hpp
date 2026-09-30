#pragma once
#include "sgn/retail_loading_frame.hpp"
#include "sgn/polygon_surface_recipes.hpp"
#include "sgn/renderer.hpp"
#include <thread>
#include "sgn/captured_front_recipe.hpp"

namespace sgn
{
class MaterialD3D12;
// NEW stable host adapter. It borrows the sole context, recipe pair and Renderer.
// No CPU Lock interface, source flag, second material bank or cadence lives here.
class RendererLoadingSurfaces final : public retail::LoadingFrameDevice
{
    Renderer *renderer_;
    PolygonDeviceContext &context_;
    PolygonSurfaceRecipes &recipes_;
    PolygonSurfaceRoles images_;
    bool fit_source_canvas_{};        // explicit NEW HOST menu policy, not a source viewport write
    bool back_publication_pending_{}; // CPU back accepted; GPU back may still be the prior image.
    // NEW HOST (R22): a role whose image was not rasterized for its current recipe
    // (deferred while the high-refresh presenter shows its own frames). Every
    // consumer of the GPU images refreshes it first; restore clears both.
    bool front_stale_{}, back_stale_{};
    void refresh_stale_images();
    const std::thread::id owner_ = std::this_thread::get_id();
    void check_thread() const;
    void ensure_images();
    std::uint64_t stage_back(const CapturedPolygonSubmission &, const SdrGammaRamp &);

  public:
    RendererLoadingSurfaces(Renderer &, PolygonDeviceContext &, PolygonSurfaceRecipes &);
    RendererLoadingSurfaces(const RendererLoadingSurfaces &) = delete;
    RendererLoadingSurfaces &operator=(const RendererLoadingSurfaces &) = delete;
    // NEW borrow identity check; does not construct or copy any surface owner.
    void require_binding(const Renderer &, const PolygonDeviceContext &) const;
    void enable_source_canvas_fit();
    bool needs_restore(const Renderer &) const;
    void restore_current_images(Renderer &); // no Flip, no Present, current CPU pair
    void suspend_materials(MaterialD3D12 &); // one CPU handoff over this exact pair
    void rebind_materials(Renderer &, MaterialD3D12 &);
    // Common post-seal operation for all application capture routes. Null is
    // inert. A copied ramp and owned immutable recipe commit only after Signal.
    void completed_back(ImmutablePolygonSubmission, SdrGammaRamp);
    // NEW HOST (R22): same acceptance as completed_back, but the GPU image is not
    // rasterized now. Used only for a race frame whose own Present the
    // high-refresh presenter suppresses; it is rasterized if ever presented.
    void completed_back_deferred_raster(ImmutablePolygonSubmission, SdrGammaRamp);
    // NEW HOST (R23): the host_wide presentation layout changed (original_4x3
    // switched live). Both retained recipes stay as sealed; their GPU images
    // are marked stale and re-rasterized (wide_menu presentation copy, canvas)
    // with the new layout by the next consumer (Flip draw, restore). Idle
    // closed context only; inert while the renderer is suspended (rebind
    // rasterizes both recipes anyway).
    void invalidate_presentation_images();
    // Preview application acceptance and presentation have distinct boundaries.
    // Retain before returning from the source tick; publish after that callback,
    // or before any source/input/audio work in the next callback. No third bank.
    void defer_completed_back(ImmutablePolygonSubmission, SdrGammaRamp);
    void publish_pending_back();
    bool draw(Renderer &);
    // NEW read-only diagnostic, including while an accepted back awaits publication.
    std::optional<CapturedPolygonFrontRecipe> capture_front_recipe() const &;
    std::optional<CapturedPolygonFrontRecipe> capture_front_recipe() const && = delete;
    // Preflight can throw; commit functions follow a successful outer/material
    // transaction on the same thread and cannot throw or invoke source code.
    void prepare_suspend() const;
    void commit_suspend() noexcept;
    void prepare_bind(Renderer &) const;
    void commit_bind(Renderer &, PolygonSurfaceRoles) noexcept;
    std::uint32_t surface_operation_432a60(std::uint32_t, std::uint32_t) override;
    std::uint32_t begin_scene() override;
    std::uint32_t end_scene() override;
    std::uint32_t finalize_423430(std::uint32_t) override;
};
} // namespace sgn
