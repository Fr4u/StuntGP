#pragma once
#include "sgn/session_outer_capture.hpp"
#include "sgn/source_outer_scheduler.hpp"
#include "sgn/light_gamma_d3d12.hpp"
#include "sgn/renderer_loading_surfaces.hpp"

namespace sgn {
// New application composition, not recovered control flow. All borrowed
// owners preexist, are initialized once and outlive this object. This object
// owns the existing outer/capture/overlay composition and platform gamma/display state.
class RaceApplicationPolygonBackend final {
    MaterialD3D12& backend_;
    Renderer* renderer_;
    LightGammaD3D12 gamma_; //Same CPU owner through Renderer loss/rebind.
    PolygonSurfaceRecipes recipes_; //One paired CPU owner, front/back.
    RendererLoadingSurfaces surfaces_; //Stable; borrows recipes_ and the same context.
    retail::SessionOuterCapture outer_; //Dies before its actual surface adapter.
    void completed_back(ImmutablePolygonSubmission);
public:
    RaceApplicationPolygonBackend(retail::RaceSession&,retail::SessionWorldMaterials&,
        retail::PolygonSubmissionState&,PolygonDeviceContext&,MaterialD3D12&,Renderer&,
        const AssetStore&);
    RaceApplicationPolygonBackend(const RaceApplicationPolygonBackend&)=delete;
    RaceApplicationPolygonBackend& operator=(const RaceApplicationPolygonBackend&)=delete;
    retail::RaceOverlayTextures& textures() & noexcept;
    retail::LightGammaDevice& gamma_device() &;
    retail::LightGammaDevice& gamma_device() &&=delete;
    retail::CapturedSessionOuterFrame capture_source_frame(const retail::SessionOuterCaptureRequest&) &;
    retail::CapturedSessionOuterFrame capture_source_frame(const retail::SessionOuterBoundCaptureRequest&) &;
    // Preview caller only: flush before mutating input/world/audio, then retain
    // a successfully sealed result without performing fallible GPU publication.
    void prepare_source_tick();
    retail::CapturedSessionOuterFrame capture_source_frame_deferred(const retail::SessionOuterBoundCaptureRequest&) &;
    retail::CapturedSourceRaceOuterFrame advance_and_capture_source_frame(
        retail::SourceRaceOuterContext&,const retail::SessionOuterCaptureRequest&) &;
    retail::CapturedSourceRaceOuterFrame advance_and_capture_source_frame(
        retail::SourceRaceOuterContext&,const retail::SessionOuterBoundCaptureRequest&) &;
    // Called BEFORE the old Renderer is destroyed. This cannot recover a
    // partially executed source frame or reset a poisoned outer cadence.
    void suspend_renderer_for_recreation();
    void bind_renderer(Renderer&);
    // False means no selected front exists; no physical Present is issued.
    bool draw(Renderer&); //May rerasterize both immutable recipes after a size change.
    std::optional<CapturedPolygonFrontRecipe> capture_displayed_recipe() const &{return surfaces_.capture_front_recipe();}
    std::optional<CapturedPolygonFrontRecipe> capture_displayed_recipe() const &&=delete;
    const ImmutablePolygonSubmission& displayed() const & noexcept{return recipes_.submissions[0];}
    const ImmutablePolygonSubmission& displayed() const &&=delete;
};
}
