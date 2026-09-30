#pragma once
#include "sgn/session_outer_frame.hpp"
#include "sgn/session_outer_frame_services.hpp"
#include "sgn/polygon_submission_capture.hpp"
#include "sgn/source_scene_reset_prefix.hpp"

namespace sgn::retail
{
struct SourceRaceOuterContext;
struct CapturedSourceRaceOuterFrame;
// New host capture wiring; recovered source behavior stays in its source ports.
// This bridge uses one device for both scene and polygon calls. The supplied
// surface receiver must implement the actual Flip/Unlock platform boundary;
// neither of its Begin/End methods is called by this bridge.
class SessionOuterLoadingDevice final : public LoadingFrameDevice
{
    PolygonDeviceContext &context_;
    LoadingFrameDevice &surfaces_;

  public:
    SessionOuterLoadingDevice(PolygonDeviceContext &, LoadingFrameDevice &) noexcept;
    SessionOuterLoadingDevice(const SessionOuterLoadingDevice &) = delete;
    SessionOuterLoadingDevice &operator=(const SessionOuterLoadingDevice &) = delete;
    std::uint32_t surface_operation_432a60(std::uint32_t, std::uint32_t) override;
    std::uint32_t begin_scene() override;
    std::uint32_t end_scene() override;
    std::uint32_t finalize_423430(std::uint32_t) override;
};

struct SessionOuterCaptureRequest
{
    PauseMenuRestartSource &restart;
    SessionOuterFrameSources &sources;
    SessionOuterDeviceWords device_words;
    SessionPolygonCapabilities capabilities;
    // Separate domains: paused source invocations can share simulation_tick.
    // Neither value is derived from physical monitor presentations here.
    std::uint64_t accepted_outer_serial;
    std::uint64_t simulation_tick;
    PolygonSubmissionCaptureLimits limits;
};
// Application route: caller supplies actual resource/capability borrows;
// this capture supplies its own Session/material/scratch/overlay/device owners.
struct SessionOuterBoundCaptureRequest
{
    PauseMenuRestartSource &restart;
    SelectedDeviceDepthBorrow selected_depth;
    std::optional<SessionOuterWorldResourceBorrow> world_resources;
    std::uint64_t accepted_outer_serial;
    std::uint64_t simulation_tick;
    PolygonSubmissionCaptureLimits limits;
};
struct CapturedSessionOuterFrame
{
    std::uint64_t accepted_outer_serial;
    SessionOuterFrameResult source;
    // Null on not_race/source_gate: keep the previous displayed submission.
    // A healthy empty capture is not permission for a new physical clear.
    ImmutablePolygonSubmission submission;
};

// Construct ONCE at cold-world initialization, before constructing any other
// race overlay owner. Session/materials/backend/context/scratch/surfaces must
// outlive this owner. All access and destruction stay on their owner thread.
// Default warm transition reaches431810,42EAA0 and pure460AF0. Active
// audio stops before native460A9C. Disabled guards permit441DC0 scene stores
// and442030 audio-pause clear.45F9D0 reaches a missing stream timer/buffer
// call or returns into460DD0. That loop either requires native sample Stop
// at460D79, preserving prior decrements, or completes before460C90's two
// stores.445FA0 then executes its reached prefix using these same Session
// and overlay owners, followed by442100 and tail4192D0. The next mandatory
// child42E6E0 remains blocked after42EA80; final4420D8 is a distinct reset if later reached.
class SessionOuterCapture final
{
    static RaceSession &checked_session(RaceSession &);
    RaceSession &session_;
    SessionWorldMaterials &materials_;
    PolygonSubmissionState &scratch_;
    MaterialD3D12 &backend_; // Same constructor borrow as capture_; never a second bank.
    // Declaration order is the lifetime contract: textures die before capture.
    PolygonSubmissionCapture capture_;
    RaceOverlayTextures textures_;
    SessionOuterLoadingDevice loading_device_;
    SessionOuterFrameCadence cadence_;

  public:
    SessionOuterCapture(RaceSession &, SessionWorldMaterials &, PolygonSubmissionState &, PolygonDeviceContext &,
                        MaterialD3D12 &, const AssetStore &, LoadingFrameDevice &surfaces);
    SessionOuterCapture(const SessionOuterCapture &) = delete;
    SessionOuterCapture &operator=(const SessionOuterCapture &) = delete;
    SessionOuterCapture(SessionOuterCapture &&) = delete;
    SessionOuterCapture &operator=(SessionOuterCapture &&) = delete;
    // Existing source startup/notice/HUD/menu calls must borrow THIS bank.
    // Never instantiate a second bank to call a different subtree.
    [[nodiscard]] RaceOverlayTextures &textures() & noexcept;
    RaceOverlayTextures &textures() && = delete;
    // Exact431810 tail route, using this Session's proven loading backend.
    // Retains CPU images/materials/fonts/overlays and all foreign snapshots.
    void release_image_resources_431810() &;
    void release_image_resources_431810() && = delete;
    // Clockless445FA0 prefix observation on this exact Session/textures_.
    // The default Warm service uses the explicit counter-bound composition.
    [[nodiscard]] SourceSceneResetResult reset_scene_prefix_445fa0() &;
    SourceSceneResetResult reset_scene_prefix_445fa0() && = delete;
    [[nodiscard]] CapturedSessionOuterFrame capture_source_frame(const SessionOuterCaptureRequest &) &;
    CapturedSessionOuterFrame capture_source_frame(const SessionOuterCaptureRequest &) && = delete;
    // expected_gamma is supplied by the owning backend, not by the request.
    [[nodiscard]] CapturedSessionOuterFrame capture_source_frame(const SessionOuterBoundCaptureRequest &,
                                                                 LightGammaDevice *expected_gamma) &;
    CapturedSessionOuterFrame capture_source_frame(const SessionOuterBoundCaptureRequest &,
                                                   LightGammaDevice *) && = delete;
    [[nodiscard]] CapturedSourceRaceOuterFrame advance_and_capture_source_frame(SourceRaceOuterContext &,
                                                                                const SessionOuterCaptureRequest &) &;
    CapturedSourceRaceOuterFrame advance_and_capture_source_frame(SourceRaceOuterContext &,
                                                                  const SessionOuterCaptureRequest &) && = delete;
    [[nodiscard]] CapturedSourceRaceOuterFrame advance_and_capture_source_frame(SourceRaceOuterContext &,
                                                                                const SessionOuterBoundCaptureRequest &,
                                                                                LightGammaDevice *expected_gamma) &;
    CapturedSourceRaceOuterFrame advance_and_capture_source_frame(SourceRaceOuterContext &,
                                                                  const SessionOuterBoundCaptureRequest &,
                                                                  LightGammaDevice *) && = delete;
    void suspend_renderer_for_recreation(ImmutablePolygonSubmission &displayed);
    void rebind_renderer(Renderer &, ImmutablePolygonSubmission &displayed);
    void suspend_renderer_for_recreation(std::span<ImmutablePolygonSubmission>);
    PolygonSurfaceRoles rebind_renderer(Renderer &, std::array<ImmutablePolygonSubmission, 2> &,
                                        const std::array<SdrGammaRamp, 2> &);
};

// Refresh entry has no Session, source callback, input, cadence or mutable bank.
// The caller retains the same immutable submission for additional refreshes.
void present_captured_session_outer_frame(Renderer &, const ImmutablePolygonSubmission &);
} // namespace sgn::retail
