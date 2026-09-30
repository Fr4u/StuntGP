#pragma once
#include "sgn/host_frame_phase.hpp"
#include "sgn/polygon_d3d12.hpp"
#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <vector>

// NEW HOST (R28) post-processing of the 3D scene only (graphics audit items 6
// and 9). Not recovered game behaviour; the 2001 renderer had no post pass.
//
// HUD split: a presented race frame is split at its single world_end phase
// marker (host_frame_phase.hpp): the 3D stages are recorded, then at the
// marker the post chain runs on the finished 3D part. With FXAA the polygon
// target is resolved (MSAA) or copied (1 sample) into a private image and
// FXAA [+ bloom] is written back over the whole polygon target (every MSAA
// sample gets the post-processed pixel); with bloom alone the glow is blended
// (ONE, ONE) onto every sample. The 2D stages (HUD, score popups, screen
// trails, pause/results menus, borders) then continue on top of it WITH the
// unchanged MSAA target, depth buffer and pipelines, and the renderer's final
// resolve and SDR gamma pass run without FXAA. A frame without exactly one
// world_end marker (menus, loading, split screen, status-only views) keeps
// today's single pass (FXAA over the whole frame, no bloom).
//
// Bloom (opt-in): only the game's own additive draws glow. Draws with blending
// ONE/ONE or SRCALPHA/ONE inside the world3d/screen3d regions are recorded a
// second time (same pipelines, depth-tested against the finished 3D depth,
// depth writes off) into a black emissive target, so the emissive image is
// exactly the light those draws added. It is box-reduced to quarter
// resolution, blurred with a dual-filter (Kawase) chain over 1/4, 1/8 and 1/16
// and added to the 3D scene with a modest strength. The scene itself is never
// thresholded: art that is not drawn additively cannot glow.
//
// Owner thread only. prepare() allocates and may throw before the frame's
// command list is reset; recording never allocates GPU objects. Following the
// R28b prewarm policy no present waits for a shader compile: the pipelines of
// a target configuration (sample count, format) are built by one background
// job started on the first frame that wants a post effect; until it is adopted
// such frames keep the single pass.
namespace sgn
{
struct HostPostSegment
{
    const CapturedPolygonFrame *commands;
    const PolygonDeviceState *initial;
    std::shared_ptr<const PolygonGpuTextures> textures;
};
struct HostPostRequest
{
    bool fxaa{}, bloom{};
    unsigned width{}, height{};
    DXGI_SAMPLE_DESC samples{1, 0};
    DXGI_FORMAT color_format{DXGI_FORMAT_R8G8B8A8_UNORM};   // polygon target and its RTVs
    DXGI_FORMAT resolve_format{DXGI_FORMAT_R8G8B8A8_UNORM}; // format of the renderer's MSAA resolve
    std::optional<PolygonLogicalRasterExtent> canvas;       // the frame's retained canvas, if any
    PolygonRasterQuality quality;                           // the polygon backend's current quality
    bool precise_vertex_mapping{};                          // the polygon backend's VS variant
};
// The polygon render target of this frame, bound (with dsv) when the marker runs.
// The renderer's R27 linear-light MSAA resolve (root, pipeline, and the heap
// whose first view is the MSAA colour as Texture2DMS; b0 word 0 = sample
// count). When set it also resolves the FXAA input, so the 3D part is resolved
// exactly like the final image; empty: fixed-function ResolveSubresource.
struct HostPostLinearResolve
{
    ID3D12RootSignature *root{};
    ID3D12PipelineState *pipeline{};
    ID3D12DescriptorHeap *srvs{};
};
struct HostPostTargets
{
    ID3D12Resource *color; // MSAA polygon color (samples > 1) or the swap-chain back buffer (1 sample)
    D3D12_CPU_DESCRIPTOR_HANDLE rtv, dsv;
    HostPostLinearResolve linear_resolve{};
};
inline constexpr float host_bloom_strength = 0.75f; // applied to the mean of the three blur levels

class HostPostProcessD3D12 final : host_frame_phase::MarkerRecorder
{
    struct Impl;
    std::unique_ptr<Impl> impl_;
    void record_marker(host_frame_phase::Marker, const PolygonGpuRecordContext &) override;

  public:
    // timestamp_frequency: ID3D12CommandQueue::GetTimestampFrequency of the queue
    // that executes the lists (GPU timings "gpu.hud_split_execution",
    // "gpu.bloom_execution" in the SGN_PROFILE report).
    HostPostProcessD3D12(ID3D12Device &, std::uint64_t timestamp_frequency);
    ~HostPostProcessD3D12();
    HostPostProcessD3D12(const HostPostProcessD3D12 &) = delete;
    HostPostProcessD3D12 &operator=(const HostPostProcessD3D12 &) = delete;
    // Before the frame's command list is reset. True when this frame will be
    // split (exactly one world_end marker and FXAA on, or bloom on with at
    // least one additive draw). Allocates targets/pipelines and builds the
    // emissive streams; throws on failure without touching any list.
    bool prepare(std::span<const HostPostSegment>, const HostPostRequest &);
    // Renderer::start_prewarm (R28b): starts, never waits for, the background
    // build of this target configuration's pipelines and, with bloom, the
    // emissive recorder's pipelines. Failures only disable the split.
    void prewarm(DXGI_SAMPLE_DESC samples, DXGI_FORMAT color_format, const PolygonRasterQuality &quality,
                 bool bloom) noexcept;
    // NEW HOST (R35): starts (never waits for) the background creation of this configuration's
    // size-dependent targets (scene copy; with bloom the emissive target, its resolve and the levels),
    // which the first frame that needs them then takes instead of creating them.
    void prewarm_targets(unsigned width, unsigned height, DXGI_SAMPLE_DESC samples, DXGI_FORMAT color_format,
                         DXGI_FORMAT resolve_format, bool bloom) noexcept;
    // Recording scope of one prepared frame: from binding the polygon target
    // until the last segment was recorded. The destructor abandons an
    // unfinished scope (exception path; its list is never executed).
    class Recording
    {
        HostPostProcessD3D12 *owner_;

      public:
        Recording(HostPostProcessD3D12 &, ID3D12GraphicsCommandList &, unsigned context_slot,
                  const HostPostTargets &) noexcept;
        ~Recording();
        Recording(const Recording &) = delete;
        Recording &operator=(const Recording &) = delete;
        // Appends the recordings that keep this frame's post objects alive
        // through the submission fence. True when the split ran: the 3D part
        // is post-processed and the final gamma pass must not apply FXAA again.
        bool finish(std::vector<PolygonGpuRecording> &retained);
    };
    // After the submission of context_slot retired (Renderer::acquire_context).
    void retire_context(unsigned context_slot) noexcept;
};
} // namespace sgn
