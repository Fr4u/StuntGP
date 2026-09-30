#include "sgn/renderer_loading_surfaces.hpp"
#include <ddraw.h>
#include "sgn/material_d3d12.hpp"
#include "sgn/host_wide_menu.hpp"

namespace sgn
{
namespace
{
bool empty_images(const PolygonSurfaceRoles &images) noexcept
{
    return !images.front_image && !images.back_image;
}
} // namespace
RendererLoadingSurfaces::RendererLoadingSurfaces(Renderer &renderer, PolygonDeviceContext &context,
                                                 PolygonSurfaceRecipes &recipes)
    : renderer_(&renderer), context_(context), recipes_(recipes), images_(renderer.polygon_surface_roles())
{
    if (!empty_images(images_) || recipes_.submissions[0] || recipes_.submissions[1] || context_.capturing() ||
        context_.snapshot().scene_open)
        throw FormatError("surface adapter construction requires empty roles and idle closed context");
}
void RendererLoadingSurfaces::require_binding(const Renderer &renderer, const PolygonDeviceContext &context) const
{
    check_thread();
    if (renderer_ != &renderer || &context_ != &context)
        throw FormatError("loading surface borrow has another renderer or polygon context");
}
std::uint64_t RendererLoadingSurfaces::stage_back(const CapturedPolygonSubmission &submission, const SdrGammaRamp &ramp)
{
    // NEW HOST (R20 wide_menu): a tagged frontend recipe is rasterized from its widened
    // presentation copy; the retained recipe itself is never modified.
    const auto wide = host_wide_menu::presentation_copy(&submission);
    const auto &shown = wide ? *wide : submission;
    return fit_source_canvas_ ? renderer_->stage_polygon_back_fitted(shown, ramp)
                              : renderer_->stage_polygon_back(shown, ramp);
}
void RendererLoadingSurfaces::enable_source_canvas_fit()
{
    check_thread();
    if (context_.capturing() || context_.snapshot().scene_open)
        throw FormatError("canvas policy cannot change inside a source frame");
    fit_source_canvas_ = true;
}
bool RendererLoadingSurfaces::needs_restore(const Renderer &renderer) const
{
    require_binding(renderer, context_);
    const auto current = renderer.polygon_surface_roles();
    if (current.width != images_.width || current.height != images_.height ||
        (empty_images(current) && !empty_images(images_)))
        return true;
    if (current.front_image != images_.front_image || current.back_image != images_.back_image)
        throw FormatError("surface images changed outside their retained recipe owner");
    return back_publication_pending_;
}
void RendererLoadingSurfaces::restore_current_images(Renderer &renderer)
{
    require_binding(renderer, context_);
    if (context_.capturing() || context_.snapshot().scene_open)
        throw FormatError("surface restoration requires an idle closed source context");
    ensure_images();
    publish_pending_back();
    refresh_stale_images();
}
void RendererLoadingSurfaces::refresh_stale_images()
{
    check_thread();
    if (!front_stale_ && !back_stale_)
        return;
    if (!renderer_ || context_.capturing() || context_.snapshot().scene_open)
        throw FormatError("stale image refresh requires a bound renderer and idle closed context");
    ensure_images();
    if (front_stale_ && recipes_.submissions[0])
    {
        // Same presentation copy and canvas policy as stage_back.
        const auto wide = host_wide_menu::presentation_copy(recipes_.submissions[0].get());
        const auto &shown = wide ? *wide : *recipes_.submissions[0];
        images_.front_image = renderer_->restage_polygon_front(shown, recipes_.ramps[0], fit_source_canvas_);
        images_.active = true;
    }
    front_stale_ = false;
    if (back_stale_ && recipes_.submissions[1])
    {
        images_.back_image = stage_back(*recipes_.submissions[1], recipes_.ramps[1]);
        images_.active = true;
    }
    back_stale_ = false;
}
void RendererLoadingSurfaces::suspend_materials(MaterialD3D12 &materials)
{
    prepare_suspend();
    if (!materials.bound_to_renderer(*renderer_))
        throw FormatError("surface recovery material backend differs");
    materials.suspend_renderer_for_recreation(SourceMenuMaterialRecovery{}, recipes_.submissions);
    commit_suspend();
}
void RendererLoadingSurfaces::rebind_materials(Renderer &renderer, MaterialD3D12 &materials)
{
    prepare_bind(renderer);
    const auto images = materials.rebind_renderer(renderer, recipes_.submissions, recipes_.ramps);
    commit_bind(renderer, images);
}
void RendererLoadingSurfaces::check_thread() const
{
    if (std::this_thread::get_id() != owner_)
        throw FormatError("loading surface used outside owner thread");
}
std::optional<CapturedPolygonFrontRecipe> RendererLoadingSurfaces::capture_front_recipe() const &
{
    check_thread();
    if (!renderer_ || context_.capturing() || context_.snapshot().scene_open)
        throw FormatError("front snapshot requires a bound renderer and idle closed context");
    return diagnostics::select_polygon_front_snapshot(recipes_, renderer_->polygon_surface_roles(), images_,
                                                      context_.identity());
}
void RendererLoadingSurfaces::ensure_images()
{
    check_thread();
    if (!renderer_)
        throw FormatError("surface renderer is suspended");
    const auto current = renderer_->polygon_surface_roles();
    if (current.width != images_.width || current.height != images_.height ||
        (empty_images(current) && !empty_images(images_)))
    {
        if (!empty_images(current))
            throw FormatError("foreign images appeared during surface resize");
        // NEW resize policy: rerasterize the retained immutable commands at the
        // new target extent, preserving each old captured viewport and ramp.
        // This neither scales game time nor claims old/new pixel equivalence.
        // NEW HOST (R20 wide_menu): widened presentation copies for the new extent.
        const auto front = host_wide_menu::presentation_copy(recipes_.submissions[0].get());
        const auto back = host_wide_menu::presentation_copy(recipes_.submissions[1].get());
        images_ =
            renderer_->restore_polygon_surfaces(front ? front.get() : recipes_.submissions[0].get(), recipes_.ramps[0],
                                                back ? back.get() : recipes_.submissions[1].get(), recipes_.ramps[1]);
        back_publication_pending_ = false;  // Successful restore published both retained CPU roles.
        front_stale_ = back_stale_ = false; // and rasterized both current recipes.
        return;
    }
    if (current.front_image != images_.front_image || current.back_image != images_.back_image)
        throw FormatError("GPU image roles changed outside their actual CPU recipe adapter");
}
void RendererLoadingSurfaces::completed_back(ImmutablePolygonSubmission submission, SdrGammaRamp ramp)
{
    check_thread();
    if (!submission)
        return;
    if (back_publication_pending_)
        throw FormatError("immediate back cannot replace pending accepted back");
    if (!renderer_ || context_.capturing() || context_.snapshot().scene_open ||
        submission->device != context_.identity())
        throw FormatError("completed back requires the same idle closed context and bound renderer");
    ensure_images();
    const auto image = stage_back(*submission, ramp);
    // No allocation, query, callback or throwing operation after GPU publication.
    recipes_.submissions[1] = std::move(submission);
    recipes_.ramps[1] = ramp;
    images_.back_image = image;
    images_.active = true;
    back_stale_ = false;
}
void RendererLoadingSurfaces::completed_back_deferred_raster(ImmutablePolygonSubmission submission, SdrGammaRamp ramp)
{
    check_thread();
    if (!submission)
        return;
    if (back_publication_pending_)
        throw FormatError("immediate back cannot replace pending accepted back");
    if (!renderer_ || context_.capturing() || context_.snapshot().scene_open ||
        submission->device != context_.identity())
        throw FormatError("completed back requires the same idle closed context and bound renderer");
    ensure_images();
    // The GPU back image keeps its previous pixels and identity; the role is
    // marked stale and rasterized from this recipe before any consumer uses it.
    recipes_.submissions[1] = std::move(submission);
    recipes_.ramps[1] = ramp;
    back_stale_ = true;
}
void RendererLoadingSurfaces::invalidate_presentation_images()
{
    check_thread();
    if (context_.capturing() || context_.snapshot().scene_open)
        throw FormatError("presentation layout change requires an idle closed source context");
    if (!renderer_)
        return;
    // Same lazy path as the R22 deferred raster: refresh_stale_images() runs
    // before every consumer of the GPU images (draw, restore_current_images).
    // A pending back is staged by publish_pending_back with the new layout.
    front_stale_ = static_cast<bool>(recipes_.submissions[0]);
    back_stale_ = static_cast<bool>(recipes_.submissions[1]);
}
void RendererLoadingSurfaces::defer_completed_back(ImmutablePolygonSubmission submission, SdrGammaRamp ramp)
{
    check_thread();
    if (!submission)
        return;
    if (back_publication_pending_ || !renderer_ || context_.capturing() || context_.snapshot().scene_open ||
        submission->device != context_.identity())
        throw FormatError("deferred back requires no pending publication and same idle closed context");
    // Source seal succeeded. These assignments cannot allocate or call GPU/source.
    // Front remains the actual displayed recipe. Back is now an accepted recipe
    // awaiting publication; the flag forbids another source Flip in that state.
    recipes_.submissions[1] = std::move(submission);
    recipes_.ramps[1] = ramp;
    back_publication_pending_ = true;
}
void RendererLoadingSurfaces::publish_pending_back()
{
    check_thread();
    if (!back_publication_pending_)
        return;
    if (!renderer_ || context_.capturing() || context_.snapshot().scene_open || !recipes_.submissions[1])
        throw FormatError("pending back publication requires a bound renderer and idle closed context");
    ensure_images();
    if (!back_publication_pending_)
        return; // Resize restored both recipes already.
    const auto image = stage_back(*recipes_.submissions[1], recipes_.ramps[1]);
    // A failure above retains the accepted CPU recipe for the existing two-role
    // material recovery transaction. Do not advance cadence or resubmit source.
    images_.back_image = image;
    images_.active = true;
    back_publication_pending_ = false;
    back_stale_ = false;
}
bool RendererLoadingSurfaces::draw(Renderer &renderer)
{
    check_thread();
    if (renderer_ != &renderer)
        throw FormatError("surface refresh renderer identity differs");
    publish_pending_back();
    ensure_images();
    refresh_stale_images();
    if (!recipes_.submissions[0])
        return false;
    return renderer_->present_polygon_front();
}
void RendererLoadingSurfaces::prepare_suspend() const
{
    check_thread();
    if (!renderer_ || context_.capturing() || context_.snapshot().scene_open)
        throw FormatError("surface suspend requires a bound renderer and idle closed context");
    const auto current = renderer_->polygon_surface_roles();
    if (!empty_images(current) &&
        (current.front_image != images_.front_image || current.back_image != images_.back_image))
        throw FormatError("cannot suspend GPU image roles changed outside their CPU recipe owner");
}
void RendererLoadingSurfaces::commit_suspend() noexcept
{
    host_wide_menu::release_recycled_storage(); // NEW HOST (R30): no pooled presentation copy survives it
    renderer_ = nullptr;
    images_ = {};
    front_stale_ = back_stale_ = false;
}
void RendererLoadingSurfaces::prepare_bind(Renderer &renderer) const
{
    check_thread();
    if (renderer_ || context_.capturing() || context_.snapshot().scene_open || recipes_.submissions[0] ||
        recipes_.submissions[1])
        throw FormatError("surface bind requires CPU-only suspension and idle closed context");
    if (!empty_images(renderer.polygon_surface_roles()))
        throw FormatError("replacement renderer already owns images");
}
void RendererLoadingSurfaces::commit_bind(Renderer &renderer, PolygonSurfaceRoles images) noexcept
{
    renderer_ = &renderer;
    images_ = images;
    back_publication_pending_ = false;
    front_stale_ = back_stale_ = false;
}
std::uint32_t RendererLoadingSurfaces::surface_operation_432a60(std::uint32_t target, std::uint32_t flags)
{
    check_thread();
    if (target != 0 || flags != 1)
        return static_cast<std::uint32_t>(DDERR_INVALIDPARAMS);
    if (context_.snapshot().scene_open || back_publication_pending_)
        return PolygonDeviceContext::invalid_order;
    if (!renderer_)
        return static_cast<std::uint32_t>(DDERR_SURFACELOST);
    try
    {
        ensure_images();
        (void)renderer_->flip_polygon_surfaces();
        // EXACT synchronous commit point: the GPU role exchange returned;
        // the following paired CPU exchange and image-ID swap cannot throw.
        recipes_.exchange();
        std::swap(images_.front_image, images_.back_image);
        images_.active = true;
        std::swap(front_stale_, back_stale_); // staleness follows its recipe/image role
        return 0;
    }
    catch (const GraphicsError &error)
    {
        return static_cast<std::uint32_t>(error.result);
    }
}
std::uint32_t RendererLoadingSurfaces::begin_scene()
{
    check_thread();
    return renderer_ ? context_.begin_scene() : static_cast<std::uint32_t>(DDERR_SURFACELOST);
}
std::uint32_t RendererLoadingSurfaces::end_scene()
{
    check_thread();
    return renderer_ ? context_.end_scene() : static_cast<std::uint32_t>(DDERR_SURFACELOST);
}
std::uint32_t RendererLoadingSurfaces::finalize_423430(std::uint32_t rectangle)
{
    check_thread();
    if (rectangle != 0)
        return static_cast<std::uint32_t>(DDERR_INVALIDPARAMS);
    if (!renderer_)
        return static_cast<std::uint32_t>(DDERR_SURFACELOST);
    // This interface cannot map CPU pixels. The original423430 wrapper ignores
    // this real not-locked status, then stores its one existing finalized word.
    return static_cast<std::uint32_t>(DDERR_NOTLOCKED);
}
} // namespace sgn
