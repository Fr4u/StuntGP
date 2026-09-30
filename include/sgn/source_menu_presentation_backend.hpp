#pragma once
#include "sgn/session_outer_frame_services.hpp"
#include "sgn/polygon_submission_capture.hpp"
#include "sgn/renderer_loading_surfaces.hpp"
#include "sgn/light_gamma_d3d12.hpp"
#include "sgn/host_presentation.hpp"
#include "sgn/polygon_submission.hpp"
#include "sgn/sdr_gamma.hpp"

namespace sgn
{
// NEW HOST (interp R19): the last sealed source frame as handed to the surfaces,
// with its presentation tags. Read-only; the capture itself is never modified.
struct SourceCompletedFrame
{
    ImmutablePolygonSubmission submission;
    SdrGammaRamp gamma;
    std::uint64_t serial{};
    host_presentation::ImmutableFrameTags tags;
};
// NEW host composition only. Every renderer, material, scratch, gamma and
// front/back recipe owner is borrowed. Stable interfaces can be passed to
// menu/navigation before CreateWindow; bind only after the actual Renderer
// exists. A reached call before bind fails. No preview session is constructed.
class SourceMenuPresentationBackend final : public retail::LoadingFrameDevice,
                                            public retail::PolygonSubmissionSink,
                                            public retail::TextureReplacementDevice
{
    retail::FrontendState &frontend_;
    retail::SessionOuterFrameServices &frames_;
    struct Binding
    {
        Renderer *renderer;
        PolygonDeviceContext &context;
        MaterialD3D12 &materials;
        RendererLoadingSurfaces &surfaces;
        LightGammaD3D12 &gamma;
        PolygonSubmissionCaptureLimits limits;
    };
    std::optional<Binding> bound_;
    // One invocation coordinator, using the existing physical table. Recreate
    // its immutable lease after real source resets, never a material bank.
    std::unique_ptr<PolygonSubmissionCapture> frame_;
    bool failed_{};
    std::optional<SourceCompletedFrame> last_completed_; // NEW R19 (interp)
    std::uint64_t completed_serial_{};
    Binding &binding();
    const Binding &binding() const;
    PolygonDeviceContext &active_device();

  public:
    SourceMenuPresentationBackend(retail::FrontendState &, retail::SessionOuterFrameServices &);
    ~SourceMenuPresentationBackend() override;
    SourceMenuPresentationBackend(const SourceMenuPresentationBackend &) = delete;
    SourceMenuPresentationBackend &operator=(const SourceMenuPresentationBackend &) = delete;
    void bind(Renderer &, PolygonDeviceContext &, MaterialD3D12 &, RendererLoadingSurfaces &, LightGammaD3D12 &,
              PolygonSubmissionCaptureLimits);
    void require_binding(retail::FrontendState &, Renderer &, PolygonDeviceContext &, MaterialD3D12 &,
                         RendererLoadingSurfaces &, LightGammaD3D12 &) const;
    bool source_frame_active() const;
    // Reached431810 on departure from the completed source menu. Uses this
    // SAME live material/physical owner; no cold guard or CPU bank is reset.
    // Current admission is intentionally scene2 only; warm/race loading is separate.
    void release_cold_menu_images_431810();
    void resize_render_targets(unsigned width, unsigned height);
    void suspend_renderer_for_recreation();
    void bind_recreated_renderer(Renderer &);
    bool renderer_suspended() const;
    retail::PolygonSubmissionSink &sink() & noexcept
    {
        return *this;
    }
    retail::LoadingFrameDevice &device() & noexcept
    {
        return *this;
    }
    retail::TextureReplacementDevice &replacements() & noexcept
    {
        return *this;
    }
    // Borrow SAME gamma receiver published by bootstrap and used by loading.
    retail::LightGammaDevice &gamma_device() const &;
    // Fatal host boundary cleanup: discard capture only. No source counter,
    // Begin/End flag, material state or scene word is rewound or synthesized.
    void discard_failed_frame();
    void detach_gpu_for_shutdown(); // terminal; discards own capture and unbinds cold frames
    // NEW HOST (interp R19). Null before the first sealed frame.
    const SourceCompletedFrame *last_completed_frame() const noexcept;
    std::uint32_t surface_operation_432a60(std::uint32_t, std::uint32_t) override;
    std::uint32_t begin_scene() override;
    std::uint32_t end_scene() override;
    std::uint32_t finalize_423430(std::uint32_t) override;
    std::uint32_t set_render_state(std::uint32_t, std::uint32_t) override;
    std::uint32_t set_texture(std::uint32_t, std::uint32_t) override;
    std::uint32_t set_texture_stage_state(std::uint32_t, std::uint32_t, std::uint32_t) override;
    std::uint32_t draw_indexed(std::uint32_t, std::uint32_t, std::span<const retail::PolygonSubmissionVertex>,
                               std::span<const std::uint16_t>, std::uint32_t, const retail::ViewportState &) override;
    std::uint32_t draw(std::uint32_t, std::uint32_t, std::span<const retail::PolygonSubmissionVertex>, std::uint32_t,
                       const retail::ViewportState &) override;
    std::uint16_t destination_width(std::uint32_t) const override;
    void upload_to_destination(std::uint32_t, const Image &) override;
};
} // namespace sgn
