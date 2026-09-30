#pragma once
#include "sgn/polygon_command_capture.hpp"
#include <d3d12.h>
#include <wrl/client.h>
#include <optional>
#include <span>

namespace sgn
{
// NEW HOST descriptor budgets. Each draw binds two SRVs; sampler pairs are
// interned by their complete descriptor bits and share one shader-visible heap.
inline constexpr std::size_t polygon_gpu_srv_draw_limit = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1 / 2u;
inline constexpr std::size_t polygon_gpu_sampler_pair_limit = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE / 2u;
// New platform state. No implicit retail device defaults: initialization must
// provide every state consumed by a draw, through the actual source API trace.
struct PolygonDeviceState
{
    std::array<std::optional<std::uint32_t>, 256> render;
    std::array<std::array<std::optional<std::uint32_t>, 32>, 2> stage;
    std::array<std::optional<std::uint32_t>, 2> texture;
};
struct PolygonTextureCreationEvidence
{
    std::uint32_t descriptor_flags{}, texture_stage{};
    std::uint8_t transparency_byte{};
};
// New immutable CPU recovery payload, shared by the existing GPU version and
// its captured banks. This is not a second source material/image registry.
struct PolygonTexturePixels
{
    std::uint32_t width{}, height{};
    std::vector<std::uint16_t> words;
};
// NEW HOST (R28b) conversion profile of ONE texture identity (token). It is
// fixed when the identity is created (material/font upload or 430730
// generation) from the settings snapshot at that moment, and EVERY later
// literal-word upload of that identity (430D40 replacement, device-loss
// rebind) reuses it. So a replacement always gets the same GPU format and the
// same upscale limit as its original; a mid-session settings change reaches a
// texture only at its next full load (a new identity).
struct PolygonTextureProfile
{
    bool high_precision{}; // B8G8R8A8: R28 8-bit expansion, gradient deband, 8-bit mips
    bool edge_fix{};       // R27 colour fill of alpha-0 texels
    bool mipmaps{};        // R23 mip chain (skipped only if the format lacks MIP support)
    // R28 sharp HUD: the largest 2D upscale factor (1, 2 or 4) for this identity's
    // content; 1 = never upscaled. 2D-classified content is scaled by the largest
    // of 2/4 not above it within 16384 texels, other content stays literal.
    std::uint8_t upscale{1};
    bool operator==(const PolygonTextureProfile &) const = default;
};
// The GPU resource format a profile produces from A1R5G5B5 words.
constexpr DXGI_FORMAT polygon_profile_format(const PolygonTextureProfile &profile) noexcept
{
    return profile.high_precision ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_B5G5R5A1_UNORM;
}
struct PolygonGpuTexture
{
    std::uint32_t token;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    DXGI_FORMAT format;
    // Optional retained source creation metadata. Its presence does not imply
    // complete legacy-driver color-key semantics in the modern rasterizer.
    std::optional<PolygonTextureCreationEvidence> source_creation;
    std::shared_ptr<const PolygonTexturePixels> recovery;
    // NEW HOST (R28b): conversion profile of this identity; `format` is the
    // actual GPU resource format, always polygon_profile_format(profile).
    PolygonTextureProfile profile{};
    // NEW HOST (R28b): the 2D upscale applied to THIS version (1 = literal;
    // otherwise 2 or 4, never above profile.upscale). `recovery` always keeps the
    // source words and extent; the resource is `scale` times that extent.
    std::uint8_t scale{1};
};
// NEW HOST (R28b): a texture whose recorded format, profile, scale and resource
// agree: the resource has polygon_profile_format(profile) and is exactly `scale`
// times the retained source extent (recovery). Replaces direct GPU-extent ==
// source-extent comparisons in the material, font, replacement and rebind checks.
bool polygon_texture_consistent(const PolygonGpuTexture &) noexcept;
// Resources must already be in PIXEL_SHADER_RESOURCE state. These are actual
// GPU objects, not borrowed source material records or inspection mesh slots.
struct PolygonGpuTextures
{
    std::uint64_t epoch;
    std::vector<PolygonGpuTexture> entries;
};
struct PolygonTextureUpload
{
    PolygonGpuTexture texture;
    Microsoft::WRL::ComPtr<ID3D12Resource> staging;
};
class PolygonGpuCommand;
class PolygonTransientArena;
struct PolygonReprojection; // NEW HOST (R33), polygon_reprojection.hpp
struct PolygonGpuRecording
{
    std::uint64_t source_tick{}, resource_epoch{};
    std::size_t draws{}, vertex_bytes{}, index_bytes{};
    PolygonDeviceState final_state;
    // Command lists and descriptors do not retain these COM objects. Own each
    // actual referenced object even if the backend or caller's bank is deleted.
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> retained_textures;
    // NEW HOST (R30 perf_renderer): the texture bank of a PolygonD3D12 recording.
    // Every texture its list references is an entry of this immutable bank, which
    // owns them all (one reference instead of one per used texture).
    std::shared_ptr<const PolygonGpuTextures> retained_bank;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> retained_root;
    std::vector<Microsoft::WRL::ComPtr<ID3D12PipelineState>> retained_pipelines;
    Microsoft::WRL::ComPtr<ID3D12Resource> upload;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptors, samplers;
    // NEW HOST (R19): inserted host GPU commands (and the buffers they own) that this
    // list references; gpu_draw_calls is a diagnostic count after batching.
    std::vector<std::shared_ptr<const PolygonGpuCommand>> retained_commands;
    std::shared_ptr<PolygonTransientArena> retained_arena; // standalone record() only
    std::size_t gpu_draw_calls{};
};
// NEW HOST (R23) image-quality policy of the polygon rasterizer. Not recovered
// behaviour: the default value is the exact pre-R23 path (original D3D7 stage
// filters, single-level uploads, hard alpha-test discard). The Renderer maps
// sgn::host_graphics::Settings onto it; no simulation or source-visible value
// depends on it.
struct PolygonRasterQuality
{
    enum class Filter : std::uint8_t
    {
        original = 0,
        trilinear = 1,
        anisotropic = 2
    };
    // original: the exact D3D7 MAG/MIN/MIPFILTER mapping. trilinear/anisotropic
    // upgrade only stages whose game state requests LINEAR min AND mag; POINT
    // requests (HUD/text) keep their exact point descriptor.
    Filter filter{Filter::original};
    std::uint32_t anisotropy{1}; // 1..16, used by Filter::anisotropic
    // Full CPU-built mip chains for A1R5G5B5 uploads (linear-light box filter,
    // coverage-preserving 1-bit alpha). Generated (unspecified-content)
    // textures always keep one level.
    bool generate_mipmaps{};
    // Alpha-tested, non-blended draws use AlphaToCoverage with a sharpened
    // coverage alpha instead of discard. Effective only with >1 samples.
    bool alpha_to_coverage{};
    // NEW HOST (R27): before upload (and before mip generation) the colour of
    // every alpha-0 A1R5G5B5 texel is filled from its nearest opaque texels by
    // iterative 8-neighbour dilation; alpha bits are unchanged. Bilinear and
    // mip filtering then no longer pull the (black) key colour into cut-out
    // edges. The literal words stay the recovery source (same result on rebind).
    bool texture_edge_fix{};
    // NEW HOST (R28): A1R5G5B5 uploads become B8G8R8A8 (exact bit replication,
    // smooth 5-bit gradients debanded, 8-bit mips) when the device samples it.
    bool high_precision_textures{};
    // NEW HOST (R28): -3..3, higher = sharper; MipLODBias = -0.25 * value on the
    // upgraded (trilinear/anisotropic) samplers only.
    std::int32_t texture_sharpness{};
    // NEW HOST (R28): Catmull-Rom reconstruction of magnified LINEAR-sampled
    // world textures (perspective draws only; never point-sampled or screen-flat 2D).
    bool smooth_magnification{};
    bool operator==(const PolygonRasterQuality &) const = default;
};
// NEW HOST canvas for explicit retained-image restoration only. Values are
// captured projection dimensions, never a replacement source viewport/state.
struct PolygonLogicalRasterExtent
{
    std::uint32_t width{}, height{};
    bool operator==(const PolygonLogicalRasterExtent &) const = default;
};
// NEW HOST (R19) storage (perf_gpu), not recovered behaviour. One arena per caller
// command context: a persistently mapped upload ring plus shader-visible
// descriptor heaps. Memory handed out stays valid until reset(); the caller
// calls reset() only after the submission fence of every list that used it
// retired. Allocation throws GraphicsError-free std::runtime_error on failure.
struct PolygonGpuUpload
{
    std::byte *cpu{};
    D3D12_GPU_VIRTUAL_ADDRESS gpu{};
    ID3D12Resource *resource{};
};
struct PolygonGpuDescriptors
{
    ID3D12DescriptorHeap *heap{};
    D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
    UINT step{};
};
class PolygonTransientArena
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    explicit PolygonTransientArena(ID3D12Device &, std::size_t page_bytes = 4u * 1024u * 1024u);
    ~PolygonTransientArena();
    PolygonTransientArena(const PolygonTransientArena &) = delete;
    PolygonTransientArena &operator=(const PolygonTransientArena &) = delete;
    void reset() noexcept;
    PolygonGpuUpload allocate(std::size_t bytes, std::size_t alignment = 256);
    // Contiguous shader-visible ranges; heaps can be replaced when full (the
    // replaced heap stays alive until reset()). Bind the returned heaps.
    PolygonGpuDescriptors srvs(std::size_t count);
    PolygonGpuDescriptors samplers(std::size_t count);
    // Objects to retain in a recording (lifetime owners of the ranges above).
    Microsoft::WRL::ComPtr<ID3D12Resource> page(const PolygonGpuUpload &) const;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap(const PolygonGpuDescriptors &) const;
};
// NEW HOST (R19) GPU command inserted into the ordered polygon stream (see
// CapturedPolygonFrame::gpu_insertions). It records into the CURRENT polygon
// render/depth target at its exact stream position. It may bind its own root
// signature, PSO, heaps, buffers and draw; it must not change render targets,
// Close/Reset the list or submit. The recorder rebinds its own state after it.
// Plan of a PLANNED insertion. `pipeline` is the complete polygon PSO
// description for this state (polygon root signature, VS, PS, input layout,
// blend/depth/raster/sample). Replace VS/InputLayout/pRootSignature only; the
// replacement root signature must keep b0 = 24 root constants (visibility
// ALL), t0-t1 SRV table and s0-s1 sampler table (PIXEL). Cache PSOs by
// pipeline_key. Descriptor heaps are bound; rebind constants/tables after
// SetGraphicsRootSignature. PS == the polygon PS, byte for byte.
// NEW HOST (R23): pipeline_key also encodes the sample count and the
// alpha-to-coverage variant (whose PS/blend state differ), so a key-indexed
// cache never returns a PSO built for another MSAA configuration.
struct PolygonGpuDrawState
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline{};
    std::uint64_t pipeline_key{};
    std::array<std::uint32_t, 24> constants{};
    D3D12_GPU_DESCRIPTOR_HANDLE textures{}, samplers{};
    D3D12_PRIMITIVE_TOPOLOGY topology{};
};
struct PolygonGpuRecordContext
{
    ID3D12GraphicsCommandList &list;
    ID3D12Device &device;
    PolygonTransientArena &transient;
    DXGI_FORMAT color_format, depth_format;
    DXGI_SAMPLE_DESC samples;
    D3D12_VIEWPORT viewport; // physical (possibly aspect-fit) viewport of the logical canvas
    D3D12_RECT scissor;
    PolygonLogicalRasterExtent logical;
    std::uint64_t source_tick, resource_epoch;
    const PolygonDeviceState &state; // source device state at this stream position
    const PolygonGpuTextures &textures;
    const PolygonGpuDrawState *draw; // non-null for a planned insertion
    // NEW HOST (R20) canvas (host_wide_layout.hpp): the recorder adds this
    // binary32 offset to every captured X/Y before the VS mapping, whose size
    // constants (b0 words 0/1) are then the canvas extent. An insertion that
    // produces its own screen XY must add the same offset. 0 in 4:3 mode.
    float canvas_offset_x{}, canvas_offset_y{};
};
class PolygonGpuCommand
{
  public:
    virtual ~PolygonGpuCommand() = default;
    virtual void record(const PolygonGpuRecordContext &) const = 0;
};
// Records pretransformed FVF2C4 geometry without invoking game code. The caller
// owns render targets, viewport dimensions, barriers, submission and fencing.
// Keep the returned recording alive until its GPU fence completes. Repeated
// presentation uses the same immutable capture AND the same initial state.
class PolygonD3D12
{
    struct Impl;
    std::unique_ptr<Impl> impl_;
    struct Reproject; // NEW HOST (R33)
    PolygonGpuRecording record_frame(ID3D12GraphicsCommandList &, PolygonTransientArena &, const CapturedPolygonFrame &,
                                     const PolygonDeviceState &, std::shared_ptr<const PolygonGpuTextures>,
                                     std::uint32_t, std::uint32_t, std::optional<PolygonLogicalRasterExtent>,
                                     const Reproject *);

