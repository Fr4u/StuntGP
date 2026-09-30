#include "sgn/session_outer_capture.hpp"
#include "sgn/race_session.hpp"
#include "sgn/session_world_materials.hpp"
#include "sgn/material_d3d12.hpp"
#include "sgn/renderer.hpp"
#include <utility>

namespace sgn::retail
{
SessionOuterLoadingDevice::SessionOuterLoadingDevice(PolygonDeviceContext &context,
                                                     LoadingFrameDevice &surfaces) noexcept
    : context_(context), surfaces_(surfaces)
{
}
std::uint32_t SessionOuterLoadingDevice::surface_operation_432a60(std::uint32_t a, std::uint32_t b)
{
    return surfaces_.surface_operation_432a60(a, b);
}
std::uint32_t SessionOuterLoadingDevice::begin_scene()
{
    return context_.begin_scene();
}
std::uint32_t SessionOuterLoadingDevice::end_scene()
{
    return context_.end_scene();
}
std::uint32_t SessionOuterLoadingDevice::finalize_423430(std::uint32_t argument)
{
    return surfaces_.finalize_423430(argument);
}

RaceSession &SessionOuterCapture::checked_session(RaceSession &session)
{
    session.shared_platform()->require_owner_thread();
    return session;
}
SessionOuterCapture::SessionOuterCapture(RaceSession &session, SessionWorldMaterials &materials,
                                         PolygonSubmissionState &scratch, PolygonDeviceContext &context,
                                         MaterialD3D12 &backend, const AssetStore &assets, LoadingFrameDevice &surfaces)
    : session_(checked_session(session)), materials_(materials), scratch_(scratch), backend_(backend),
      capture_(context, backend, materials.replacement_storage(session_)),
      textures_(assets, capture_, session_.shared_platform()), loading_device_(capture_.device(), surfaces)
{
}
RaceOverlayTextures &SessionOuterCapture::textures() & noexcept
{
    return textures_;
}
void SessionOuterCapture::release_image_resources_431810() &
{
    // Host admission before the first release: exact Session, current world
    // ticket/model bindings/publication, and the actual cold-loading backend.
    materials_.require_backend_for(session_, backend_);
    capture_.release_image_resources_431810();
}

CapturedSessionOuterFrame SessionOuterCapture::capture_source_frame(const SessionOuterCaptureRequest &request) &
{
    session_.shared_platform()->require_owner_thread();
    // Reference composition only; no new source owner, setter, upload or tick.
    SessionOuterFrameContext frame{
        session_,        materials_,      request.capabilities, scratch_,       capture_.device(), textures_,
        request.restart, request.sources, request.device_words, loading_device_};
    // Intentionally outside the cleanup scope. The pinned begin implementation
    // either throws before capture ownership or completes with nonthrowing
    // ownership assignments. A refused nested/foreign begin must not discard
    // the existing caller's capture or poison its cadence.
    capture_.begin_source_frame(request.simulation_tick, request.limits);
    try
    {
        auto source = prepare_session_outer_frame_once(frame, cadence_, request.accepted_outer_serial);
        auto submission = capture_.seal_source_frame();
        if (source.kind != SessionOuterFrameKind::rendered)
            submission.reset();
        return {request.accepted_outer_serial, std::move(source), std::move(submission)};
    }
    catch (...)
    {
        // Includes seal failure after prepare_once completed its cadence.
        // These host invalidations do not rewind source words or close a scene.
        cadence_.poison();
        detach_world_polygon_level(session_.state().polygon_frame);
        // UNCONDITIONAL after successful begin. Earlier poison can make
        // capturing()==false while an underlying device capture still exists.
        capture_.discard_failed_source_frame();
        throw;
    }
}

CapturedSessionOuterFrame SessionOuterCapture::capture_source_frame(const SessionOuterBoundCaptureRequest &request,
                                                                    LightGammaDevice *expected_gamma) &
{
    session_.shared_platform()->require_owner_thread();
    SessionOuterFrameServices sources(session_, materials_, scratch_, textures_, request.selected_depth,
                                      request.world_resources, expected_gamma);
    // One invocation-only provider, alive through the existing source call and
    // seal. The raw path supplies this capture's actual device/loading bridge.
    const SessionOuterCaptureRequest bound{request.restart,
                                           sources,
                                           sources.device_words(),
                                           sources.capabilities(),
                                           request.accepted_outer_serial,
                                           request.simulation_tick,
                                           request.limits};
    return capture_source_frame(bound);
}

void SessionOuterCapture::suspend_renderer_for_recreation(ImmutablePolygonSubmission &displayed)
{
    (void)materials_.validate_for(session_);
    capture_.suspend_renderer_for_recreation(displayed);
}
void SessionOuterCapture::rebind_renderer(Renderer &renderer, ImmutablePolygonSubmission &displayed)
{
    (void)materials_.validate_for(session_);
    capture_.rebind_renderer(renderer, displayed);
}
void SessionOuterCapture::suspend_renderer_for_recreation(std::span<ImmutablePolygonSubmission> retained)
{
    (void)materials_.validate_for(session_);
    capture_.suspend_renderer_for_recreation(retained);
}
PolygonSurfaceRoles SessionOuterCapture::rebind_renderer(Renderer &renderer,
                                                         std::array<ImmutablePolygonSubmission, 2> &retained,
                                                         const std::array<SdrGammaRamp, 2> &ramps)
{
    (void)materials_.validate_for(session_);
    return capture_.rebind_renderer(renderer, retained, ramps);
}
void present_captured_session_outer_frame(Renderer &renderer, const ImmutablePolygonSubmission &submission)
{
    if (!submission)
        throw FormatError("cannot present a missing source frame submission");
    renderer.draw_polygon_submission(*submission);
}
} // namespace sgn::retail
