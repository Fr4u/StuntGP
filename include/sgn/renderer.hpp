#pragma once
#include "sgn/assets.hpp"
#include "sgn/scene.hpp"
#include "sgn/retail_projection.hpp"
#include "sgn/polygon_d3d12.hpp"
#include "sgn/sdr_gamma.hpp"
#include <memory>
#include <functional>
#include <span>
#include <string>
#include <windows.h>

namespace sgn
{
struct CapturedPolygonSubmission;
struct PolygonReprojection; // NEW HOST (R33) polygon_reprojection.hpp
struct LiveGeometry;
struct LiveInstanceView
{
    std::span<const std::array<float, 16>> model_to_view;
};
class GraphicsError : public std::runtime_error
{
  public:
    HRESULT result;
    GraphicsError(HRESULT code, const std::string &message) : std::runtime_error(message), result(code) {}
    [[nodiscard]] bool device_lost() const noexcept;
};
// NEW DXGI transport identities; never source COM addresses or copied legacy caps.
struct RendererAdapterInfo
{
    std::uint64_t luid{}, dedicated_video_bytes{}, shared_system_bytes{};
    std::uint32_t vendor_id{}, device_id{};
    bool software{};
    std::string description;
};
struct RendererDeviceInfo
{
    RendererAdapterInfo adapter;
    std::uint32_t rgba8_support{}, a1rgb5_support{}, depth32_support{}; // actual D3D12_FORMAT_SUPPORT1
    std::uint32_t feature_level{};
    bool color_target_present{}, depth_target_present{};
};
struct RenderOptions
{
    bool debug{}, warp{}, vsync{true};
    bool race_interface{}; // new native controls overlay, independent of scene simulation
    // Explicit recording mode: copy rendered pixels before flip-discard
    // presentation. Ordinary rendering performs no capture allocation/copy.
    bool capture_frames{};
    unsigned requested_samples{
        4}; // 1, 2, 4 or 8 (R23); unsupported counts fall back to the highest supported lower count
    std::optional<std::uint64_t> adapter_luid; // exact DXGI selection, retained on recovery
    // NEW HOST (R23): follow sgn::host_graphics::Settings (MSAA 1/2/4/8 clamped
    // to device support, texture filtering, mipmaps, alpha-to-coverage, FXAA).
    // Applied at construction and, once per draw at a point outside any
    // recording, whenever host_graphics::revision() changes; requested_samples
    // is then ignored. Default false keeps the exact pre-R23 renderer (tests,
    // oracles, diagnostics); the game presentation sets it.
    bool host_graphics{};
    // NEW HOST (R28b): optional text file of the pipeline states used by earlier runs
    // (read at construction and prewarmed in the background, rewritten at destruction).
    // Empty: no persistence (only states seen in this run are prewarmed).
    std::filesystem::path pipeline_state_file;
};
// NEW host snapshot of actual device/target state; no source flags or COM slots.
struct RendererHostState
{
    HRESULT device_status;
    bool targets_ready;
    bool unfenced_submission;
};
enum class RenderFailurePoint
{
    resize_buffers,
    target_allocation,
    polygon_target_allocation,
    polygon_upload_wait,
    polygon_frame_signal,
    polygon_frame_present,
    polygon_restore_back_signal
};
struct GpuDiagnostic
{
    unsigned id{}, severity{};
    std::string message;
};
struct RenderStats
{
    std::string adapter;
    unsigned width{}, height{}, samples{}, draws{}, meshes{}, instances{};
    std::uint64_t uploaded_vertex_bytes{}, uploaded_texture_bytes{};
    std::uint64_t live_vertex_bytes{}, live_updates{}, live_buffer_allocations{};
    bool tearing_supported{}, debug_active{};
    std::uint64_t presents{}, debug_errors{}, occluded_presents{};
    std::size_t pending_polygon_uploads{};
    std::uint64_t gamma_presentations{}; // Successful polygon presentations with the window-local SDR pass.
    double gpu_ms_last{};
    // NEW HOST (R23) image-quality state. Bit n of supported_samples_mask set:
    // n-sample MSAA is available for every target format (bit 1 always set).
    std::uint32_t supported_samples_mask{}, requested_samples{};
    std::uint64_t graphics_revision{}; // host_graphics revision last applied (0: not following)
    double fxaa_gpu_ms_last{};         // GPU time of the last combined FXAA+gamma pass
    bool fxaa_active{}, alpha_to_coverage_active{}, mipmaps_active{};
    // NEW HOST (R27): graphics-audit quick wins actually in effect.
    bool smooth_gradients_active{}, texture_edge_fix_active{}, linear_resolve_active{};
    std::uint32_t max_frame_latency{2}; // SetMaximumFrameLatency value (1 with low_latency)
    // NEW HOST (R28): texture pipeline v2 (applies to uploads after the change) and magnification.
    bool high_precision_textures_active{}, smooth_magnification_active{};
    // NEW HOST (R24) display-mode state. exclusive_fullscreen is the DXGI state
    // last observed (GetFullscreenState); exclusive_* describe the mode chosen
    // at the last successful exclusive entry (0 before any entry).
    bool mode_switch_enabled{}, exclusive_fullscreen{};
    unsigned exclusive_width{}, exclusive_height{};
    std::uint32_t exclusive_refresh_numerator{}, exclusive_refresh_denominator{};
    std::uint64_t fullscreen_transitions{};      // observed DXGI windowed<->fullscreen changes
    std::uint64_t fullscreen_transition_drops{}; // Presents rejected by an unrealized DXGI transition
};
struct View
{
    Vec3 eye, target;
    float near_plane{0.01f}, far_plane{10000};
    Vec3 up{0, 1, 0};
    // Row-major matrices for row-vector multiplication. The recovered camera
    // adapter supplies them without altering simulation-owned camera state.
    std::optional<std::array<float, 16>> world_to_view{}, source_projection{};
    struct RetailViewport
    {
        retail::ProjectionParameters projection;
        float clip_far{};
        bool widen_horizontal{true};
    };
    std::optional<RetailViewport> retail_viewport{};
};
// New host image roles. IDs are existing queue fence values, never source ticks.
// Back can be submitted but not yet retired. Flip waits before exposing it.
struct PolygonSurfaceRoles
{
    bool active{};
    std::uint64_t front_image{}, back_image{};
    unsigned width{}, height{};
};
enum class PolygonSurfaceFlip
{
    empty_front,
    completed_front
};
// NEW R19 (perf_gpu) host GPU services for gpu_world/interp; not recovered behaviour.
// PSOs recorded through PolygonGpuCommand must match these target formats.
struct RendererPolygonTargetFormat
{
    DXGI_FORMAT color, depth;
    DXGI_SAMPLE_DESC samples;
};
// Immutable DEFAULT-heap buffer (vertex/index/constant data uploaded once).
struct GpuStaticBuffer
{
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    D3D12_GPU_VIRTUAL_ADDRESS address{};
    std::uint64_t bytes{};
};
class Renderer
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    static std::vector<RendererAdapterInfo> enumerate_d3d12_adapters(bool software_only = false);
    RendererDeviceInfo device_info() const; // read-only query of this actual device/targets
    Renderer(HWND window, RenderOptions options);
    ~Renderer();
    // NEW read-only native identity for retained-host startup; no ownership transfer.
    HWND native_window() const noexcept;
    RenderOptions creation_options() const;
    RendererHostState host_state() const;
    void require_polygon_texture_owner(const PolygonGpuTexture &) const;
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    void resize(unsigned width, unsigned height);
    void load(const Model &model, const AssetStore &assets);
    void load_scene(std::span<const Model> models, const AssetStore &assets);
    // Modern device bindings. Indices address actual shader descriptors and
    // remain valid until the next load_scene. Uploads wait for prior GPU uses.
    unsigned append_texture(const Image &image);
    void replace_texture(unsigned native_slot, const Image &image);
    unsigned texture_index(unsigned mesh, unsigned local_texture) const;
    std::array<unsigned, 2> texture_dimensions(unsigned native_slot) const;
    Image read_texture(unsigned native_slot); // GPU readback for qualification
    // Stage CPU geometry for an existing mesh slot with unchanged texture
    // bindings. GPU transfers occur in draw_scene after that frame's fence.
    void stage_mesh(std::size_t mesh, const LiveGeometry &geometry);
    void draw(const View &view, const std::string &title, const std::string &detail, float dpi);
    void draw_scene(const View &view, std::span<const SceneInstance> instances, std::string_view title,
                    std::string_view detail, float dpi, std::span<const LiveInstanceView> live_views = {});
    // New window presentation path for completed source polygon commands.
    // Clears the modern target to black and reverse depth to zero; it applies
    // no inspection lighting, mesh transform, overlay or game update.
    void draw_polygons(const CapturedPolygonFrame &, const PolygonDeviceState &,
                       std::shared_ptr<const PolygonGpuTextures>);
    // One clear/Present for an ordered immutable multi-epoch submission.
    // All segment recordings are retained through the presentation fence.
    void draw_polygon_submission(const CapturedPolygonSubmission &);
    // Exact supplied CPU ramp, copied into this frame's GPU constants after
    // its fence. Applied after all source draws/resolve, before capture/Present.
    // SDR RGB8 UNORM only; no monitor gamma/HDR/driver-equivalence claim.
    void draw_polygon_submission(const CapturedPolygonSubmission &, const SdrGammaRamp &);
    // Phase-one GPU image primitive, not a complete DirectDraw implementation.
    // Stage executes immutable polygon commands once and bakes the supplied
    // gamma into a separate RGBA8 image. It signals its fence but never Presents.
    // Once stage/Flip is used, direct scene/polygon presentation is rejected.
    std::uint64_t stage_polygon_back(const CapturedPolygonSubmission &, const SdrGammaRamp &);
    // Explicit NEW host aspect-fit canvas for menu frames after a window resize.
    // Uses the same rasterizer/resources; leaves source viewport words untouched.
    std::uint64_t stage_polygon_back_fitted(const CapturedPolygonSubmission &, const SdrGammaRamp &);
    // NEW R22: rasterize a retained recipe into a new FRONT image (the role a
    // deferred back reached through a Flip). Same canvas policy as the back stage.
    std::uint64_t restage_polygon_front(const CapturedPolygonSubmission &, const SdrGammaRamp &, bool fit_canvas);
    // Exchanges actual image owners. NEW R19: no CPU wait for the submitted
    // back image; the GPU queue orders its completion before the present copy.
    // Empty roles are allowed by NEW cold-start policy; no black image is made.
    PolygonSurfaceFlip flip_polygon_surfaces();
    // False with no front, otherwise one copy plus the shared polygon Present.
    // No source callbacks, polygon recording or second gamma pass on refresh.
    bool present_polygon_front();
    PolygonSurfaceRoles polygon_surface_roles() const;
    // NEW transactional raster restoration, no source Flip, callbacks or Present.
    // Each recipe retains its own logical projection extent across segments;
    // use a centered aspect-fit physical GPU viewport, never edit source words.
    // Whole-target clear then captured gamma gives letterbox color gamma(0).
    // Ordinary direct draw and source-back stage keep exact-target admission.
    // Requires empty image roles. Produce both candidates, then publish both;
    // failure leaves roles empty and all queued work retained by actual Frames.
    PolygonSurfaceRoles restore_polygon_surfaces(const CapturedPolygonSubmission *front, const SdrGammaRamp &front_ramp,
                                                 const CapturedPolygonSubmission *back, const SdrGammaRamp &back_ramp);
    // Successful size changes discard both image roles. Failed ResizeBuffers
    // retains them. A replacement Renderer starts empty; pixel recovery is not
    // provided. Callers must reconcile their CPU recipe roles at these boundaries.
    PolygonGpuTexture upload_polygon_texture(std::uint32_t token, std::uint32_t width, std::uint32_t height,
                                             std::span<const std::uint16_t> a1r5g5b5);
    // NEW HOST (R28b): texture identities keep one conversion profile. A NEW
    // identity takes polygon_texture_profile() (the settings snapshot at this
    // safe point); a replacement or rebind of an existing identity passes the
    // profile recorded in its PolygonGpuTexture. The 4-argument form above is
    // the new-identity form with polygon_texture_profile().
    PolygonTextureProfile polygon_texture_profile();
    PolygonGpuTexture upload_polygon_texture(std::uint32_t token, std::uint32_t width, std::uint32_t height,
                                             std::span<const std::uint16_t> a1r5g5b5, const PolygonTextureProfile &);
    std::vector<PolygonGpuTexture> generate_polygon_textures(std::uint32_t primary_token, std::uint32_t alternate_token,
                                                             std::uint32_t dimension);
    // ---- NEW R19 (perf_gpu) host services, see COORDINATION.md ----
    RendererPolygonTargetFormat polygon_target_format() const;
    ID3D12Device &native_device() const; // same device as every polygon resource
    // One upload + queue flush: level load only, never per frame. Throws on
    // empty/oversized input or allocation failure.
    std::shared_ptr<const GpuStaticBuffer> create_static_buffer(std::span<const std::byte>,
                                                                D3D12_RESOURCE_STATES final_state);
    // Renders an immutable submission (e.g. an interpolated host frame) with
    // the supplied ramp straight to the swap chain and Presents it. Front/back
    // roles, recipes and all source-visible state are untouched. fit_canvas
    // uses the same centered aspect-fit as stage_polygon_back_fitted.
    void wait_transient_present_buffer(); // NEW R22: next back buffer free (call before sampling time)
    void present_polygon_submission_transient(const CapturedPolygonSubmission &, const SdrGammaRamp &, bool fit_canvas);
    // NEW HOST (R30 perf_renderer): the same presentation of a shared immutable
    // submission (e.g. host_presentation's ImmutablePolygonSubmission). Its CPU
    // admission (submission validation, fitted canvas) is remembered per object
    // (weak identity, the last four objects), so showing the same capture again
    // skips it; a new object (an interpolated copy) is validated in full.
    void present_polygon_submission_transient(const std::shared_ptr<const CapturedPolygonSubmission> &,
                                              const SdrGammaRamp &, bool fit_canvas);
    // NEW HOST (R33): an interpolated frame as its BASE capture plus per-slot GPU
    // motions (polygon_reprojection.hpp): the moving draws are re-projected by the
    // GPU; the same presentation path otherwise (admission remembered per base).
    void present_polygon_reprojected(const std::shared_ptr<const CapturedPolygonSubmission> &,
                                     const PolygonReprojection &, const SdrGammaRamp &, bool fit_canvas);
    // Non-blocking GPU progress (same queue fence ids as PolygonSurfaceRoles).
    std::uint64_t completed_gpu_fence() const;
    std::uint64_t last_submitted_gpu_fence() const noexcept;
    // gpu_world hybrid mode only; default false keeps the unchanged polygon VS.
    void set_polygon_precise_vertex_mapping(bool);
    // ---- NEW HOST (R32 perf-swap) in-race texture replacement ----
    // upload_polygon_texture never waits for the GPU: the copy is queued on the
    // frame queue ahead of every later list and its objects are owned until its
    // fence. retire_polygon_texture keeps a superseded texture (the resource a
    // 430D40 replacement replaced) until every submission made before the swap
    // retired (counter swap.deferred_release). Never throws.
    void retire_polygon_texture(Microsoft::WRL::ComPtr<ID3D12Resource>) noexcept;
    // Notes an identity that a 430D40 replacement writes, so race loads prewarm
    // the converted levels of its swaps (host_texture_processing).
    void note_polygon_replacement_profile(const PolygonTextureProfile &) noexcept;
    // ---- NEW HOST (R24) display modes (host_display_mode.hpp owns the policy) ----
    // Only a host_graphics renderer creates its flip-model swap chain with
    // DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH and can enter exclusive fullscreen.
    // Call these only at a safe point: on the owner thread, outside every
    // renderer call, source frame and window callback (they send window
    // messages synchronously). Both realize the transition with ResizeBuffers
    // at the new client extent (resize() semantics: a changed extent discards
    // the polygon image roles, an unchanged one keeps them).
    bool exclusive_fullscreen_supported() const noexcept;
    // Output containing the window, its native (preferred) resolution and the
    // highest refresh rate enumerated there: ResizeTarget, SetFullscreenState
    // (TRUE), ResizeTarget with a zeroed refresh rate, ResizeBuffers. Returns
    // false when DXGI reports DXGI_ERROR_NOT_CURRENTLY_AVAILABLE (no focus,
    // occluded, another exclusive application, remote session) or a transition
    // in progress: the swap chain stays windowed and the caller retries later.
    bool enter_exclusive_fullscreen();
    // SetFullscreenState(FALSE) + ResizeBuffers. False only while DXGI reports
    // a transition in progress. No-op (true) when not in exclusive fullscreen.
    bool leave_exclusive_fullscreen();
    // Actual DXGI state (GetFullscreenState); DXGI can leave on its own, e.g.
    // when the window loses focus or is occluded. Realizes such a change at an
    // unchanged extent immediately (ResizeBuffers).
    bool exclusive_fullscreen();
    void vsync(bool enabled);
    HANDLE frame_event() const;
    bool occluded() const;
    // NEW HOST (R33b): the next transient present would not block on GPU work (non-blocking fence reads).
    bool transient_present_ready() noexcept;
    // NEW HOST (R35): the swap chain's present counter and DXGI frame statistics, read now
    // (IDXGISwapChain::GetLastPresentCount + GetFrameStatistics; flip model). `present_ok`:
    // last_present is valid; `ok`: the statistics are valid (a displayed present and the
    // latest vblank: PresentCount, PresentRefreshCount, SyncRefreshCount, SyncQPCTime).
    struct DisplayStatistics
    {
        bool present_ok{}, ok{};
        std::uint32_t last_present{};
        std::uint32_t present_count{}, present_refresh{}, sync_refresh{};
        std::int64_t sync_qpc{};
    };
    DisplayStatistics display_statistics() noexcept;
    const RenderStats &stats() const;
    // Requires capture_frames and a successful draw at the current size.
    // Reads the retained pre-Present copy, never a discarded swap buffer.
    void capture(const std::filesystem::path &path);
    void finish();
    // Explicit diagnostic injection. Removes only this renderer's D3D device.
    void remove_device_for_test(bool defer_failure = false);
    void collect_diagnostics();
    void check_health() const;
    const std::vector<GpuDiagnostic> &diagnostics() const;
    void inject_debug_error_for_test();
    void inject_failure_for_test(RenderFailurePoint);
    // One synchronous callback after record/Close and before queue execution.
    // Requires debug mode; the callback must not reenter this renderer.
    void before_polygon_submit_for_test(std::function<void()>);
};
} // namespace sgn