  public:
    PolygonD3D12(ID3D12Device &, DXGI_FORMAT color, DXGI_FORMAT depth, DXGI_SAMPLE_DESC);
    ~PolygonD3D12();
    PolygonD3D12(const PolygonD3D12 &) = delete;
    PolygonD3D12 &operator=(const PolygonD3D12 &) = delete;
    PolygonGpuRecording record(ID3D12GraphicsCommandList &, const CapturedPolygonFrame &, const PolygonDeviceState &,
                               std::shared_ptr<const PolygonGpuTextures>, std::uint32_t target_width,
                               std::uint32_t target_height,
                               std::optional<PolygonLogicalRasterExtent> retained_canvas = std::nullopt);
    // NEW HOST (R19) hot path: identical validation/exceptions and identical pixels,
    // but transient upload/descriptors come from the caller's context arena,
    // descriptors are interned per texture pair, redundant state is filtered
    // and consecutive draws with identical state are merged (order preserved).
    // NEW HOST (R30 perf_renderer): texture views and samplers are cached in
    // CPU-only heaps (per resource / per exact sampler bits) and copied into the
    // arena tables; token, pair, sampler and PSO lookups use flat indexes; the
    // recording retains its bank (retained_bank). Same validation, same pixels.
    PolygonGpuRecording record(ID3D12GraphicsCommandList &, PolygonTransientArena &, const CapturedPolygonFrame &,
                               const PolygonDeviceState &, std::shared_ptr<const PolygonGpuTextures>,
                               std::uint32_t target_width, std::uint32_t target_height,
                               std::optional<PolygonLogicalRasterExtent> retained_canvas = std::nullopt);
    // NEW HOST (R33) GPU re-projection (polygon_reprojection.hpp). `frame` must be
    // segment `segment` of reprojection.base->submission. The segment's plan (the
    // same validation, states, PSOs, texture/sampler tables and batching as
    // record()), its vertex payload, slot stream and indices are kept on the GPU per
    // base capture and target configuration; later calls with the same base write
    // only descriptors, the slot motions and the commands. Draws with a moving slot
    // use the re-projection VS (same PS, same state); all others are recorded
    // exactly as record() records them.
    PolygonGpuRecording record_reprojected(ID3D12GraphicsCommandList &, PolygonTransientArena &,
                                           const CapturedPolygonFrame &, const PolygonDeviceState &,
                                           std::shared_ptr<const PolygonGpuTextures>, std::uint32_t target_width,
                                           std::uint32_t target_height,
                                           std::optional<PolygonLogicalRasterExtent> retained_canvas,
                                           const PolygonReprojection &, std::size_t segment);
    // The draws of `frame` are copies of `sources` (draws of reprojection.base, in
    // order; e.g. the bloom emissive stream). Planned and uploaded per call like
    // record(), re-projected like their originals.
    PolygonGpuRecording record_reprojected_copy(ID3D12GraphicsCommandList &, PolygonTransientArena &,
                                                const CapturedPolygonFrame &, const PolygonDeviceState &,
                                                std::shared_ptr<const PolygonGpuTextures>, std::uint32_t target_width,
                                                std::uint32_t target_height,
                                                std::optional<PolygonLogicalRasterExtent> retained_canvas,
                                                const PolygonReprojection &,
                                                std::span<const CapturedPolygonDraw *const> sources);
    ID3D12Device &device() const noexcept;
    // NEW HOST (R19) opt-in (gpu_world hybrid mode): compile/use the polygon VS with a
    // `precise` XYZRHW mapping. Default false keeps the unchanged shader bytes.
    void set_precise_vertex_mapping(bool);
    bool precise_vertex_mapping() const noexcept;
    // NEW HOST (R23). Quality applies to the next record()/upload. A new sample
    // description (MSAA change) drops the PSO cache; recordings keep retaining
    // the PSOs they reference. The caller recreates its targets with the same
    // description before recording again. Neither call records GPU commands.
    void set_raster_quality(const PolygonRasterQuality &);
    const PolygonRasterQuality &raster_quality() const noexcept;
    // NEW HOST (R28b): profile for a NEW texture identity from the current
    // quality and the device's format support (high precision only where
    // B8G8R8A8 can be sampled).
    PolygonTextureProfile texture_profile() const;
    // NEW HOST (R28b) pipeline prewarm. Every pipeline state the recorder has
    // used is remembered (quality-independent: its base key plus whether it could
    // take the alpha-to-coverage / smooth-magnification variants). prewarm starts
    // ONE background job (std::async; ID3D12Device and D3DCompile are
    // free-threaded) that compiles the PS variants `quality` needs and creates
    // the PSO of every remembered state for `samples` and `quality`. It never
    // blocks; a job for another target keeps running and is adopted when done.
    // pipelines_prewarmed() adopts finished jobs (render thread, outside any
    // recording) and is true once the job for this exact target has been
    // adopted. PSOs are kept per sample count, so an MSAA change back and forth
    // needs no rebuild. A state first used after its prewarm is still created
    // on first use (the pre-R28b behaviour).
    void prewarm_pipelines(DXGI_SAMPLE_DESC, const PolygonRasterQuality &);
    bool pipelines_prewarmed(DXGI_SAMPLE_DESC, const PolygonRasterQuality &);
    // Remembered states, encoded for a cache file, and their validated re-entry
    // (malformed codes are skipped, like unknown keys in the settings file).
    std::vector<std::uint64_t> pipeline_states() const;
    void remember_pipeline_states(std::span<const std::uint64_t>);
    void set_sample_desc(DXGI_SAMPLE_DESC);
    DXGI_SAMPLE_DESC sample_desc() const noexcept;
    // Default admission remains exact physical target projection. An explicit
    // retained canvas requires every draw to match it and uses a centered
    // aspect-fit GPU viewport. Vertices/UV/depth/source state stay immutable.
    // NEW HOST (R20): with a retained canvas and a widescreen layout
    // (host_wide::compute_layout(target, original_4x3()).wide()) the 4:3
    // capture is the centre of the extended logical canvas, which fills the
    // whole target with one uniform scale; uploaded X/Y copies get the canvas
    // offset (the immutable capture itself is unchanged).
    // Records a literal A1R5G5B5 upload and transition to shader-resource state.
    // The caller retains both returned objects through its submission fence.
    // NEW HOST (R23/R27/R28, R28b): the conversion chain is exactly `profile`
    // (never the current quality): edge fix, then 8-bit expansion or 16-bit,
    // then mips; `format` = polygon_profile_format(profile) (the format must be
    // sampleable, else the upload throws). The chain is a pure function of the
    // words, so a device-loss re-upload with the recorded profile rebuilds the
    // same resource; `recovery` stays the literal words.
    // NEW HOST (R32 perf-swap): `staging`, when given, supplies the upload
    // buffer (a 512-byte aligned range of a persistently mapped arena page, no
    // committed resource created); PolygonTextureUpload::staging then names
    // that page. The caller keeps the arena range unreset until the fence of
    // the list that copies it. Same texture, same levels, same bytes copied.
    PolygonTextureUpload upload_a1r5g5b5(ID3D12GraphicsCommandList &, std::uint32_t token, std::uint32_t width,
                                         std::uint32_t height, std::span<const std::uint16_t>,
                                         const PolygonTextureProfile &, PolygonTransientArena *staging = nullptr);
    // NEW HOST (R32 perf-swap): notes the conversion shape of an identity that a
    // 430D40 replacement writes (host_texture_processing::note_replacement_shape),
    // with this device's format support, so race loads can prewarm its levels.
    void note_replacement_profile(const PolygonTextureProfile &) const;
    // No initial CPU write or clear: both destinations copy the same newly
    // allocated staging texture. Its initial pixel values are unspecified.
    // NEW HOST (R28b): the resource stays B5G5R5A1 (its readback is the 16-bit
    // recovery payload); its profile is texture_profile() without high
    // precision, so the 430D40 replacements that later fill it keep B5G5R5A1.
    std::vector<PolygonTextureUpload> generate_a1r5g5b5(ID3D12GraphicsCommandList &, std::uint32_t primary_token,
                                                        std::uint32_t alternate_token, std::uint32_t dimension);
};
} // namespace sgn
