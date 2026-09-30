#include "sgn/race_application_polygon_backend.hpp"
#include "sgn/material_d3d12.hpp"

namespace sgn
{
namespace
{
MaterialD3D12 &checked_backend(MaterialD3D12 &backend, Renderer &renderer)
{
    if (!backend.bound_to_renderer(renderer))
        throw FormatError("application renderer differs from physical material backend");
    return backend;
}
} // namespace
RaceApplicationPolygonBackend::RaceApplicationPolygonBackend(retail::RaceSession &session,
                                                             retail::SessionWorldMaterials &materials,
                                                             retail::PolygonSubmissionState &scratch,
                                                             PolygonDeviceContext &context, MaterialD3D12 &backend,
                                                             Renderer &renderer, const AssetStore &assets)
    : backend_(checked_backend(backend, renderer)), renderer_(&renderer), surfaces_(renderer, context, recipes_),
      outer_(session, materials, scratch, context, backend_, assets, surfaces_)
{
}
retail::RaceOverlayTextures &RaceApplicationPolygonBackend::textures() & noexcept
{
    return outer_.textures();
}
retail::LightGammaDevice &RaceApplicationPolygonBackend::gamma_device() &
{
    (void)gamma_.apply_count();
    return gamma_;
}
void RaceApplicationPolygonBackend::completed_back(ImmutablePolygonSubmission submission)
{
    if (submission)
        surfaces_.completed_back(std::move(submission), gamma_.snapshot());
}
retail::CapturedSessionOuterFrame RaceApplicationPolygonBackend::capture_source_frame(
    const retail::SessionOuterCaptureRequest &request) &
{
    if (!renderer_ || !backend_.bound_to_renderer(*renderer_))
        throw FormatError("source frame cannot run while renderer is suspended");
    auto result = outer_.capture_source_frame(request);
    completed_back(result.submission);
    return result;
}
retail::CapturedSessionOuterFrame RaceApplicationPolygonBackend::capture_source_frame(
    const retail::SessionOuterBoundCaptureRequest &request) &
{
    if (!renderer_ || !backend_.bound_to_renderer(*renderer_))
        throw FormatError("source frame cannot run while renderer is suspended");
    auto result = outer_.capture_source_frame(request, &gamma_);
    completed_back(result.submission);
    return result;
}
retail::CapturedSourceRaceOuterFrame RaceApplicationPolygonBackend::advance_and_capture_source_frame(
    retail::SourceRaceOuterContext &context, const retail::SessionOuterCaptureRequest &request) &
{
    if (!renderer_ || !backend_.bound_to_renderer(*renderer_))
        throw FormatError("source frame cannot run while renderer is suspended");
    auto result = outer_.advance_and_capture_source_frame(context, request);
    if (result.frame)
        completed_back(result.frame->submission);
    return result;
}
retail::CapturedSourceRaceOuterFrame RaceApplicationPolygonBackend::advance_and_capture_source_frame(
    retail::SourceRaceOuterContext &context, const retail::SessionOuterBoundCaptureRequest &request) &
{
    if (!renderer_ || !backend_.bound_to_renderer(*renderer_))
        throw FormatError("source frame cannot run while renderer is suspended");
    auto result = outer_.advance_and_capture_source_frame(context, request, &gamma_);
    if (result.frame)
        completed_back(result.frame->submission);
    return result;
}
void RaceApplicationPolygonBackend::prepare_source_tick()
{
    surfaces_.publish_pending_back();
}
retail::CapturedSessionOuterFrame RaceApplicationPolygonBackend::capture_source_frame_deferred(
    const retail::SessionOuterBoundCaptureRequest &request) &
{
    if (!renderer_ || !backend_.bound_to_renderer(*renderer_))
        throw FormatError("source frame cannot run while renderer is suspended");
    auto result = outer_.capture_source_frame(request, &gamma_);
    if (result.submission)
        surfaces_.defer_completed_back(result.submission, gamma_.snapshot());
    return result;
}
void RaceApplicationPolygonBackend::suspend_renderer_for_recreation()
{
    surfaces_.prepare_suspend();
    outer_.suspend_renderer_for_recreation(recipes_.submissions);
    surfaces_.commit_suspend();
    renderer_ = nullptr;
}
void RaceApplicationPolygonBackend::bind_renderer(Renderer &renderer)
{
    if (renderer_)
        throw FormatError("application renderer must be suspended before recreation");
    surfaces_.prepare_bind(renderer);
    const auto images = outer_.rebind_renderer(renderer, recipes_.submissions, recipes_.ramps);
    surfaces_.commit_bind(renderer, images);
    renderer_ = &renderer;
}
bool RaceApplicationPolygonBackend::draw(Renderer &renderer)
{
    return surfaces_.draw(renderer);
}
} // namespace sgn
