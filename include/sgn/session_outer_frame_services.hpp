#pragma once
#include "sgn/session_outer_source_bindings.hpp"
#include <optional>
#include "sgn/source_next_scene_adapter_identity.hpp"

namespace sgn::retail
{
class FrontendState;
class SourceApplicationCore;
struct LoadingPresentationWorld;
// NEW host qualification metadata. Unimplemented gamma is not evidence of
// the source null-device branch4320C4..4320D1. No implicit selection is made.
enum class SessionOuterGammaBinding
{
    unresolved,
    source_device_absent,
    source_device_present
};

// A current resource VIEW, not an owner or an inferred resource producer.
// The lvalue aggregate and every referent must belong to this same Session
// generation and remain valid through the synchronous child. Publish fresh
// spans into current only BETWEEN children; never invalidate a live borrow.
// gamma_binding is fixed for this invocation; pointer/state must agree.
struct SessionOuterWorldResourceBorrow
{
    RaceSession &session;
    SessionWorldFrameResources &current; // non-const lvalue prevents a temporary
    SessionOuterGammaBinding gamma_binding;
};

// Concrete invocation-only wiring. Source storage stays in the Session and
// the SAME material/scratch/overlay/device owners used by SessionOuterCapture.
// The capture overload in capture-binding.patch constructs this class inside
// that owner, preventing a foreign scratch/texture bank in the application path.
class SessionOuterFrameServices final : public SessionOuterFrameSources
{
    friend class SourceApplicationCore;
    // NEW synchronous permission opened only by this same Core around431300.
    // It identifies the current material epoch; it does not admit menu drawing,
    // fonts, replacement uploads or later42C510. Cleared on return and failure.
    std::optional<std::uint64_t> source_renderer_reset_epoch_;
    // Separate Core-only synchronous42C510/445A40 scope. Allows the SAME
    // table to be reset/repopulated; does not admit menu/fonts/replacements.
    std::optional<std::uint64_t> source_material_loading_epoch_;
    // Core commits a fresh weak identity only after its whole scene tail. This
    // never grants reset/loading scopes permission to borrow fonts or UI data.
    std::optional<SourceNextSceneAdapterIdentity> source_next_scene_;
    static RaceSession &checked_session(RaceSession &, const RaceOverlayTextures &);
    RaceSession *session_{};
    FrontendState *frontend_{};
    SessionWorldMaterials *source_materials_{};
    MaterialTextureBackend *source_backend_{};
    bool source_backend_detached_{}; // terminal cold host borrow lifetime
    const SessionWorldMaterials &materials_;
    PolygonSubmissionState &scratch_;
    RaceOverlayTextures *textures_{};
    std::optional<SelectedDeviceDepthBorrow> selected_depth_;
    SelectedDeviceDepthBorrow selected_depth() const;
    std::optional<SessionOuterWorldResourceBorrow> world_resources_;
    // Authoritative capture-backend identity, not a caller-supplied resource
    // declaration. Null is only compatible with qualified source absence.
    LightGammaDevice *expected_gamma_;
    // Reference-bearing AudioDriver must outlive every returned HUD view.
    // No local AudioDriver may be bound to RaceHudSubmission::audio.
    std::optional<AudioDriver> audio_;
    RaceSessionState &world() const;
    RaceSession &race_session() const;
    const WorldMaterialLoadingState &drawing_materials() const;
    LoadingFrameWorld loading_frame(const WorldMaterialLoadingState &);

    TextSubmission borrow_text();
    std::span<PolygonSubmissionVertex, 4> borrow_vertices();

  public:
    // No source call, validation of drawing resources, allocation or reset.
    // std::nullopt explicitly means no world producer: the world getter will
    // reject it. UI/demo/device getters do not silently construct world inputs.
    SessionOuterFrameServices(RaceSession &, const SessionWorldMaterials &, PolygonSubmissionState &,
                              RaceOverlayTextures &, SelectedDeviceDepthBorrow,
                              std::optional<SessionOuterWorldResourceBorrow>, LightGammaDevice *expected_gamma);
    // Cold-menu composition is inert: it needs neither a RaceSession nor a
    // Renderer before CreateWindow. Bind the actual backend after it exists.
    SessionOuterFrameServices(FrontendState &, SessionWorldMaterials &, PolygonSubmissionState &,
                              SelectedDeviceDepthBorrow);
    SessionOuterFrameServices(FrontendState &, SessionWorldMaterials &, PolygonSubmissionState &);
    void bind_source_depth(SelectedDeviceDepthBorrow);
    void bind_source_backend(MaterialTextureBackend &, LightGammaDevice &);
    void detach_source_backend_for_shutdown(); // host-only; clears stored borrows, no source unload
    void require_source_frontend(const FrontendState &) const;
    // These borrows identify this ONE private material state, even during
    // 42C510 before fonts/count publication. They confer no menu readiness.
    WorldMaterialLoadingState &source_loading_state(const MaterialTextureBackend &) const;
    std::uint32_t &source_loading_shown_word(WorldMaterialLoadingState &, const MaterialTextureBackend &) const;
    std::uint32_t &source_loading_tile_base(WorldMaterialLoadingState &, const MaterialTextureBackend &) const;
    LoadingPresentationWorld borrow_source_loading_presentation(WorldMaterialLoadingState &,
                                                                const MaterialTextureBackend &);
    LoadingFrameWorld borrow_source_loading_frame(WorldMaterialLoadingState &, const MaterialTextureBackend &);
    PhysicalMaterialStorage &source_capture_storage(const MaterialTextureBackend &) const;
    PhysicalMaterialStorage &source_menu_replacement_storage(const MaterialTextureBackend &) const;
    SessionOuterFrameServices(const SessionOuterFrameServices &) = delete;
    SessionOuterFrameServices &operator=(const SessionOuterFrameServices &) = delete;
    SessionOuterFrameServices(SessionOuterFrameServices &&) = delete;
    SessionOuterFrameServices &operator=(SessionOuterFrameServices &&) = delete;

    SessionWorldFrameResources borrow_world_resources() override;
    RaceHudSubmission borrow_countdown() override;
    PauseAgeFadeWorld borrow_age_fade() override;
    LoadingFrameWorld borrow_loading_frame() override;
    DemoTailWords borrow_demo_words() override;
    SessionOuterDeviceWords device_words();
    SessionPolygonCapabilities capabilities();
};
} // namespace sgn::retail
