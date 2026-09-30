#pragma once
#include "sgn/polygon_submission.hpp"
#include "sgn/sdr_gamma.hpp"
#include "sgn/race_overlay_textures.hpp"

namespace sgn
{
class MaterialD3D12;
// NEW explicit empty-bank lease for pre-material4328C0 clears, not a fake texture.
struct SourceUntexturedCaptureOnly
{
};
struct PolygonSurfaceRoles;
namespace retail
{
class PhysicalMaterialStorage;
}
struct PolygonSubmissionCaptureLimits
{
    PolygonDeviceCaptureLimits total;
    std::size_t resource_segments;
};
// Exclusive owner of capture boundaries; source calls use the SAME context.
// Backend/materials/context must outlive this coordinator; this coordinator
// must outlive the RaceOverlayTextures that borrows it. Owner-thread only.
class PolygonSubmissionCapture final : public retail::TextureReplacementDevice
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    PolygonSubmissionCapture(PolygonDeviceContext &, MaterialD3D12 &, retail::PhysicalMaterialStorage &);
    PolygonSubmissionCapture(SourceUntexturedCaptureOnly, PolygonDeviceContext &, MaterialD3D12 &,
                             retail::PhysicalMaterialStorage &);
    ~PolygonSubmissionCapture();
    PolygonSubmissionCapture(const PolygonSubmissionCapture &) = delete;
    PolygonSubmissionCapture &operator=(const PolygonSubmissionCapture &) = delete;
    PolygonSubmissionCapture(PolygonSubmissionCapture &&) = delete;
    PolygonSubmissionCapture &operator=(PolygonSubmissionCapture &&) = delete;
    PolygonDeviceContext &device() noexcept;
    void begin_source_frame(std::uint64_t, PolygonSubmissionCaptureLimits);
    ImmutablePolygonSubmission seal_source_frame();
    void discard_failed_source_frame();
    bool capturing() const noexcept;
    bool poisoned() const noexcept;
    // 431810..431814 JMP431790. New D3D12 ownership translation at the
    // existing release_all boundary; no CPU image/material/transparency reset.
    // Requires a healthy, closed, idle owner and the original physical lease.
    void release_image_resources_431810();
    // New platform-only recovery at a healthy closed source boundary.
    void suspend_renderer_for_recreation(ImmutablePolygonSubmission &displayed);
    void rebind_renderer(Renderer &, ImmutablePolygonSubmission &displayed);
    void suspend_renderer_for_recreation(std::span<ImmutablePolygonSubmission>);
    PolygonSurfaceRoles rebind_renderer(Renderer &, std::array<ImmutablePolygonSubmission, 2> &,
                                        const std::array<SdrGammaRamp, 2> &);
    std::uint16_t destination_width(std::uint32_t) const override;
    void upload_to_destination(std::uint32_t, const Image &) override;
};
} // namespace sgn
