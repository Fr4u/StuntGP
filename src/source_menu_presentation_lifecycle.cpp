#include "sgn/source_menu_presentation_backend.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/material_d3d12.hpp"

namespace sgn
{
void SourceMenuPresentationBackend::detach_gpu_for_shutdown()
{
    frontend_.shared_platform()->require_owner_thread();
    discard_failed_frame(); // no EndScene/source rollback; context still exists
    frames_.detach_source_backend_for_shutdown();
    bound_.reset(); // references only; GPU owners remain with Production
}
void SourceMenuPresentationBackend::require_binding(retail::FrontendState &frontend, Renderer &renderer,
                                                    PolygonDeviceContext &context, MaterialD3D12 &materials,
                                                    RendererLoadingSurfaces &surfaces, LightGammaD3D12 &gamma) const
{
    const auto &b = binding();
    if (&frontend != &frontend_ || b.renderer != &renderer || &b.context != &context || &b.materials != &materials ||
        &b.surfaces != &surfaces || &b.gamma != &gamma)
        throw FormatError("D3D12 lifecycle borrows another presentation owner");
}
bool SourceMenuPresentationBackend::source_frame_active() const
{
    frontend_.shared_platform()->require_owner_thread();
    return frame_ || (bound_ && (bound_->context.capturing() || bound_->context.snapshot().scene_open));
}
bool SourceMenuPresentationBackend::renderer_suspended() const
{
    frontend_.shared_platform()->require_owner_thread();
    return bound_ && !bound_->renderer;
}
void SourceMenuPresentationBackend::release_cold_menu_images_431810()
{
    auto &b = binding(); // Same frontend/world, live renderer, context and GPU bank.
    if (source_frame_active())
        throw FormatError("431810 menu image release requires an idle closed source frame");
    // This borrow proves the original SessionWorldMaterials backend identity,
    // unchanged model handles, actual scene2 fonts and 42E772 publication.
    // A fresh local coordinator borrows that SAME physical table; it creates
    // neither a source frame nor a second GPU/material owner.
    auto &storage = frames_.source_menu_replacement_storage(b.materials);
    PolygonSubmissionCapture release(b.context, b.materials, storage);
    // The existing 431810 -> 431790 translation validates the complete material
    // lease before mutation. Immutable front/back recipes keep their own GPU
    // resources alive. No physical table/published count, font, gamma or cold admission
    // flag is cleared, and no loading/draw readiness is republished here.
    release.release_image_resources_431810();
}
void SourceMenuPresentationBackend::resize_render_targets(unsigned width, unsigned height)
{
    auto &b = binding();
    if (source_frame_active())
        throw FormatError("D3D12 resize cannot interrupt an active source frame");
    b.surfaces.enable_source_canvas_fit();
    b.renderer->resize(width, height);              // source viewport remains its logical canvas
    b.surfaces.restore_current_images(*b.renderer); // no source Flip or extra Present
}
void SourceMenuPresentationBackend::suspend_renderer_for_recreation()
{
    auto &b = binding();
    if (source_frame_active())
        throw FormatError("D3D12 recovery cannot replay an incomplete source frame");
    // Enforce the actual cold material owner and backend even when empty.
    auto &storage = frames_.source_capture_storage(b.materials);
    if (storage.size() != b.materials.resource_entries())
        throw FormatError("D3D12 recovery reached an unfinished source material transition");
    b.surfaces.suspend_materials(b.materials);
    b.renderer = nullptr; // nonthrowing after the existing CPU handoff committed
}
void SourceMenuPresentationBackend::bind_recreated_renderer(Renderer &renderer)
{
    frontend_.shared_platform()->require_owner_thread();
    if (failed_ || !bound_ || bound_->renderer || source_frame_active())
        throw FormatError("D3D12 menu rebind requires a suspended healthy source boundary");
    auto &b = *bound_;
    b.surfaces.rebind_materials(renderer, b.materials);
    b.renderer = &renderer; // surface/material transaction has committed; no throwing work follows
}
} // namespace sgn
