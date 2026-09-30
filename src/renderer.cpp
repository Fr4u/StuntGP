#include "sgn/renderer.hpp"
#include "sgn/host_shader_cache.hpp"
#include "sgn/host_frame_trace.hpp"
#include "sgn/host_graphics_settings.hpp"
#include "sgn/host_post_process_d3d12.hpp"
#include "sgn/host_profiler.hpp"
#include "sgn/host_texture_upscale_d3d12.hpp"
#include "sgn/host_wide_layout.hpp"
#include "sgn/polygon_reprojection.hpp"
#include "sgn/polygon_submission.hpp"
#include "sgn/sdr_gamma_d3d12.hpp"
#include "sgn/live_scene_geometry.hpp"
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <thread>
#include <vector>
#include <cstdio>
#include <cwchar>
#include <deque>

namespace sgn
{
using Microsoft::WRL::ComPtr;
using namespace DirectX;
namespace
{
void hr(HRESULT result, const char *action)
{
    if (FAILED(result))
    {
        std::ostringstream out;
        out << action << " failed (0x" << std::hex << static_cast<unsigned long>(result) << ")";
        throw GraphicsError(result, out.str());
    }
}
struct Handle
{
    HANDLE value{};
    ~Handle()
    {
        if (value)
            CloseHandle(value);
    }
    Handle() = default;
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
};
D3D12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE type)
{
    D3D12_HEAP_PROPERTIES h{};
    h.Type = type;
    h.CreationNodeMask = 1;
    h.VisibleNodeMask = 1;
    return h;
}
D3D12_RESOURCE_DESC buffer_desc(std::size_t n)
{
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width = n;
    d.Height = 1;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return d;
}
D3D12_RESOURCE_BARRIER barrier(ID3D12Resource *r, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = r;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = before;
    b.Transition.StateAfter = after;
    return b;
}
std::string narrow(const wchar_t *text)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), 0);
    WideCharToMultiByte(CP_UTF8, 0, text, -1, s.data(), n, nullptr, nullptr);
    s.pop_back();
    return s;
}
constexpr DXGI_FORMAT target_format = DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr DXGI_FORMAT srgb_format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
// NEW R22: four swap-chain buffers with frame latency 2 (the waitable object).
// With three, one buffer on screen plus two queued left no free buffer, so the
// next frame blocked on the display (gamma/fence wait up to 12 ms at 240 Hz):
// irregular frame sampling and dropped refreshes with the high-refresh presenter.
constexpr unsigned frame_count = 4, max_textures = 512;
// NEW R19 (perf_gpu): command contexts (allocator + retirement fence + retained
// GPU owners + transient arena) are decoupled from swap-chain slots. A stage
// and a present are two submissions per source frame, so six contexts keep
// three frames of GPU work in flight; the CPU waits only to reuse a context
// whose submission is still executing (true GPU-bound backpressure).
constexpr unsigned context_count = 6, image_pool_limit = 4;
// NEW HOST (R24): device-lost results (the GraphicsError::device_lost set).
bool device_lost_status(HRESULT result) noexcept
{
    return result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET ||
           result == DXGI_ERROR_DEVICE_HUNG || result == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}
// NEW HOST (R24): the preferred (native) timing of the panel behind the DXGI
// output whose GDI name is `device` ("\\.\DISPLAYn"), read through the Win32
// display-configuration API. nullopt when no active path reports one (remote
// or indirect displays); the caller then uses the desktop resolution.
std::optional<std::array<UINT, 2>> preferred_display_extent(const wchar_t *device)
{
    std::vector<DISPLAYCONFIG_PATH_INFO> paths;
    std::vector<DISPLAYCONFIG_MODE_INFO> modes;
    LONG status = ERROR_INSUFFICIENT_BUFFER;
    for (unsigned attempt = 0; attempt < 4 && status == ERROR_INSUFFICIENT_BUFFER; ++attempt)
    {
        UINT32 path_count{}, mode_count{};
        status = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_count, &mode_count);
        if (status != ERROR_SUCCESS)
            return std::nullopt;
        paths.resize(path_count);
        modes.resize(mode_count);
        status =
            QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_count, paths.data(), &mode_count, modes.data(), nullptr);
        if (status == ERROR_SUCCESS)
            paths.resize(path_count);
    }
    if (status != ERROR_SUCCESS)
        return std::nullopt;
    for (const auto &path : paths)
    {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
        source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        source.header.size = sizeof(source);
        source.header.adapterId = path.sourceInfo.adapterId;
        source.header.id = path.sourceInfo.id;
        if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS ||
            std::wcscmp(source.viewGdiDeviceName, device) != 0)
            continue;
        DISPLAYCONFIG_TARGET_PREFERRED_MODE preferred{};
        preferred.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_PREFERRED_MODE;
        preferred.header.size = sizeof(preferred);
        preferred.header.adapterId = path.targetInfo.adapterId;
        preferred.header.id = path.targetInfo.id;
        if (DisplayConfigGetDeviceInfo(&preferred.header) == ERROR_SUCCESS && preferred.width && preferred.height)
            return std::array<UINT, 2>{preferred.width, preferred.height};
    }
    return std::nullopt;
}
// NEW HOST (R24): the exclusive-fullscreen mode of `output`: its native
// resolution (preferred timing, else the desktop resolution) at the highest
// refresh rate DXGI enumerates there for the swap-chain format. Only
// enumerated modes are used (a non-enumerated mode makes DXGI blit instead of
// flip, "Full-screen performance tip"); interlaced/scaled modes are excluded.
DXGI_MODE_DESC select_exclusive_mode(IDXGIOutput &output)
{
    DXGI_OUTPUT_DESC description{};
    hr(output.GetDesc(&description), "exclusive output description");
    std::vector<DXGI_MODE_DESC> modes;
    for (unsigned attempt = 0;; ++attempt)
    {
        UINT count = 0;
        hr(output.GetDisplayModeList(target_format, 0, &count, nullptr), "exclusive display mode count");
        modes.resize(count);
        if (!count)
            break;
        const auto listed = output.GetDisplayModeList(target_format, 0, &count, modes.data());
        if (listed == DXGI_ERROR_MORE_DATA && attempt < 4)
            continue; // the mode list changed between the two calls
        hr(listed, "exclusive display mode list");
        modes.resize(count);
        break;
    }
    if (modes.empty())
        throw FormatError("exclusive fullscreen: the output enumerates no RGBA8 display mode");
    const auto &desktop = description.DesktopCoordinates;
    const auto desktop_width = static_cast<UINT>(desktop.right - desktop.left);
    const auto desktop_height = static_cast<UINT>(desktop.bottom - desktop.top);
    // Rotated outputs may report either orientation; both are accepted.
    const auto best_at = [&](UINT width, UINT height) -> std::optional<DXGI_MODE_DESC> {
        std::optional<DXGI_MODE_DESC> best;
        for (const auto &mode : modes)
        {
            if (!((mode.Width == width && mode.Height == height) || (mode.Width == height && mode.Height == width)))
                continue;
            if (!mode.RefreshRate.Numerator || !mode.RefreshRate.Denominator)
                continue;
            if (!best || std::uint64_t(mode.RefreshRate.Numerator) * best->RefreshRate.Denominator >
                             std::uint64_t(best->RefreshRate.Numerator) * mode.RefreshRate.Denominator)
                best = mode;
        }
        return best;
    };
    if (const auto native = preferred_display_extent(description.DeviceName))
        if (const auto mode = best_at((*native)[0], (*native)[1]))
            return *mode;
    if (const auto mode = best_at(desktop_width, desktop_height))
        return *mode;
    throw FormatError("exclusive fullscreen: no enumerated mode at the native or desktop resolution");
}
constexpr char shader[] = R"(
cbuffer Constants : register(b0) { row_major float4x4 camera; row_major float4x4 normal_transform; float4 options; float4 source_projection; float4 source_clip; };
Texture2D image : register(t0); SamplerState filter : register(s0);
struct Input { float3 p:POSITION; float3 n:NORMAL; float2 uv:TEXCOORD; float4 c:COLOR; };
struct Output { float4 p:SV_POSITION; float3 n:NORMAL; float2 uv:TEXCOORD; float4 c:COLOR; float2 clip_distance:SV_ClipDistance; };
Output vs(Input v) {
    Output o; o.clip_distance=float2(1,1);
    bool ui=options.x>0.5 && options.x<1.5;
    o.p=ui ? float4(v.p.x/options.y*2-1,1-v.p.y/options.z*2,0,1):mul(float4(v.p,1),camera);
    if(!ui && options.w>0.5) {
        precise float3 projected=float3(o.p.x*source_projection.x,o.p.y*source_projection.y,o.p.z*source_projection.z+source_projection.w);
        // Source viewport uses camera Z+1 and stores 32/(Z+1) as depth.
        // D3D12 performs hardware polygon clipping; the scalar reference keeps
        // the distinct original edge intersection and sign-mask behavior.
        o.clip_distance=float2(projected.z-source_clip.x,source_clip.y-projected.z);
        o.p=float4(projected.x*source_clip.z,-projected.y*source_clip.w,32,o.p.z+1);
    }
    o.n=mul(float4(v.n,0),normal_transform).xyz;o.uv=v.uv;o.c=v.c;return o;
}
float4 ps(Output i):SV_TARGET {
    float4 c=image.Sample(filter,i.uv);
    if(options.x>1.5) {
        // Source ARGB diffuse words, COLOROP/ALPHAOP MODULATE (42FA80).
        // Modern sRGB textures/target retain linear-light filtering/blending.
        float3 diffuse=i.c.bgr;
        diffuse=lerp(diffuse/12.92,pow(abs((diffuse+0.055)/1.055),2.4),step(0.04045,diffuse));
        c*=float4(diffuse,i.c.a);
        clip(c.a-(options.y>0.5 ? 1.0:160.0)/255.0); //433F40 alpha references
        return c;
    }
    if(options.x>0.5) return float4(pow(abs(i.c.rgb),2.2),c.a*i.c.a);
    clip(c.a-0.20);
    // New neutral inspection lighting. Not a claim of retail D3D7 parity.
    float squared=dot(i.n,i.n);
    float3 normal=squared>0 ? i.n*rsqrt(max(squared,1.0e-30)) : float3(0,0,0);
    float light=0.66+0.34*abs(dot(normal,normalize(float3(-0.4,0.85,-0.3))));
    return float4(c.rgb*light,c.a);
}
)";
ComPtr<ID3DBlob> compile(const char *entry, const char *target)
{
    ComPtr<ID3DBlob> code, errors;
    auto result = host_shader_cache::compile(
        shader, sizeof(shader), "embedded-inspection-shader", nullptr, entry, target,
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code,
        &errors);
    if (FAILED(result))
        throw std::runtime_error(errors ? static_cast<const char *>(errors->GetBufferPointer())
                                        : "HLSL compilation failed");
    return code;
}
// NEW HOST (R27) linear-light MSAA resolve (always used by the game path for
// MSAA > 1; ResolveSubresource averaged the stored gamma-encoded UNORM codes,
// which darkens and thickens bright/dark edges). Every sample is decoded with
// the sRGB EOTF, averaged in linear light and re-encoded. Why sRGB and not the
// game's ramp: the stored codes are display-referred values that the host
// presents on a G2.2/sRGB swap chain (DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
// the game's ramp is a later "lift" applied by the gamma pass after the
// resolve, not the encoding of the stored codes. The piecewise sRGB curve is
// the same curve the hardware _SRGB conversion and the R23 mip generator use.
// A pixel whose samples are all identical (every interior pixel) returns that
// sample unchanged, i.e. exactly what ResolveSubresource produced. Alpha is
// averaged linearly, like the fixed-function resolve. Blending stays gamma-space.
constexpr char linear_resolve_shader[] = R"(
Texture2DMS<float4> image : register(t0);
cbuffer Resolve : register(b0) { uint samples; uint3 unused; };
float4 vs(uint id : SV_VertexID) : SV_Position {
    float2 p=float2((id<<1)&2,id&2);
    return float4(p*float2(2,-2)+float2(-1,1),0,1);
}
float3 to_linear(float3 c) { return c<=0.04045?c/12.92:pow(abs((c+0.055)/1.055),2.4); }
float3 to_display(float3 l) { return l<=0.0031308?l*12.92:1.055*pow(abs(l),1.0/2.4)-0.055; }
float4 ps(float4 position : SV_Position) : SV_Target {
    int2 p=int2(position.xy);
    float4 first=image.Load(p,0);
    bool identical=true;
    float3 light=to_linear(first.rgb);
    float alpha=first.a;
    for(uint i=1;i<samples;++i) {
        float4 s=image.Load(p,i);
        identical=identical&&all(s==first);
        light+=to_linear(s.rgb);
        alpha+=s.a;
    }
    if(identical) return first;
    return float4(saturate(to_display(light/(float)samples)),alpha/(float)samples);
}
)";
ComPtr<ID3DBlob> compile_linear_resolve(const char *entry, const char *target)
{
    ComPtr<ID3DBlob> code, errors;
    auto result = host_shader_cache::compile(
        linear_resolve_shader, sizeof(linear_resolve_shader), "sgn-linear-msaa-resolve", nullptr, entry, target,
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code,
        &errors);
    if (FAILED(result))
        throw std::runtime_error(errors ? static_cast<const char *>(errors->GetBufferPointer())
                                        : "linear resolve HLSL compilation failed");
    return code;
}

// NEW HOST (R28b): thread-safe builder (device + D3DCompile only), so the render thread
// can prewarm it in the background and adopt the result.
struct ResolvePipeline
{
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> pipeline;
};
ResolvePipeline build_linear_resolve(ID3D12Device &device)
{
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable = {1, &range};
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[1].Constants = {0, 0, 4};
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC description{};
    description.NumParameters = 2;
    description.pParameters = parameters;
    ComPtr<ID3DBlob> serialized, error;
    ComPtr<ID3D12RootSignature> next_root;
    ComPtr<ID3D12PipelineState> next_pipeline;
    hr(D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error),
       "linear resolve root serialization");
    hr(device.CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
                                  IID_PPV_ARGS(&next_root)),
       "linear resolve root");
    const auto vs = compile_linear_resolve("vs", "vs_5_0"), ps = compile_linear_resolve("ps", "ps_5_0");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};
    state.pRootSignature = next_root.Get();
    state.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    state.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    state.SampleMask = UINT_MAX;
    state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    state.NumRenderTargets = 1;
    state.RTVFormats[0] = target_format;
    state.SampleDesc.Count = 1;
    state.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    state.RasterizerState.DepthClipEnable = TRUE;
    state.DepthStencilState.DepthEnable = FALSE;
    state.BlendState.RenderTarget[0].SrcBlend = state.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    state.BlendState.RenderTarget[0].DestBlend = state.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    state.BlendState.RenderTarget[0].BlendOp = state.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    state.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    hr(device.CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&next_pipeline)), "linear resolve pipeline");
    return {std::move(next_root), std::move(next_pipeline)};
}
std::vector<Image> mip_chain(Image first)
{
    std::vector<Image> chain;
    chain.push_back(std::move(first));
    std::array<float, 256> linear{};
    for (unsigned i = 0; i < 256; ++i)
    {
        float x = static_cast<float>(i) / 255;
        linear[i] = x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f);
    }
    while (chain.back().width > 1 || chain.back().height > 1)
    {
        const auto &src = chain.back();
        Image dst{std::max(1u, src.width / 2), std::max(1u, src.height / 2), {}};
        dst.rgba.resize(std::size_t(dst.width) * dst.height * 4);
        for (unsigned y = 0; y < dst.height; ++y)
            for (unsigned x = 0; x < dst.width; ++x)
            {
                float rgb[3]{}, alpha = 0;
                unsigned samples = 0;
                for (unsigned dy = 0; dy < (src.height > 1 ? 2u : 1u); ++dy)
                    for (unsigned dx = 0; dx < (src.width > 1 ? 2u : 1u); ++dx)
                    {
                        auto at = (std::size_t(std::min(src.height - 1, y * 2 + dy)) * src.width +
                                   std::min(src.width - 1, x * 2 + dx)) *
                                  4;
                        float a = src.rgba[at + 3] / 255.f;
                        alpha += a;
                        for (unsigned c = 0; c < 3; ++c)
                            rgb[c] += linear[src.rgba[at + c]] * a;
                        ++samples;
                    }
                auto at = (std::size_t(y) * dst.width + x) * 4;
                for (unsigned c = 0; c < 3; ++c)
                {
                    float v = alpha > 0 ? rgb[c] / alpha : 0;
                    float s = v <= 0.0031308f ? 12.92f * v : 1.055f * std::pow(v, 1.f / 2.4f) - 0.055f;
                    dst.rgba[at + c] = static_cast<std::uint8_t>(std::clamp(std::lround(s * 255), 0l, 255l));
                }
                dst.rgba[at + 3] = static_cast<std::uint8_t>(std::lround(alpha / static_cast<float>(samples) * 255));
            }
        chain.push_back(std::move(dst));
    }
    return chain;
}
} // namespace

struct Renderer::Impl
{
    struct PolygonSurfaceImage
    {
        ComPtr<ID3D12Resource> pixels;
        SdrGammaRamp gamma; // Baked into pixels; metadata never reapplied on refresh.
        std::uint64_t fence{};
        bool inject_signal_failure{}; // Explicit debug receiving seam, never source state.
        // NEW R19: state after the last EXECUTED list (pooled allocations are
        // reused; a fresh allocation starts in COPY_DEST).
        D3D12_RESOURCE_STATES state{D3D12_RESOURCE_STATE_COPY_DEST};
    };
    // NEW R19 host allocation pool for image pixels. An image's last owner is
    // always a retired context, the roles or a flush, so a recycled resource
    // has no GPU use in flight. Capacity is reserved: recycling never throws.
    struct ImagePool
    {
        struct Entry
        {
            ComPtr<ID3D12Resource> pixels;
            D3D12_RESOURCE_STATES state;
        };
        std::vector<Entry> free;
        ImagePool()
        {
            free.reserve(image_pool_limit);
        }
    };
    struct ImageRecycler
    {
        std::weak_ptr<ImagePool> pool;
        void operator()(PolygonSurfaceImage *image) const noexcept
        {
            if (image && image->pixels)
            {
                if (const auto owner = pool.lock(); owner && owner->free.size() < image_pool_limit)
                    owner->free.push_back({std::move(image->pixels), image->state});
            }
            delete image;
        }
    };
    struct Frame
    {
        ComPtr<ID3D12Resource> back, overlay;
        ComPtr<ID3D12Resource> live_upload, live_vertices;
        std::size_t live_capacity{};
        std::uint64_t live_revision{};
        D3D12_VERTEX_BUFFER_VIEW live_view{};
        std::vector<UINT> live_offsets;
        // Last submission that wrote this slot's CPU-visible overlay/live data
        // or its capture readback (scene/direct/front presentation).
        std::uint64_t fence{};
        std::uint64_t gamma_fence{}; // last submission reading this slot's gamma constants
        ComPtr<ID3D12Resource> capture_readback;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT capture_layout{};
        UINT64 capture_bytes{};
        SdrGammaFrame gamma; // Frame-fenced GPU cache, no borrowed source ramp.
    };
    struct CommandContext
    {
        ComPtr<ID3D12CommandAllocator> allocator;
        std::uint64_t fence{};
        bool timed{};      // its timestamp pair was written by that submission
        bool fxaa_timed{}; // NEW HOST (R23): its FXAA timestamp pair was written too
        std::vector<PolygonGpuRecording> polygon_recordings;
        // Real GPU image ownership is installed before Execute, including when
        // queue Signal fails and no normal retirement fence can be published.
        std::shared_ptr<const PolygonSurfaceImage> polygon_image;
        std::unique_ptr<PolygonTransientArena> arena;
    };
    HWND window{};
    RenderOptions options;
    RenderStats telemetry;
    ComPtr<IDXGIFactory6> factory;
    ComPtr<ID3D12Device> device;
    RendererAdapterInfo adapter_info;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<IDXGISwapChain3> swap;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12InfoQueue> info;
    Handle fence_event, latency_event;
    ComPtr<ID3D12QueryHeap> timestamps;
    ComPtr<ID3D12Resource> timestamp_readback;
    std::uint64_t timestamp_frequency{};
    ComPtr<ID3D12DescriptorHeap> rtvs, dsvs, srvs;
    ComPtr<ID3D12Resource> color, depth, vertices, font;
    ComPtr<ID3D12DescriptorHeap> polygon_rtvs;
    ComPtr<ID3D12Resource> polygon_color;
    // NEW HOST (R27) linear-light resolve: SRV of polygon_color (replaced with
    // it, after a flush) and the sample-count independent pipeline.
    ComPtr<ID3D12DescriptorHeap> resolve_srvs;
    ComPtr<ID3D12RootSignature> resolve_root;
    ComPtr<ID3D12PipelineState> resolve_pipeline;
    std::unique_ptr<PolygonD3D12> polygon_backend;
    std::unique_ptr<SdrGammaD3D12> gamma_backend;
    std::unique_ptr<HostPostProcessD3D12> post_process; // NEW HOST (R28) HUD split + bloom, created on first use
    std::vector<HostPostSegment> post_segment_scratch;  // NEW HOST (R30 perf_renderer): prepare_post_process list
    // NEW HOST (R30 perf_renderer): the last shared submissions admitted by the
    // shared-ownership transient path (see present_polygon_submission_transient).
    struct TransientAdmission
    {
        std::weak_ptr<const CapturedPolygonSubmission> submission;
        std::optional<PolygonLogicalRasterExtent> canvas;
        bool fit_canvas{};
    };
    std::array<TransientAdmission, 4> transient_admissions{};
    std::size_t next_transient_admission{};
    std::vector<PolygonTextureUpload> pending_polygon_uploads;
    // NEW HOST (R32 perf-swap) fence-tracked polygon texture uploads: no CPU wait for the GPU. A batch is
    // a command allocator plus a persistently mapped upload arena (staging); lists are appended to the
    // current batch while it has room and it is reset only after the fence of its last list. Every upload
    // (texture + staging page) is owned by fenced_uploads until its own fence; deferred_releases holds the
    // resources a replacement superseded until the last submission before the swap retired.
    struct UploadBatch
    {
        ComPtr<ID3D12CommandAllocator> allocator;
        std::unique_ptr<PolygonTransientArena> arena;
        std::uint64_t fence{};
        std::size_t bytes{};
    };
    std::vector<UploadBatch> upload_batches;
    std::size_t upload_batch{};
    struct FencedUpload
    {
        std::uint64_t fence{};
        PolygonTextureUpload upload;
    };
    std::deque<FencedUpload> fenced_uploads;
    struct DeferredRelease
    {
        std::uint64_t fence{};
        ComPtr<ID3D12Resource> resource;
    };
    std::deque<DeferredRelease> deferred_releases;
    std::optional<RenderFailurePoint> injected_failure;
    std::function<void()> before_polygon_submit;
    bool targets_ready{};
    bool polygon_submission_unfenced{}; // Terminal reuse guard after Execute/Signal failure.
    bool reverse_depth{};
    const std::thread::id surface_owner = std::this_thread::get_id();
    bool polygon_roles_active{};
    std::shared_ptr<const PolygonSurfaceImage> polygon_front, polygon_back;
    void require_surface_thread() const
    {
        if (std::this_thread::get_id() != surface_owner)
            throw FormatError("polygon surface used outside renderer owner thread");
    }
    std::array<Frame, frame_count> frames;
    std::array<CommandContext, context_count> contexts;
    unsigned next_context{};
    ComPtr<ID3D12CommandAllocator> upload_allocator; // flush-guarded one-shot uploads
    std::shared_ptr<ImagePool> image_pool = std::make_shared<ImagePool>();
    std::vector<ComPtr<ID3D12Resource>> textures;
    struct Mesh
    {
        std::vector<Surface> surfaces;
        std::vector<std::string> texture_names;
        std::vector<unsigned> texture_remap;
        std::vector<Vertex> live_vertices;
        std::vector<LiveMaterial> live_materials;
        bool live{};
        std::uint32_t parts{};
    };
    std::vector<Mesh> meshes;
    D3D12_VERTEX_BUFFER_VIEW vertex_view{};
    std::vector<Vertex> overlay;
    std::vector<GpuDiagnostic> diagnostics;
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> opaque, opaque_mirrored, opaque_two_sided, transparent, transparent_mirrored,
        transparent_two_sided, overlay_pipeline;
    std::array<ComPtr<ID3D12PipelineState>, 6> source_pipelines;
    std::array<ComPtr<ID3D12PipelineState>, 30> live_pipelines;
    std::array<float, 96> advance{};
    unsigned rtv_step{}, srv_step{}, last_back{};
    std::uint64_t next_fence{1};
    std::uint64_t live_revision{1};
    unsigned swap_flags{};
    bool submitted{}, occluded{};
    // NEW HOST (R23) image quality (RenderOptions::host_graphics only). The
    // applied snapshot changes only at apply_host_graphics(), which runs at the
    // start of a draw/upload, outside every command-list recording.
    host_graphics::Settings graphics{};
    std::uint64_t graphics_revision{};
    bool graphics_applied{};
    unsigned pipeline_samples{}; // sample count the inspection pipelines were built for
    unsigned frame_latency{2};   // NEW HOST (R27): current SetMaximumFrameLatency value
    // NEW HOST (R28b) pipeline prewarm. The requested settings snapshot; its
    // pipeline-affecting part (MSAA, A2C, smooth magnification, FXAA, smooth
    // gradients) is applied only once every pipeline it needs exists. Jobs are
    // declared after `device`, so they are destroyed (joined) before it.
    host_graphics::Settings requested_graphics{};
    std::uint64_t requested_revision{};
    bool sample_targets_valid{true}; // false while an MSAA rebuild has not completed
    std::future<ResolvePipeline> resolve_job;
    struct GammaJob
    {
        bool fxaa{}, dither{};
        std::future<SdrGammaVariant> result;
    };
    std::vector<GammaJob> gamma_jobs;
    // NEW HOST (R35) MSAA target prebuild: an MSAA change creates its new sample-count targets (MSAA colour,
    // polygon MSAA colour, depth: the committed resources exactly as create_depth / create_targets /
    // create_polygon_targets describe them) on a background thread (the device is free-threaded); the
    // setting applies once they exist (like its pipelines), and the rebuild then takes them instead of
    // creating them on the render thread. Fresh resources either way: same descriptions, states and clear
    // values, so every frame renders the same pixels.
    struct SampleTargets
    {
        std::uint32_t width{}, height{}, samples{};
        bool reversed{};
        ComPtr<ID3D12Resource> depth, color, polygon_color;
    };
    std::future<SampleTargets> sample_target_job;
    std::uint32_t sample_target_job_samples{};
    std::optional<SampleTargets> prebuilt_targets;
    // NEW HOST (R24) display modes. Changed only on the owner thread at a safe
    // point (display-mode calls, resize, draw start), never while recording.
    bool mode_switch_enabled{};     // swap chain created with ALLOW_MODE_SWITCH (host_graphics only)
    bool exclusive_active{};        // DXGI fullscreen state last observed
    bool swap_transition_pending{}; // a fullscreen transition still needs ResizeBuffers

    Impl(HWND w, RenderOptions opt) : window(w), options(opt)
    {
        // NEW HOST (R23): the domain is the MSAA counts D3D12 defines for these
        // targets (1/2/4/8, the host_graphics::sanitize domain). A count the
        // device lacks is never an error: creation falls back to the highest
        // supported count at or below the request (1 always exists).
        if (options.requested_samples != 1 && options.requested_samples != 2 && options.requested_samples != 4 &&
            options.requested_samples != 8)
            throw FormatError("requested sample count must be 1, 2, 4 or 8");
        overlay.reserve(8192);
        if (options.debug)
        {
            ComPtr<ID3D12Debug> debug;
            hr(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)), "D3D12 debug layer required for --debug");
            debug->EnableDebugLayer();
            telemetry.debug_active = true;
        }
        hr(CreateDXGIFactory2(options.debug ? DXGI_CREATE_FACTORY_DEBUG : 0, IID_PPV_ARGS(&factory)),
           "CreateDXGIFactory2");
        ComPtr<IDXGIAdapter1> adapter;
        if (options.adapter_luid)
        {
            LUID requested{};
            requested.LowPart = static_cast<DWORD>(*options.adapter_luid);
            requested.HighPart = static_cast<LONG>(*options.adapter_luid >> 32u);
            hr(factory->EnumAdapterByLuid(requested, IID_PPV_ARGS(&adapter)),
               "selected DXGI adapter no longer available");
            DXGI_ADAPTER_DESC1 selected{};
            hr(adapter->GetDesc1(&selected), "selected adapter description");
            if (bool(selected.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != options.warp)
                throw FormatError("selected adapter software policy disagrees with RenderOptions");
        }
        else if (options.warp)
            hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)), "WARP adapter");
        else
        {
            for (unsigned n = 0;; ++n)
            {
                ComPtr<IDXGIAdapter1> candidate;
                auto result = factory->EnumAdapterByGpuPreference(n, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                                  IID_PPV_ARGS(&candidate));
                if (result == DXGI_ERROR_NOT_FOUND)
                    break;
                hr(result, "enumerate graphics adapter");
                DXGI_ADAPTER_DESC1 d{};
                hr(candidate->GetDesc1(&d), "graphics adapter description");
                if (!(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) &&
                    SUCCEEDED(
                        D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
                {
                    adapter = candidate;
                    break;
                }
            }
            if (!adapter)
                throw std::runtime_error("No hardware D3D12 adapter. Use --warp explicitly for diagnostics.");
        }
        DXGI_ADAPTER_DESC1 ad{};
        hr(adapter->GetDesc1(&ad), "GetDesc1");
        telemetry.adapter = narrow(ad.Description);
        adapter_info = {std::uint64_t(ad.AdapterLuid.LowPart) |
                            (std::uint64_t(static_cast<std::uint32_t>(ad.AdapterLuid.HighPart)) << 32u),
                        ad.DedicatedVideoMemory,
                        ad.SharedSystemMemory,
                        ad.VendorId,
                        ad.DeviceId,
                        bool(ad.Flags & DXGI_ADAPTER_FLAG_SOFTWARE),
                        telemetry.adapter};
        hr(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "D3D12CreateDevice");
        if (options.debug)
            hr(device.As(&info), "D3D12 info queue");
        D3D12_COMMAND_QUEUE_DESC q{};
        q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        hr(device->CreateCommandQueue(&q, IID_PPV_ARGS(&queue)), "CreateCommandQueue");
        hr(queue->GetTimestampFrequency(&timestamp_frequency), "GPU timestamp frequency");
        D3D12_QUERY_HEAP_DESC queries{};
        queries.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        // NEW HOST (R23): four per context (submission begin/end, FXAA begin/end).
        queries.Count = context_count * 4;
        hr(device->CreateQueryHeap(&queries, IID_PPV_ARGS(&timestamps)), "timestamp heap");
        timestamp_readback = make_buffer(context_count * 4 * sizeof(std::uint64_t), D3D12_HEAP_TYPE_READBACK,
                                         D3D12_RESOURCE_STATE_COPY_DEST);
        BOOL tearing = FALSE;
        telemetry.tearing_supported =
            SUCCEEDED(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing, sizeof(tearing))) &&
            tearing;
        // NEW HOST (R24): a host_graphics swap chain may enter exclusive
        // fullscreen (display_mode=exclusive), which needs ALLOW_MODE_SWITCH
        // for the native mode. The waitable object stays valid in fullscreen
        // under Direct3D 12 (DXGI_SWAP_CHAIN_FLAG). The flags never change
        // afterwards: ResizeBuffers always passes this same value (tearing
        // cannot be added or removed there).
        mode_switch_enabled = options.host_graphics;
        telemetry.mode_switch_enabled = mode_switch_enabled;
        swap_flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT |
                     (telemetry.tearing_supported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0) |
                     (mode_switch_enabled ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0);
        RECT rect{};
        GetClientRect(window, &rect);
        telemetry.width = std::max(1l, rect.right);
        telemetry.height = std::max(1l, rect.bottom);
        host_wide::set_output_size(telemetry.width, telemetry.height); // NEW R20 host layout (no render effect here)
        DXGI_SWAP_CHAIN_DESC1 sc{};
        sc.Width = telemetry.width;
        sc.Height = telemetry.height;
        sc.Format = target_format;
        sc.SampleDesc.Count = 1;
        sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sc.BufferCount = frame_count;
        sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sc.Flags = swap_flags;
        ComPtr<IDXGISwapChain1> initial;
        hr(factory->CreateSwapChainForHwnd(queue.Get(), window, &sc, nullptr, nullptr, &initial),
           "CreateSwapChainForHwnd");
        hr(initial.As(&swap), "swap chain v3");
        // DXGI's own Alt+Enter stays disabled: the host display-mode controller
        // (host_display_mode.hpp) is the only owner of fullscreen transitions.
        hr(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER), "MakeWindowAssociation");
        hr(swap->SetMaximumFrameLatency(2), "SetMaximumFrameLatency");
        latency_event.value = swap->GetFrameLatencyWaitableObject();
        if (!latency_event.value)
            throw std::runtime_error("missing frame latency event");
        hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "CreateFence");
        fence_event.value = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!fence_event.value)
            throw std::runtime_error("CreateEvent failed");
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = frame_count + 1;
        hr(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtvs)), "RTV heap");
        rtv_step = device->GetDescriptorHandleIncrementSize(desc.Type);
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        desc.NumDescriptors = 1;
        hr(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&dsvs)), "DSV heap");
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = max_textures;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        hr(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srvs)), "SRV heap");
        srv_step = device->GetDescriptorHandleIncrementSize(desc.Type);
        telemetry.samples = 1;
        telemetry.supported_samples_mask = supported_sample_mask();
        if (options.host_graphics)
        {
            // NEW HOST (R23): the live settings snapshot decides the initial
            // targets (requested_samples is not consulted). The revision is read
            // BEFORE the snapshot: a set() racing in between can only make the
            // stored revision older than the applied values, so the first
            // apply_host_graphics() re-applies; a later change is never skipped.
            graphics_revision = host_graphics::revision();
            graphics = host_graphics::current();
            graphics_applied = true;
            requested_revision = graphics_revision; // NEW HOST (R28b)
            requested_graphics = graphics;
            telemetry.samples = clamp_samples(graphics.msaa_samples);
            options.vsync = graphics.vsync; // NEW HOST (R24): Present follows the settings
            apply_frame_latency();          // NEW HOST (R27): low_latency
        }
        else if (options.requested_samples == 2 || options.requested_samples == 8)
            telemetry.samples = clamp_samples(options.requested_samples); // NEW HOST (R23)
        else
        {
            // Pre-R23 selection for 1 and 4, unchanged.
            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS ms{target_format, 4,
                                                             D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE, 0};
            if (options.requested_samples == 4 &&
                SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &ms, sizeof(ms))) &&
                ms.NumQualityLevels)
            {
                ms.Format = DXGI_FORMAT_D32_FLOAT;
                if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &ms, sizeof(ms))) &&
                    ms.NumQualityLevels)
                    telemetry.samples = 4;
            }
        }
        publish_quality_stats();
        for (auto &frame : frames)
            frame.overlay = make_buffer(2 * 1024 * 1024, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        for (auto &context : contexts)
            hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&context.allocator)),
               "command allocator");
        hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&upload_allocator)),
           "command allocator");
        hr(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, upload_allocator.Get(), nullptr,
                                     IID_PPV_ARGS(&list)),
           "command list");
        hr(list->Close(), "initial close");
        create_targets();
        create_pipelines();
        create_font();
        if (options.host_graphics)
        {
            // NEW HOST (R28b): prewarm at startup: the PS/gamma/resolve variants of the
            // initial settings and every pipeline state remembered by an earlier run.
            if (!options.pipeline_state_file.empty())
                polygons().remember_pipeline_states(read_pipeline_states(options.pipeline_state_file));
            start_prewarm(graphics);
        }
    }
    void retire_submitted_work_noexcept() noexcept
    {
        // A healthy-device timeout is not permission to free queued resources.
        // Teardown waits until the queue retires or device removal is confirmed.
        for (;;)
        {
            try
            {
                flush();
                break;
            }
            catch (...)
            {
                if (!device || FAILED(device->GetDeviceRemovedReason()))
                    break;
                Sleep(1);
            }
        }
    }
    ~Impl()
    {
        retire_submitted_work_noexcept();
        leave_fullscreen_for_release_noexcept();
        save_pipeline_states_noexcept(); // NEW HOST (R28b)
    }
    // NEW HOST (R28b) remembered pipeline states: one hexadecimal code per line.
    // A missing file is an empty list; malformed lines are skipped (like the
    // settings file), since the list only decides what is built early.
    static std::vector<std::uint64_t> read_pipeline_states(const std::filesystem::path &path)
    {
        std::vector<std::uint64_t> codes;
        std::ifstream in(path, std::ios::binary);
        std::string line;
        while (in && std::getline(in, line))
        {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
                line.pop_back();
            std::uint64_t code{};
            const auto [end, error] = std::from_chars(line.data(), line.data() + line.size(), code, 16);
            if (error == std::errc{} && end == line.data() + line.size() && !line.empty())
                codes.push_back(code);
        }
        return codes;
    }
    // Written atomically (temporary file + rename). A destructor cannot recover
    // from a failure; it is reported to the debugger output, never hidden.
    void save_pipeline_states_noexcept() noexcept
    {
        if (options.pipeline_state_file.empty() || !polygon_backend)
            return;
        try
        {
            const auto &path = options.pipeline_state_file;
            std::error_code error;
            std::filesystem::create_directories(path.parent_path(), error);
            auto temporary = path;
            temporary += ".tmp";
            {
                std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
                char text[24];
                for (const auto code : polygon_backend->pipeline_states())
                {
                    const auto [end, status] = std::to_chars(text, text + sizeof text, code, 16);
                    (void)status;
                    out.write(text, end - text);
                    out.put('\n');
                }
                if (!out.flush())
                    error = std::make_error_code(std::errc::io_error);
            }
            if (!error)
                std::filesystem::rename(temporary, path, error);
            if (error)
                OutputDebugStringA(("StuntGP NEW: pipeline state list not saved: " + error.message() + "\n").c_str());
        }
        catch (const std::exception &failure)
        {
            OutputDebugStringA("StuntGP NEW: pipeline state list not saved: ");
            OutputDebugStringA(failure.what());
            OutputDebugStringA("\n");
        }
    }
    // NEW HOST (R24): a swap chain must not be released in full-screen mode
    // (DXGI overview, "Destroying a swap chain"). Runs on every destruction:
    // exit, race restart and device-loss recreation. SetFullscreenState(FALSE)
    // is a no-op for a windowed swap chain. A destructor cannot recover from a
    // failure; it is reported to the debugger output, never hidden.
    void leave_fullscreen_for_release_noexcept() noexcept
    {
        if (!swap || !mode_switch_enabled)
            return;
        const auto left = swap->SetFullscreenState(FALSE, nullptr);
        if (FAILED(left))
        {
            char text[112]{};
            std::snprintf(text, sizeof(text),
                          "StuntGP NEW: SetFullscreenState(FALSE) before swap-chain release failed (0x%08lx)\n",
                          static_cast<unsigned long>(left));
            OutputDebugStringA(text);
        }
    }
    bool consume_failure(RenderFailurePoint point) noexcept
    {
        if (injected_failure != point)
            return false;
        injected_failure.reset();
        return true;
    }
    void require_targets() const
    {
        if (polygon_submission_unfenced)
            throw FormatError("unfenced polygon work requires renderer teardown");
        if (!targets_ready)
            throw FormatError("renderer targets unavailable; retry resize before drawing or capture");
    }
    // NEW HOST (R23): sample counts usable for EVERY target the renderer
    // creates with them (polygon/scene color RGBA8 and its sRGB view, D32 depth).
    std::uint32_t supported_sample_mask() const
    {
        std::uint32_t mask = 1u << 1;
        for (const unsigned count : {2u, 4u, 8u})
        {
            bool usable = true;
            for (const auto format : {target_format, srgb_format, DXGI_FORMAT_D32_FLOAT})
            {
                D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS ms{format, count,
                                                                 D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE, 0};
                if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &ms, sizeof(ms))) ||
                    !ms.NumQualityLevels)
                {
                    usable = false;
                    break;
                }
            }
            if (usable)
                mask |= 1u << count;
        }
        return mask;
    }
    // Largest supported count not above the request (1 is always available).
    unsigned clamp_samples(unsigned requested) const noexcept
    {
        for (const unsigned count : {8u, 4u, 2u})
            if (count <= requested && (telemetry.supported_samples_mask & (1u << count)))
                return count;
        return 1;
    }
    PolygonRasterQuality raster_quality() const noexcept
    {
        return raster_quality_for(graphics);
    }
    PolygonRasterQuality raster_quality_for(const host_graphics::Settings &settings) const noexcept
    {
        PolygonRasterQuality quality{};
        if (!options.host_graphics)
            return quality; // exact pre-R23 polygon path
        quality.filter = settings.texture_filter == host_graphics::TextureFilter::anisotropic
                             ? PolygonRasterQuality::Filter::anisotropic
                         : settings.texture_filter == host_graphics::TextureFilter::trilinear
                             ? PolygonRasterQuality::Filter::trilinear
                             : PolygonRasterQuality::Filter::original;
        quality.anisotropy = std::clamp<std::uint32_t>(settings.anisotropy, 1u, 16u);
        quality.generate_mipmaps = settings.generate_mipmaps;
        quality.alpha_to_coverage = settings.alpha_to_coverage;
        quality.texture_edge_fix = settings.texture_edge_fix; // NEW HOST (R27)
        // NEW HOST (R28) texture pipeline v2 and magnification quality.
        quality.high_precision_textures = settings.high_precision_textures;
        quality.texture_sharpness = std::clamp<std::int32_t>(settings.texture_sharpness, -3, 3);
        quality.smooth_magnification = settings.smooth_magnification;
        return quality;
    }
    // NEW HOST (R27): dithered SDR store and the white-point fix (gamma pass).
    bool smooth_gradients_enabled() const noexcept
    {
        return options.host_graphics && graphics.smooth_gradients;
    }
    // NEW HOST (R27): linear-light resolve for every MSAA > 1 target of the game
    // (host_graphics) path; renderers that do not follow the settings (tests,
    // oracles) keep the fixed-function resolve.
    bool linear_resolve() const noexcept
    {
        return options.host_graphics && telemetry.samples > 1;
    }
    // NEW HOST (R27): low_latency = one queued frame instead of two. The
    // frame-latency waitable object stays the pacing primitive; only its
    // maximum changes, live, on the swap chain (kept across ResizeBuffers).
    void apply_frame_latency()
    {
        const unsigned wanted = options.host_graphics && graphics.low_latency ? 1u : 2u;
        if (wanted == frame_latency)
            return;
        hr(swap->SetMaximumFrameLatency(wanted), "SetMaximumFrameLatency");
        frame_latency = wanted;
        telemetry.max_frame_latency = wanted;
    }
    void create_linear_resolve_pipeline()
    {
        if (resolve_pipeline)
            return;
        adopt_linear_resolve(build_linear_resolve(*device.Get())); // fallback; normally prewarmed (R28b)
    }
    void adopt_linear_resolve(ResolvePipeline built)
    {
        if (resolve_pipeline)
            return; // recorded lists may reference the current one
        resolve_root = std::move(built.root);
        resolve_pipeline = std::move(built.pipeline);
    }
    bool fxaa_enabled() const noexcept
    {
        return options.host_graphics && graphics.post_anti_aliasing == host_graphics::PostAntiAliasing::fxaa;
    }
    void publish_quality_stats() noexcept
    {
        telemetry.requested_samples = options.host_graphics ? graphics.msaa_samples : options.requested_samples;
        telemetry.graphics_revision = options.host_graphics ? graphics_revision : 0;
        telemetry.fxaa_active = fxaa_enabled();
        telemetry.alpha_to_coverage_active =
            options.host_graphics && graphics.alpha_to_coverage && telemetry.samples > 1;
        telemetry.mipmaps_active = options.host_graphics && graphics.generate_mipmaps;
        telemetry.smooth_gradients_active = smooth_gradients_enabled();
        telemetry.texture_edge_fix_active = options.host_graphics && graphics.texture_edge_fix;
        telemetry.linear_resolve_active = linear_resolve();
        telemetry.max_frame_latency = frame_latency;
        telemetry.high_precision_textures_active = options.host_graphics && graphics.high_precision_textures;
        telemetry.smooth_magnification_active = options.host_graphics && graphics.smooth_magnification;
    }
    // NEW HOST (R23): MSAA change. Same discipline as resize: retire all queued
    // work, then recreate every sample-count-dependent object (scene/polygon
    // MSAA color, shared depth, inspection PSOs, polygon PSO cache). Resolved
    // single-sample images (front/back roles, gamma caches, pool) are unaffected.
    // A failure leaves targets_ready false and pipeline_samples stale, so the
    // next apply retries the whole rebuild.
    void rebuild_sample_targets(unsigned samples)
    {
        if (polygon_submission_unfenced)
            throw FormatError("unfenced polygon work requires renderer teardown");
        flush();
        sample_targets_valid = false;
        targets_ready = false;
        color.Reset();
        polygon_color.Reset();
        polygon_rtvs.Reset();
        resolve_srvs.Reset();
        depth.Reset();
        telemetry.samples = samples;
        if (polygon_backend)
            polygon_backend->set_sample_desc(DXGI_SAMPLE_DESC{samples, 0});
        create_targets();
        // NEW HOST (R28b): the inspection PSOs (draw_scene only, never the game path)
        // are rebuilt lazily by draw_scene instead of stalling this frame.
        sample_targets_valid = true;
    }
    void ensure_gamma_backend()
    {
        if (gamma_backend)
            return;
        UINT support{};
        constexpr auto space = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
        hr(swap->CheckColorSpaceSupport(space, &support), "SDR gamma color-space support");
        if (!(support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
            throw FormatError("SDR gamma swap-chain color space unavailable");
        hr(swap->SetColorSpace1(space), "window-local SDR color space");
        gamma_backend = std::make_unique<SdrGammaD3D12>(*device.Get());
    }
    // NEW HOST (R28b): starts (never waits for) every background build the
    // settings need that is neither built nor already running.
    static SampleTargets build_sample_targets(ComPtr<ID3D12Device> owner, std::uint32_t width, std::uint32_t height,
                                              std::uint32_t samples, bool reversed, DXGI_FORMAT target,
                                              DXGI_FORMAT srgb, bool host_graphics)
    {
        SampleTargets built{width, height, samples, reversed, {}, {}, {}};
        D3D12_HEAP_PROPERTIES h{};
        h.Type = D3D12_HEAP_TYPE_DEFAULT;
        h.CreationNodeMask = h.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC d{};
        d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        d.Width = width;
        d.Height = height;
        d.DepthOrArraySize = 1;
        d.MipLevels = 1;
        d.SampleDesc.Count = samples;
        // create_depth
        d.Format = DXGI_FORMAT_D32_FLOAT;
        d.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        D3D12_CLEAR_VALUE clear{};
        clear.Format = d.Format;
        clear.DepthStencil.Depth = reversed ? 0.0f : 1.0f;
        hr(owner->CreateCommittedResource(&h, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear,
                                          IID_PPV_ARGS(&built.depth)),
           "prebuilt depth buffer");
        if (samples > 1)
        {
            // create_targets: the MSAA colour buffer
            d.Format = target;
            d.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            clear = {};
            clear.Format = srgb;
            clear.Color[0] = 0.012f;
            clear.Color[1] = 0.018f;
            clear.Color[2] = 0.028f;
            clear.Color[3] = 1;
            hr(owner->CreateCommittedResource(&h, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_RENDER_TARGET, &clear,
                                              IID_PPV_ARGS(&built.color)),
               "prebuilt MSAA color buffer");
            // create_polygon_targets: the polygon window MSAA colour
            D3D12_CLEAR_VALUE polygon_clear{};
            polygon_clear.Format = target;
            polygon_clear.Color[3] = 1;
            hr(owner->CreateCommittedResource(&h, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                              &polygon_clear, IID_PPV_ARGS(&built.polygon_color)),
               "prebuilt polygon window MSAA color");
        }
        (void)host_graphics;
        return built;
    }
    // Starts the prebuild for `samples` at the current extent unless it exists or runs.
    void start_sample_targets(std::uint32_t samples)
    {
        if (samples == telemetry.samples)
            return;
        if (prebuilt_targets && prebuilt_targets->samples == samples && prebuilt_targets->width == telemetry.width &&
            prebuilt_targets->height == telemetry.height)
            return;
        if (sample_target_job.valid() && sample_target_job_samples == samples)
            return;
        if (sample_target_job.valid())
            return; // another count is being built; restarted after it is adopted
        ComPtr<ID3D12Device> owner = device;
        sample_target_job_samples = samples;
        sample_target_job =
            std::async(std::launch::async, build_sample_targets, owner, telemetry.width, telemetry.height, samples,
                       reverse_depth, target_format, srgb_format, options.host_graphics);
    }
    // True when the targets for `samples` exist at the current extent (adopting a finished job).
    bool sample_targets_prebuilt(std::uint32_t samples)
    {
        if (samples == telemetry.samples)
            return true;
        if (sample_target_job.valid() &&
            sample_target_job.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            try
            {
                prebuilt_targets = sample_target_job.get();
            }
            catch (...)
            {
                prebuilt_targets.reset(); // the rebuild creates them itself (and reports its own failure)
                return true;
            }
        }
        if (prebuilt_targets && (prebuilt_targets->samples != samples || prebuilt_targets->width != telemetry.width ||
                                 prebuilt_targets->height != telemetry.height))
            prebuilt_targets.reset();
        if (prebuilt_targets)
            return true;
        start_sample_targets(samples);
        return false;
    }
    // A prebuilt resource for the current extent and sample count, or null (the caller creates one).
    ComPtr<ID3D12Resource> take_prebuilt(ComPtr<ID3D12Resource> SampleTargets::*which, bool depth_reversed = false)
    {
        if (!prebuilt_targets || prebuilt_targets->samples != telemetry.samples ||
            prebuilt_targets->width != telemetry.width || prebuilt_targets->height != telemetry.height ||
            (which == &SampleTargets::depth && prebuilt_targets->reversed != depth_reversed))
            return nullptr;
        auto taken = std::move((*prebuilt_targets).*which);
        if (!prebuilt_targets->depth && !prebuilt_targets->color && !prebuilt_targets->polygon_color)
            prebuilt_targets.reset();
        return taken;
    }
    void start_prewarm(const host_graphics::Settings &next)
    {
        const auto samples = clamp_samples(next.msaa_samples);
        start_sample_targets(samples); // NEW HOST (R35)
        polygons().prewarm_pipelines(DXGI_SAMPLE_DESC{samples, 0}, raster_quality_for(next));
        const bool fxaa = next.post_anti_aliasing == host_graphics::PostAntiAliasing::fxaa;
        const bool dither = next.smooth_gradients;
        if (fxaa || dither)
        {
            ensure_gamma_backend();
            if (!gamma_backend->has_variant(fxaa, dither) &&
                std::none_of(gamma_jobs.begin(), gamma_jobs.end(),
                             [&](const GammaJob &job) { return job.fxaa == fxaa && job.dither == dither; }))
            {
                ComPtr<ID3D12Device> owner = device;
                gamma_jobs.push_back({fxaa, dither, std::async(std::launch::async, [owner, fxaa, dither] {
                                          return SdrGammaD3D12::build_variant(*owner.Get(), fxaa, dither);
                                      })});
            }
        }
        if (samples > 1 && !resolve_pipeline && !resolve_job.valid())
        {
            ComPtr<ID3D12Device> owner = device;
            resolve_job = std::async(std::launch::async, [owner] { return build_linear_resolve(*owner.Get()); });
        }
        // NEW HOST (R28): HUD split / bloom pipelines, same background policy (never waited for).
        if (options.host_graphics && (fxaa || next.bloom))
        {
            if (!post_process)
                post_process = std::make_unique<HostPostProcessD3D12>(*device.Get(), timestamp_frequency);
            post_process->prewarm(DXGI_SAMPLE_DESC{samples, 0}, target_format, raster_quality_for(next), next.bloom);
            // NEW HOST (R35): and its size-dependent targets (the resolve format is target_format, as in
            // prepare_post_process).
            post_process->prewarm_targets(telemetry.width, telemetry.height, DXGI_SAMPLE_DESC{samples, 0},
                                          target_format, target_format, next.bloom);
        }
    }
    // NEW HOST (R28b): adopts finished builds (render thread, no list recording)
    // and reports whether everything `next` needs exists; restarts what is missing.
    bool prewarm_complete(const host_graphics::Settings &next)
    {
        const auto ready = [](auto &future) {
            return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        };
        for (auto job = gamma_jobs.begin(); job != gamma_jobs.end();)
            if (ready(job->result))
            {
                ensure_gamma_backend();
                gamma_backend->adopt_variant(job->fxaa, job->dither, job->result.get());
                job = gamma_jobs.erase(job);
            }
            else
                ++job;
        if (ready(resolve_job))
            adopt_linear_resolve(resolve_job.get());
        const auto samples = clamp_samples(next.msaa_samples);
        const bool fxaa = next.post_anti_aliasing == host_graphics::PostAntiAliasing::fxaa;
        const bool dither = next.smooth_gradients;
        bool complete = polygons().pipelines_prewarmed(DXGI_SAMPLE_DESC{samples, 0}, raster_quality_for(next));
        if ((fxaa || dither) && (!gamma_backend || !gamma_backend->has_variant(fxaa, dither)))
            complete = false;
        if (samples > 1 && !resolve_pipeline)
            complete = false;
        if (!sample_targets_prebuilt(samples)) // NEW HOST (R35): the new MSAA targets exist
            complete = false;
        if (!complete)
            start_prewarm(next);
        return complete;
    }
    // The once-per-draw safe point: called before any list Reset of a draw or
    // upload, never while a command list is recording.
    void apply_host_graphics()
    {
        if (!options.host_graphics)
            return;
        const auto revision = host_graphics::revision();
        if (revision != requested_revision)
        {
            requested_graphics = host_graphics::current();
            requested_revision = revision;
            start_prewarm(requested_graphics); // NEW HOST (R28b): background, never waited for
        }
        if (graphics_applied && requested_revision == graphics_revision && sample_targets_valid)
            return;
        // NEW HOST (R28b): everything that needs no new pipeline applies now; the
        // pipeline-affecting part keeps its current value until its pipelines exist
        // (checked again at every later draw/upload), so no present waits for a compile.
        auto next = requested_graphics;
        const bool complete = prewarm_complete(requested_graphics);
        if (!complete)
        {
            next.msaa_samples = graphics.msaa_samples;
            next.alpha_to_coverage = graphics.alpha_to_coverage;
            next.smooth_magnification = graphics.smooth_magnification;
            next.post_anti_aliasing = graphics.post_anti_aliasing;
            next.smooth_gradients = graphics.smooth_gradients;
        }
        const auto samples = clamp_samples(next.msaa_samples);
        if (samples != telemetry.samples || !sample_targets_valid)
        {
            SGN_PROFILE_SCOPE("gpu.msaa_reconfigure");
            rebuild_sample_targets(samples);
        }
        graphics = next;
        options.vsync = next.vsync; // NEW HOST (R24): live VSync for every later Present
        apply_frame_latency();      // NEW HOST (R27): live low_latency
        if (polygon_backend)
            polygon_backend->set_raster_quality(raster_quality());
        if (complete)
            graphics_revision = requested_revision; // otherwise retried at the next safe point
        graphics_applied = true;
        publish_quality_stats();
    }
    ComPtr<ID3D12Resource> make_buffer(std::size_t n, D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES state)
    {
        ComPtr<ID3D12Resource> r;
        auto h = heap(type);
        auto d = buffer_desc(n);
        hr(device->CreateCommittedResource(&h, D3D12_HEAP_FLAG_NONE, &d, state, nullptr, IID_PPV_ARGS(&r)),
           "buffer allocation");
        return r;
    }
    void check_device()
    {
        hr(device->GetDeviceRemovedReason(), "GPU device removed");
    }
    void wait(std::uint64_t value)
    {
        auto completed = fence->GetCompletedValue();
        if (completed == UINT64_MAX)
        {
            check_device();
            throw GraphicsError(DXGI_ERROR_DEVICE_REMOVED, "GPU fence signaled removal");
        }
        if (completed < value)
        {
            hr(fence->SetEventOnCompletion(value, fence_event.value), "fence event");
            SGN_PROFILE_SCOPE("gpu.fence_wait");
            auto result = WaitForSingleObject(fence_event.value, 15000);
            check_device();
            if (result != WAIT_OBJECT_0)
                throw std::runtime_error("GPU fence timed out");
            // An earlier timed-out registration may signal the shared event.
            // Event wakeup alone is not proof this requested fence retired.
            completed = fence->GetCompletedValue();
            if (completed == UINT64_MAX)
                throw GraphicsError(DXGI_ERROR_DEVICE_REMOVED, "GPU fence signaled removal");
            if (completed < value)
                throw std::runtime_error("GPU fence event woke before requested retirement");
        }
    }
    void flush()
    {
        if (!queue || !fence)
            return;
        auto value = next_fence++;
        hr(queue->Signal(fence.Get(), value), "flush signal");
        wait(value);
        pending_polygon_uploads.clear();
        retire_fenced(value); // NEW HOST (R32): everything queued before the flush has retired
        telemetry.pending_polygon_uploads = 0;
    }
    // NEW HOST (R32 perf-swap): releases fenced uploads and superseded textures whose fence retired.
    // Non-blocking; on device removal (UINT64_MAX) nothing is released (the teardown policy decides).
    void retire_fenced(std::uint64_t completed) noexcept
    {
        if (completed == UINT64_MAX)
            return;
        while (!fenced_uploads.empty() && fenced_uploads.front().fence <= completed)
            fenced_uploads.pop_front();
        while (!deferred_releases.empty() && deferred_releases.front().fence <= completed)
        {
            deferred_releases.pop_front();
            sgn::host_profile::add("swap.deferred_release", 0);
        }
        telemetry.pending_polygon_uploads = pending_polygon_uploads.size() + fenced_uploads.size();
    }
    // NEW HOST (R32 perf-swap): the batch for the next upload list; waits only when every batch is still
    // executing AND full (bulk loading), never for an ordinary upload or swap.
    UploadBatch &acquire_upload_batch()
    {
        const auto completed = fence->GetCompletedValue();
        if (completed == UINT64_MAX)
        {
            check_device();
            throw GraphicsError(DXGI_ERROR_DEVICE_REMOVED, "GPU fence signaled removal");
        }
        retire_fenced(completed);
        constexpr std::size_t batch_budget = 16u * 1024u * 1024u, batch_limit = 4;
        const auto reset = [&](std::size_t index) -> UploadBatch & {
            auto &batch = upload_batches[index];
            hr(batch.allocator->Reset(), "upload allocator reset");
            batch.arena->reset();
            batch.bytes = 0;
            upload_batch = index;
            return batch;
        };
        if (!upload_batches.empty())
        {
            auto &current = upload_batches[upload_batch];
            if (current.fence <= completed)
                return reset(upload_batch);
            if (current.bytes < batch_budget)
                return current; // append another list to the executing allocator (never reset while in flight)
        }
        for (std::size_t index = 0; index < upload_batches.size(); ++index)
            if (upload_batches[index].fence <= completed)
                return reset(index);
        if (upload_batches.size() < batch_limit)
        {
            UploadBatch batch;
            hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&batch.allocator)),
               "upload command allocator");
            batch.arena = std::make_unique<PolygonTransientArena>(*device.Get());
            upload_batches.push_back(std::move(batch));
            upload_batch = upload_batches.size() - 1;
            return upload_batches.back();
        }
        std::size_t oldest = 0;
        for (std::size_t index = 1; index < upload_batches.size(); ++index)
            if (upload_batches[index].fence < upload_batches[oldest].fence)
                oldest = index;
        wait(upload_batches[oldest].fence);
        retire_fenced(fence->GetCompletedValue());
        return reset(oldest);
    }
    void retire_polygon_texture(ComPtr<ID3D12Resource> resource) noexcept
    {
        if (!resource)
            return;
        try
        {
            deferred_releases.push_back({next_fence - 1, std::move(resource)});
        }
        catch (...)
        {
            // Only an extra owner is lost: every bank and recording that references it still owns it.
        }
    }
    D3D12_CPU_DESCRIPTOR_HANDLE rtv(unsigned i) const
    {
        auto h = rtvs->GetCPUDescriptorHandleForHeapStart();
        h.ptr += std::size_t(i) * rtv_step;
        return h;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu(unsigned i) const
    {
        auto h = srvs->GetCPUDescriptorHandleForHeapStart();
        h.ptr += std::size_t(i) * srv_step;
        return h;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE srv_gpu(unsigned i) const
    {
        auto h = srvs->GetGPUDescriptorHandleForHeapStart();
        h.ptr += std::uint64_t(i) * srv_step;
        return h;
    }
    void create_depth(bool reversed)
    {
        D3D12_RESOURCE_DESC d{};
        d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        d.Width = telemetry.width;
        d.Height = telemetry.height;
        d.DepthOrArraySize = 1;
        d.MipLevels = 1;
        d.SampleDesc.Count = telemetry.samples;
        d.Format = DXGI_FORMAT_D32_FLOAT;
        d.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        auto h = heap(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_CLEAR_VALUE clear{};
        clear.Format = d.Format;
        clear.DepthStencil.Depth = reversed ? 0.0f : 1.0f;
        ComPtr<ID3D12Resource> next = take_prebuilt(&SampleTargets::depth, reversed); // NEW HOST (R35)
        if (!next)
            hr(device->CreateCommittedResource(&h, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear,
                                               IID_PPV_ARGS(&next)),
               "depth buffer");
        depth = std::move(next);
        device->CreateDepthStencilView(depth.Get(), nullptr, dsvs->GetCPUDescriptorHandleForHeapStart());
        reverse_depth = reversed;
    }
    void create_targets()
    {
        targets_ready = false;
        if (consume_failure(RenderFailurePoint::target_allocation))
            hr(E_OUTOFMEMORY, "injected target allocation");
        for (unsigned i = 0; i < frame_count; ++i)
        {
            hr(swap->GetBuffer(i, IID_PPV_ARGS(&frames[i].back)), "swap buffer");
            D3D12_RENDER_TARGET_VIEW_DESC view{};
            view.Format = srgb_format;
            view.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            device->CreateRenderTargetView(frames[i].back.Get(), &view, rtv(i));
        }
        D3D12_RESOURCE_DESC d{};
        d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        d.Width = telemetry.width;
        d.Height = telemetry.height;
        d.DepthOrArraySize = 1;
        d.MipLevels = 1;
        d.SampleDesc.Count = telemetry.samples;
        auto h = heap(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_CLEAR_VALUE clear{};
        create_depth(reverse_depth);
        if (telemetry.samples > 1)
        {
            d.Format = target_format;
            d.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            clear = {};
            clear.Format = srgb_format;
            clear.Color[0] = 0.012f;
            clear.Color[1] = 0.018f;
            clear.Color[2] = 0.028f;
            clear.Color[3] = 1;
            color = take_prebuilt(&SampleTargets::color); // NEW HOST (R35)
            if (!color)
                hr(device->CreateCommittedResource(&h, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                   &clear, IID_PPV_ARGS(&color)),
                   "MSAA color buffer");
            D3D12_RENDER_TARGET_VIEW_DESC v{};
            v.Format = srgb_format;
            v.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
            device->CreateRenderTargetView(color.Get(), &v, rtv(frame_count));
        }
        targets_ready = true;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE polygon_rtv(unsigned index) const
    {
        auto handle = polygon_rtvs->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += std::size_t(index) * rtv_step;
        return handle;
    }
    void create_polygon_targets()
    {
        D3D12_DESCRIPTOR_HEAP_DESC heap_description{};
        heap_description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heap_description.NumDescriptors = frame_count + 1;
        ComPtr<ID3D12DescriptorHeap> next_rtvs, next_resolve_srvs;
        ComPtr<ID3D12Resource> next_color;
        hr(device->CreateDescriptorHeap(&heap_description, IID_PPV_ARGS(&next_rtvs)), "polygon window RTV heap");
        const auto next_rtv = [&](unsigned index) {
            auto handle = next_rtvs->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += std::size_t(index) * rtv_step;
            return handle;
        };
        for (unsigned index = 0; index < frame_count; ++index)
        {
            D3D12_RENDER_TARGET_VIEW_DESC view{};
            view.Format = target_format;
            view.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            device->CreateRenderTargetView(frames[index].back.Get(), &view, next_rtv(index));
        }
        if (consume_failure(RenderFailurePoint::polygon_target_allocation))
            hr(E_OUTOFMEMORY, "injected polygon target allocation");
        if (telemetry.samples > 1)
        {
            D3D12_RESOURCE_DESC d{};
            d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            d.Width = telemetry.width;
            d.Height = telemetry.height;
            d.DepthOrArraySize = d.MipLevels = 1;
            d.SampleDesc.Count = telemetry.samples;
            d.Format = target_format;
            d.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            const auto properties = heap(D3D12_HEAP_TYPE_DEFAULT);
            D3D12_CLEAR_VALUE clear{};
            clear.Format = target_format;
            clear.Color[3] = 1;
            next_color = take_prebuilt(&SampleTargets::polygon_color); // NEW HOST (R35)
            if (!next_color)
                hr(device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &d,
                                                   D3D12_RESOURCE_STATE_RENDER_TARGET, &clear,
                                                   IID_PPV_ARGS(&next_color)),
                   "polygon window MSAA color");
            D3D12_RENDER_TARGET_VIEW_DESC view{};
            view.Format = target_format;
            view.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
            device->CreateRenderTargetView(next_color.Get(), &view, next_rtv(frame_count));
            if (options.host_graphics)
            {
                // NEW HOST (R27): shader view of the MSAA colour for the linear resolve.
                D3D12_DESCRIPTOR_HEAP_DESC srv_heap{};
                srv_heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                srv_heap.NumDescriptors = 1;
                srv_heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                hr(device->CreateDescriptorHeap(&srv_heap, IID_PPV_ARGS(&next_resolve_srvs)),
                   "linear resolve descriptor heap");
                D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
                srv.Format = target_format;
                srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
                srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                device->CreateShaderResourceView(next_color.Get(), &srv,
                                                 next_resolve_srvs->GetCPUDescriptorHandleForHeapStart());
            }
        }
        polygon_rtvs = std::move(next_rtvs);
        polygon_color = std::move(next_color);
        resolve_srvs = std::move(next_resolve_srvs);
        prebuilt_targets.reset(); // NEW HOST (R35): the last consumer of a prebuild; nothing is kept
    }
    PolygonD3D12 &polygons()
    {
        if (!polygon_backend)
        {
            auto next = std::make_unique<PolygonD3D12>(*device.Get(), target_format, DXGI_FORMAT_D32_FLOAT,
                                                       DXGI_SAMPLE_DESC{telemetry.samples, 0});
            next->set_raster_quality(raster_quality()); // NEW HOST (R23); default quality when not following settings
            polygon_backend = std::move(next);
        }
        return *polygon_backend;
    }
    void execute_polygon_uploads()
    {
        telemetry.pending_polygon_uploads = pending_polygon_uploads.size();
        hr(list->Close(), "polygon upload close");
        ID3D12CommandList *lists[]{list.Get()};
        queue->ExecuteCommandLists(1, lists);
        const auto retirement = next_fence++;
        hr(queue->Signal(fence.Get(), retirement), "polygon upload signal");
        if (consume_failure(RenderFailurePoint::polygon_upload_wait))
            throw std::runtime_error("injected polygon upload wait failure");
        wait(retirement);
        pending_polygon_uploads.clear();
        telemetry.pending_polygon_uploads = 0;
    }
    // NEW HOST (R28b): profile of a NEW texture identity, from the settings
    // snapshot applied at this safe point (no list is recording here).
    PolygonTextureProfile polygon_texture_profile()
    {
        apply_host_graphics();
        auto profile = polygons().texture_profile();
        // NEW HOST (R28) sharp HUD: this identity's 2D upscale limit, from the setting
        // and the output size now (1 = never; tests/oracles without host_graphics: 1).
        profile.upscale =
            static_cast<std::uint8_t>(options.host_graphics && graphics.sharp_hud
                                          ? host_texture_upscale::factor_for_output(telemetry.width, telemetry.height)
                                          : 1u);
        // NEW HOST (R32 perf-swap): a generated identity created from this snapshot (the usual 430D40
        // destination) has this profile without high precision; noting its shape here, at every new
        // identity (menus load long before a race), lets the first race load prewarm its swaps.
        auto generated = profile;
        generated.high_precision = false;
        polygons().note_replacement_profile(generated);
        return profile;
    }
    PolygonGpuTexture upload_polygon_texture(std::uint32_t token, std::uint32_t width, std::uint32_t height,
                                             std::span<const std::uint16_t> pixels,
                                             const PolygonTextureProfile &profile)
    {
        apply_host_graphics(); // NEW HOST (R23) safe point; the conversion is `profile`, not the settings
        auto &backend = polygons();
        // NEW HOST (R28) sharp HUD: 2D UI content is uploaded upscaled, up to the identity's
        // recorded limit (R28b); otherwise `image` is the literal source.
        const auto image = host_texture_upscale::prepare_polygon_upload(token, width, height, pixels, profile.upscale);
        // NEW HOST (R32 perf-swap): no CPU wait for the GPU. The former begin_upload() drained the whole
        // queue (flush) and execute_polygon_uploads() waited for the copy: two waits inside the tick for
        // an in-race swap. The copy is now recorded into a fence-tracked batch (staging in its persistent
        // arena), submitted on the frame queue, and owned until its own fence. Queue order puts the copy
        // (and its COPY_DEST -> PIXEL_SHADER_RESOURCE barrier) before every later list, i.e. before any
        // draw that samples the new texture. Same texture, same levels, same bytes.
        if (polygon_submission_unfenced)
            throw FormatError("unfenced polygon work requires renderer teardown");
        SGN_PROFILE_SCOPE("gpu.upload_polygon_texture_submit");
        auto &batch = acquire_upload_batch();
        hr(list->Reset(batch.allocator.Get(), nullptr), "upload list reset");
        PolygonTextureUpload upload;
        try
        {
            upload = backend.upload_a1r5g5b5(*list.Get(), token, image.width, image.height, image.words, profile,
                                             batch.arena.get());
        }
        catch (...)
        {
            hr(list->Close(), "rejected polygon upload close");
            throw;
        }
        hr(list->Close(), "polygon upload close");
        fenced_uploads.push_back({next_fence, upload}); // owners before Execute: never freed under the GPU
        const auto retirement = next_fence++;
        batch.fence = retirement;
        batch.bytes += image.words.size_bytes() * 3; // staging estimate (levels, 8-bit path, row pitch)
        ID3D12CommandList *lists[]{list.Get()};
        queue->ExecuteCommandLists(1, lists);
        hr(queue->Signal(fence.Get(), retirement), "polygon upload signal");
        telemetry.pending_polygon_uploads = pending_polygon_uploads.size() + fenced_uploads.size();
        if (consume_failure(RenderFailurePoint::polygon_upload_wait))
            throw std::runtime_error("injected polygon upload wait failure");
        telemetry.uploaded_texture_bytes += pixels.size_bytes();
        if (image.source)
        {
            // NEW HOST (R28): recovery keeps the source extent and words; R28b records the applied factor.
            upload.texture.recovery = image.source;
            upload.texture.scale = static_cast<std::uint8_t>(image.factor);
        }
        return std::move(upload.texture);
    }
    // New host recovery storage.430730 leaves pixels unspecified; retain the
    // actual allocation's bytes while the device is healthy, never guess them.
    std::shared_ptr<const PolygonTexturePixels> retain_generated_pixels(const PolygonGpuTexture &texture)
    {
        const auto description = texture.resource->GetDesc();
        if (description.Width == 0 || description.Width > 16384 || description.Height == 0 ||
            description.Height > 16384 || description.Format != DXGI_FORMAT_B5G5R5A1_UNORM)
            throw FormatError("generated recovery image extent/format");
        auto pixels = std::make_shared<PolygonTexturePixels>();
        pixels->width = static_cast<std::uint32_t>(description.Width);
        pixels->height = description.Height;
        pixels->words.resize(std::size_t(pixels->width) * pixels->height);
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};
        UINT64 bytes{};
        device->GetCopyableFootprints(&description, 0, 1, 0, &layout, nullptr, nullptr, &bytes);
        auto readback =
            make_buffer(static_cast<std::size_t>(bytes), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
        begin_upload();
        try
        {
            // Existing pending upload ownership retains BOTH GPU resources if
            // Execute/Signal/Wait fails. No local COM release before the fence.
            pending_polygon_uploads.push_back({texture, readback});
            auto to_copy = barrier(texture.resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                   D3D12_RESOURCE_STATE_COPY_SOURCE);
            list->ResourceBarrier(1, &to_copy);
            D3D12_TEXTURE_COPY_LOCATION source{}, destination{};
            source.pResource = texture.resource.Get();
            source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            destination.pResource = readback.Get();
            destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            destination.PlacedFootprint = layout;
            list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
            auto restore = barrier(texture.resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            list->ResourceBarrier(1, &restore);
        }
        catch (...)
        {
            hr(list->Close(), "rejected generated recovery close");
            throw;
        }
        execute_polygon_uploads();
        void *mapped{};
        D3D12_RANGE range{0, static_cast<SIZE_T>(bytes)};
        hr(readback->Map(0, &range, &mapped), "generated recovery map");
        for (std::uint32_t y = 0; y < pixels->height; ++y)
            std::memcpy(pixels->words.data() + std::size_t(y) * pixels->width,
                        static_cast<const std::byte *>(mapped) + layout.Offset +
                            std::size_t(y) * layout.Footprint.RowPitch,
                        std::size_t(pixels->width) * 2);
        D3D12_RANGE no_write{};
        readback->Unmap(0, &no_write);
        return pixels;
    }
    std::vector<PolygonGpuTexture> generate_polygon_textures(std::uint32_t primary, std::uint32_t alternate,
                                                             std::uint32_t dimension)
    {
        apply_host_graphics(); // NEW HOST (R28b): the identities' profile comes from this snapshot
        auto &backend = polygons();
        begin_upload();
        std::vector<PolygonGpuTexture> result;
        try
        {
            auto uploads = backend.generate_a1r5g5b5(*list.Get(), primary, alternate, dimension);
            result.reserve(uploads.size());
            for (const auto &upload : uploads)
                result.push_back(upload.texture);
            pending_polygon_uploads.insert(pending_polygon_uploads.end(), uploads.begin(), uploads.end());
        }
        catch (...)
        {
            hr(list->Close(), "rejected generated polygon upload close");
            throw;
        }
        execute_polygon_uploads();
        if (result.empty())
            throw FormatError("generated upload returned no actual texture");
        const auto recovery = retain_generated_pixels(result.front());
        // NEW HOST (R28b): the generated identities' profile: this snapshot, without high
        // precision (the resource and its readback recovery stay B5G5R5A1).
        auto profile = polygon_texture_profile();
        profile.high_precision = false;
        for (auto &texture : result)
        {
            texture.recovery = recovery;
            texture.profile = profile;
        }
        return result;
    }
    // NEW R19: the only CPU wait of a normal frame. Waits for the retirement of
    // the submission that last used this context (six submissions ago), then
    // releases its retained owners, reads its timestamps and resets storage.
    CommandContext &acquire_context(const char *reset_action)
    {
        SGN_PROFILE_SCOPE("gpu.context_acquire");
        auto &context = contexts[next_context];
        wait(context.fence);
        const auto slot = next_context;
        next_context = (next_context + 1) % context_count;
        if (context.timed)
        {
            context.timed = false;
            read_context_timestamps(slot, context.fxaa_timed);
        }
        if (post_process)
            post_process->retire_context(slot); // NEW HOST (R28) split/bloom GPU timings
        context.fxaa_timed = false;
        context.polygon_image.reset();
        context.polygon_recordings.clear();
        if (!fenced_uploads.empty() || !deferred_releases.empty())
            retire_fenced(fence->GetCompletedValue()); // NEW HOST (R32): non-blocking
        hr(context.allocator->Reset(), reset_action);
        if (context.arena)
            context.arena->reset();
        return context;
    }
    unsigned context_slot(const CommandContext &context) const noexcept
    {
        return static_cast<unsigned>(&context - contexts.data());
    }
    PolygonTransientArena &context_arena(CommandContext &context)
    {
        if (!context.arena)
            context.arena = std::make_unique<PolygonTransientArena>(*device.Get());
        return *context.arena;
    }
    // Early, non-blocking release of owners retained by retired contexts so
    // image allocations return to the pool before the next stage needs one.
    void release_retired_contexts() noexcept
    {
        const auto completed = fence->GetCompletedValue();
        if (completed == UINT64_MAX)
            return; // removal: keep the existing teardown policy
        for (auto &context : contexts)
            if (context.fence <= completed)
            {
                context.polygon_image.reset();
                context.polygon_recordings.clear();
            }
    }
    std::shared_ptr<PolygonSurfaceImage> make_polygon_image(const D3D12_RESOURCE_DESC &description)
    {
        SGN_PROFILE_SCOPE("gpu.image_acquire");
        release_retired_contexts();
        ImagePool::Entry entry{};
        auto &pool = image_pool->free;
        while (!pool.empty())
        {
            auto candidate = std::move(pool.back());
            pool.pop_back();
            const auto d = candidate.pixels->GetDesc();
            if (d.Dimension == description.Dimension && d.Width == description.Width &&
                d.Height == description.Height && d.DepthOrArraySize == description.DepthOrArraySize &&
                d.MipLevels == description.MipLevels && d.Format == description.Format &&
                d.SampleDesc.Count == description.SampleDesc.Count &&
                d.SampleDesc.Quality == description.SampleDesc.Quality && d.Layout == description.Layout &&
                d.Flags == description.Flags)
            {
                entry = std::move(candidate);
                break;
            }
        }
        if (!entry.pixels)
        {
            auto properties = heap(D3D12_HEAP_TYPE_DEFAULT);
            entry.state = D3D12_RESOURCE_STATE_COPY_DEST;
            hr(device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &description,
                                               D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&entry.pixels)),
               "polygon back image allocation");
        }
        auto owned = std::make_unique<PolygonSurfaceImage>();
        owned->pixels = std::move(entry.pixels);
        owned->state = entry.state;
        // shared_ptr(p,d) calls d(p) itself if its control block cannot be allocated.
        return std::shared_ptr<PolygonSurfaceImage>(owned.release(), ImageRecycler{image_pool});
    }
    void read_context_timestamps(unsigned index, bool fxaa)
    {
        void *mapped{};
        D3D12_RANGE range{index * 32, index * 32 + 32};
        hr(timestamp_readback->Map(0, &range, &mapped), "GPU timestamps read");
        const auto *times = static_cast<const std::uint64_t *>(mapped) + index * 4;
        if (times[1] >= times[0])
        {
            telemetry.gpu_ms_last =
                static_cast<double>(times[1] - times[0]) * 1000 / static_cast<double>(timestamp_frequency);
            // NEW R22 diagnostics (SGN_PROFILE only): GPU execution time of one submission.
            sgn::host_profile::add("gpu.execution_per_submission",
                                   static_cast<std::int64_t>(telemetry.gpu_ms_last * 1e6));
        }
        if (fxaa && times[3] >= times[2])
        {
            // NEW HOST (R23): GPU time of the combined FXAA + SDR gamma pass.
            telemetry.fxaa_gpu_ms_last =
                static_cast<double>(times[3] - times[2]) * 1000 / static_cast<double>(timestamp_frequency);
            sgn::host_profile::add("gpu.fxaa_execution", static_cast<std::int64_t>(telemetry.fxaa_gpu_ms_last * 1e6));
        }
        D3D12_RANGE empty{};
        timestamp_readback->Unmap(0, &empty);
    }
    void prepare_frame_capture(Frame &frame)
    {
        if (!options.capture_frames || frame.capture_readback)
            return;
        const auto description = frame.back->GetDesc();
        device->GetCopyableFootprints(&description, 0, 1, 0, &frame.capture_layout, nullptr, nullptr,
                                      &frame.capture_bytes);
        frame.capture_readback = make_buffer(static_cast<std::size_t>(frame.capture_bytes), D3D12_HEAP_TYPE_READBACK,
                                             D3D12_RESOURCE_STATE_COPY_DEST);
    }
    void record_frame_capture(Frame &frame, D3D12_RESOURCE_STATES before)
    {
        if (options.capture_frames)
        {
            auto transition = barrier(frame.back.Get(), before, D3D12_RESOURCE_STATE_COPY_SOURCE);
            list->ResourceBarrier(1, &transition);
            D3D12_TEXTURE_COPY_LOCATION from{}, to{};
            from.pResource = frame.back.Get();
            from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            to.pResource = frame.capture_readback.Get();
            to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            to.PlacedFootprint = frame.capture_layout;
            list->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
            before = D3D12_RESOURCE_STATE_COPY_SOURCE;
        }
        auto transition = barrier(frame.back.Get(), before, D3D12_RESOURCE_STATE_PRESENT);
        list->ResourceBarrier(1, &transition);
    }
    struct PolygonRenderSegment
    {
        const CapturedPolygonFrame *commands;
        const PolygonDeviceState *initial;
        std::shared_ptr<const PolygonGpuTextures> textures;
        const PolygonDeviceState *expected_final;
    };
    std::vector<PolygonRenderSegment> segment_scratch; // NEW HOST (R30 perf_renderer): draw_polygon_submission list
    // NEW HOST (R33): set only inside present_polygon_reprojected (its segments are the base's).
    const PolygonReprojection *active_reprojection{};
    void present_polygon_frame(unsigned index, bool gamma_applied)
    {
        if (consume_failure(RenderFailurePoint::polygon_frame_present))
            hr(E_FAIL, "injected polygon frame Present");
        SGN_PROFILE_SCOPE("gpu.present_call");
        if (!present_to_swap_chain("polygon Present"))
            return;
        last_back = index;
        submitted = true;
        ++telemetry.presents;
        if (gamma_applied)
            ++telemetry.gamma_presentations;
    }
    // NEW HOST (R24) single Present policy for every mode. VSync on: interval 1.
    // VSync off: interval 0 with DXGI_PRESENT_ALLOW_TEARING in windowed and
    // borderless (tearing support required), and WITHOUT it in exclusive
    // fullscreen, where the flag is invalid and interval 0 already tears.
    // exclusive_active was re-read at this draw's start (realize_display_state).
    // Returns false only for a Present that DXGI rejected because a fullscreen
    // transition it made itself (focus loss, occlusion) is not yet realized by
    // ResizeBuffers (flip model, SetFullscreenState remarks): that frame is
    // dropped and counted; its work is already fenced. Every other failure throws.
    bool present_to_swap_chain(const char *action)
    {
        const bool tearing = !options.vsync && telemetry.tearing_supported && !exclusive_active;
        const auto trace_begin = host_frame_trace::present_begin(); // NEW HOST (R34) diagnostics, 0 when off
        const auto status = swap->Present(options.vsync ? 1 : 0, tearing ? DXGI_PRESENT_ALLOW_TEARING : 0);
        host_frame_trace::present_end(swap.Get(), trace_begin, status, options.vsync ? 1u : 0u, tearing);
        occluded = status == DXGI_STATUS_OCCLUDED;
        if (occluded)
            ++telemetry.occluded_presents;
        if (FAILED(status) && !device_lost_status(status) && mode_switch_enabled)
        {
            (void)observe_fullscreen_state();
            if (swap_transition_pending)
            {
                ++telemetry.fullscreen_transition_drops;
                realize_transition_same_extent(); // unchanged extent: realized now; else by the owner's resize
                return false;
            }
        }
        hr(status, action);
        return true;
    }
    // NEW HOST (R24): reads the actual DXGI state; true when it changed since
    // the last observation (the change then still needs ResizeBuffers).
    bool observe_fullscreen_state()
    {
        if (!mode_switch_enabled)
            return false; // never fullscreen: DXGI Alt+Enter is disabled
        BOOL fullscreen = FALSE;
        hr(swap->GetFullscreenState(&fullscreen, nullptr), "GetFullscreenState");
        if ((fullscreen != FALSE) == exclusive_active)
            return false;
        exclusive_active = fullscreen != FALSE;
        telemetry.exclusive_fullscreen = exclusive_active;
        swap_transition_pending = true;
        ++telemetry.fullscreen_transitions;
        return true;
    }
    // Client extent of the swap-chain window; nullopt while minimized or empty.
    std::optional<std::array<unsigned, 2>> client_extent() const
    {
        RECT client{};
        if (!GetClientRect(window, &client))
            hr(HRESULT_FROM_WIN32(GetLastError()), "swap-chain window client extent");
        if (IsIconic(window) || client.right <= 0 || client.bottom <= 0)
            return std::nullopt;
        return std::array<unsigned, 2>{static_cast<unsigned>(client.right), static_cast<unsigned>(client.bottom)};
    }
    // A pending transition at an unchanged extent is realized here (image roles
    // kept). A changed extent is left to the owner's resize path (WM_SIZE ->
    // resize()/restore), which also reconciles the image roles.
    void realize_transition_same_extent()
    {
        if (!swap_transition_pending || polygon_submission_unfenced)
            return;
        const auto extent = client_extent();
        if (extent && (*extent)[0] == telemetry.width && (*extent)[1] == telemetry.height)
            resize(telemetry.width, telemetry.height);
    }
    // Draw-start safe point: before this draw's context/list, never while recording.
    void realize_display_state()
    {
        if (!mode_switch_enabled)
            return;
        (void)observe_fullscreen_state();
        realize_transition_same_extent();
    }
    // After a transition this renderer requested: ResizeBuffers at the new
    // client extent, required by the flip model even if the extent is unchanged.
    void realize_requested_transition()
    {
        swap_transition_pending = true;
        if (const auto extent = client_extent())
            resize((*extent)[0], (*extent)[1]);
        // Minimized: the next resize() (WM_SIZE on restore) realizes it.
    }
    bool enter_exclusive_fullscreen()
    {
        require_surface_thread();
        if (!mode_switch_enabled)
            throw FormatError("exclusive fullscreen requires a host-graphics swap chain (ALLOW_MODE_SWITCH)");
        if (polygon_submission_unfenced)
            throw FormatError("unfenced polygon work requires renderer teardown");
        check_device();
        (void)observe_fullscreen_state();
        if (!exclusive_active)
        {
            ComPtr<IDXGIOutput> output;
            const auto located = swap->GetContainingOutput(&output);
            if (located == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
                return false;
            hr(located, "exclusive fullscreen output");
            const auto mode = select_exclusive_mode(*output.Get());
            // DXGI best practices: ResizeTarget BEFORE SetFullscreenState (one
            // mode change, one WM_SIZE), then ResizeTarget again with a zeroed
            // refresh rate once in fullscreen (a no-op avoiding refresh issues).
            auto requested = mode;
            const auto targeted = swap->ResizeTarget(&requested);
            if (targeted == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
                return false;
            hr(targeted, "ResizeTarget (exclusive mode)");
            const auto entered = swap->SetFullscreenState(TRUE, output.Get());
            if (entered == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE || entered == DXGI_STATUS_MODE_CHANGE_IN_PROGRESS)
            {
                // Documented "try again later": keep whatever state DXGI has
                // reached, realize it, and let the caller retry.
                (void)observe_fullscreen_state();
                if (swap_transition_pending)
                    realize_requested_transition();
                return exclusive_active;
            }
            hr(entered, "SetFullscreenState(TRUE)");
            DXGI_MODE_DESC settle = mode;
            settle.RefreshRate = {0, 0};
            hr(swap->ResizeTarget(&settle), "ResizeTarget (settle refresh rate)");
            telemetry.exclusive_width = mode.Width;
            telemetry.exclusive_height = mode.Height;
            telemetry.exclusive_refresh_numerator = mode.RefreshRate.Numerator;
            telemetry.exclusive_refresh_denominator = mode.RefreshRate.Denominator;
            (void)observe_fullscreen_state();
        }
        realize_requested_transition();
        return exclusive_active;
    }
    bool leave_exclusive_fullscreen()
    {
        require_surface_thread();
        if (!mode_switch_enabled)
            return true; // such a swap chain is never fullscreen
        if (polygon_submission_unfenced)
            throw FormatError("unfenced polygon work requires renderer teardown");
        (void)observe_fullscreen_state();
        if (exclusive_active)
        {
            const auto left = swap->SetFullscreenState(FALSE, nullptr);
            if (left == DXGI_STATUS_MODE_CHANGE_IN_PROGRESS)
            {
                (void)observe_fullscreen_state();
                if (swap_transition_pending)
                    realize_requested_transition();
                return !exclusive_active;
            }
            hr(left, "SetFullscreenState(FALSE)");
            (void)observe_fullscreen_state();
        }
        if (swap_transition_pending)
            realize_requested_transition();
        return !exclusive_active;
    }
    bool exclusive_fullscreen()
    {
        require_surface_thread();
        if (!mode_switch_enabled)
            return false;
        (void)observe_fullscreen_state();
        realize_transition_same_extent();
        return exclusive_active;
    }
    // NEW HOST (R28): HUD split / bloom plan of one gamma frame (host_post_process_d3d12.hpp),
    // before its list is reset. False (always with FXAA and bloom off) keeps the single pass.
    bool prepare_post_process(std::span<const PolygonRenderSegment> segments, bool fxaa,
                              const std::optional<PolygonLogicalRasterExtent> &canvas)
    {
        const bool bloom = options.host_graphics && graphics.bloom;
        if (!fxaa && !bloom)
            return false;
        if (!post_process)
            post_process = std::make_unique<HostPostProcessD3D12>(*device.Get(), timestamp_frequency);
        // NEW HOST (R30 perf_renderer): reused list (capacity kept; bank references
        // dropped on every exit), no per-frame allocation.
        auto &post_segments = post_segment_scratch;
        struct Release
        {
            std::vector<HostPostSegment> &list;
            ~Release()
            {
                list.clear();
            }
        } release{post_segments};
        post_segments.clear();
        post_segments.reserve(segments.size());
        for (const auto &segment : segments)
            post_segments.push_back({segment.commands, segment.initial, segment.textures});
        HostPostRequest request;
        request.fxaa = fxaa;
        request.bloom = bloom;
        request.width = telemetry.width;
        request.height = telemetry.height;
        request.samples = DXGI_SAMPLE_DESC{telemetry.samples, 0};
        request.color_format = target_format;
        request.resolve_format = target_format; // the ResolveSubresource format of this frame's final resolve
        request.canvas = canvas;
        request.quality = polygons().raster_quality();
        request.precise_vertex_mapping = polygons().precise_vertex_mapping();
        return post_process->prepare(post_segments, request);
    }
    // transient: NEW R19 host presentation (interpolated frames) that Presents
    // without touching front/back roles; admitted with or without a canvas.
    void draw_polygon_segments(std::span<PolygonRenderSegment> segments, const SdrGammaRamp *gamma = nullptr,
                               const std::shared_ptr<PolygonSurfaceImage> &output = {},
                               std::optional<PolygonLogicalRasterExtent> retained_canvas = std::nullopt,
                               bool transient = false)
    {
        require_targets();
        if (retained_canvas && !output && !transient)
            throw FormatError("retained raster canvas requires explicit image restoration");
        if (polygon_roles_active && !output && !transient)
            throw FormatError("direct presentation cannot bypass polygon front/back roles");
        realize_display_state(); // NEW HOST (R24) same safe point: fullscreen state for this draw's Present
        apply_host_graphics();   // NEW HOST (R23) safe point: before the context/list of this draw
        require_targets();
        bool fxaa = gamma != nullptr && fxaa_enabled(); // NEW HOST (R28): false once the HUD split ran it
        const bool dither = gamma != nullptr && smooth_gradients_enabled(); // NEW HOST (R27)
        const bool linear = linear_resolve();                               // NEW HOST (R27)
        if (!reverse_depth)
        {
            flush();
            create_depth(true);
        }
        if (!polygon_rtvs)
            create_polygon_targets();
        if (linear)
            create_linear_resolve_pipeline(); // NEW HOST (R27): once, before any recording
        const auto index = swap->GetCurrentBackBufferIndex();
        auto &frame = frames[index];
        // R19: no swap-slot wait. GPU-only slot resources (back buffer, MSAA,
        // depth, gamma input) are ordered by the single queue; CPU-written
        // storage is guarded by its own fence (context, gamma constants).
        auto &context = acquire_context("polygon frame allocator reset");
        const auto slot = context_slot(context);
        // NEW HOST (R30 perf_renderer): the retired (already cleared) list of this
        // context supplies its capacity; nothing else about the recordings changes.
        auto recordings = std::move(context.polygon_recordings);
        recordings.clear();
        recordings.reserve(segments.size());
        if (gamma)
        {
            ensure_gamma_backend();
            wait(frame.gamma_fence); // CPU rewrites this slot's gamma constants
            gamma_backend->prepare(frame.gamma, frame.back->GetDesc(), *gamma, fxaa, dither);
        }
        if (!output)
            prepare_frame_capture(frame);
        const bool post_split = gamma && prepare_post_process(segments, fxaa, retained_canvas); // NEW HOST (R28)
        // Every previous segment stays alive until its context fence retires.
        hr(list->Reset(context.allocator.Get(), nullptr), "polygon frame list reset");
        list->EndQuery(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 4);
        auto transition = barrier(frame.back.Get(), D3D12_RESOURCE_STATE_PRESENT,
                                  telemetry.samples > 1 && !linear ? D3D12_RESOURCE_STATE_RESOLVE_DEST
                                                                   : D3D12_RESOURCE_STATE_RENDER_TARGET);
        list->ResourceBarrier(1, &transition);
        const auto target = polygon_rtv(telemetry.samples > 1 ? frame_count : index);
        const auto ds = dsvs->GetCPUDescriptorHandleForHeapStart();
        const float black[]{0, 0, 0, 1};
        list->ClearRenderTargetView(target, black, 0, nullptr);
        list->ClearDepthStencilView(ds, D3D12_CLEAR_FLAG_DEPTH, 0, 0, 0, nullptr);
        list->OMSetRenderTargets(1, &target, FALSE, &ds);
        // NEW HOST (R28): the world_end marker of this frame runs the post chain on the 3D part.
        std::optional<HostPostProcessD3D12::Recording> post_recording;
        if (post_split)
        {
            HostPostTargets post_targets{telemetry.samples > 1 ? polygon_color.Get() : frame.back.Get(), target, ds};
            if (linear) // the R27 linear-light resolve also resolves the FXAA input
                post_targets.linear_resolve = {resolve_root.Get(), resolve_pipeline.Get(), resolve_srvs.Get()};
            post_recording.emplace(*post_process, *list.Get(), slot, post_targets);
        }
        std::size_t draws = 0, bytes = 0;
        try
        {
            // Consume only call-local bank references. record() retains actual
            // GPU objects; bank metadata must be releasable before Execute.
            for (std::size_t segment_index = 0; segment_index < segments.size(); ++segment_index)
            {
                auto &segment = segments[segment_index];
                // NEW HOST (R33): a GPU re-projected present records its base segments with their kept plans.
                auto recording = active_reprojection
                                     ? polygons().record_reprojected(
                                           *list.Get(), context_arena(context), *segment.commands, *segment.initial,
                                           std::move(segment.textures), telemetry.width, telemetry.height,
                                           retained_canvas, *active_reprojection, segment_index)
                                     : polygons().record(*list.Get(), context_arena(context), *segment.commands,
                                                         *segment.initial, std::move(segment.textures), telemetry.width,
                                                         telemetry.height, retained_canvas);
                if (segment.expected_final && !same_polygon_state(recording.final_state, *segment.expected_final))
                    throw FormatError("recorded segment final state differs from immutable capture");
                draws += recording.draws;
                bytes += recording.vertex_bytes + recording.index_bytes;
                recordings.push_back(std::move(recording));
            }
        }
        catch (...)
        {
            hr(list->Close(), "rejected polygon submission close");
            throw;
        }
        telemetry.draws = static_cast<unsigned>(draws);
        telemetry.instances = 0;
        telemetry.live_vertex_bytes = bytes;
        ++telemetry.live_updates;
        // Publication is before Execute. Failed Present/Signal must retain ALL
        // descriptors, uploads, pipelines and textures referenced by this list.
        if (post_recording)
            fxaa = !post_recording->finish(recordings) && fxaa; // NEW HOST (R28): FXAA already ran without the HUD
        context.polygon_recordings = std::move(recordings);
        context.polygon_image = output;
        if (linear)
        {
            // NEW HOST (R27): linear-light resolve into the back buffer (see linear_resolve_shader).
            SGN_PROFILE_SCOPE("gpu.linear_resolve");
            transition = barrier(polygon_color.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            list->ResourceBarrier(1, &transition);
            const auto back_target = polygon_rtv(index);
            list->OMSetRenderTargets(1, &back_target, FALSE, nullptr);
            const D3D12_VIEWPORT whole{0, 0, static_cast<float>(telemetry.width), static_cast<float>(telemetry.height),
                                       0, 1};
            const D3D12_RECT rect{0, 0, static_cast<LONG>(telemetry.width), static_cast<LONG>(telemetry.height)};
            list->RSSetViewports(1, &whole);
            list->RSSetScissorRects(1, &rect);
            list->SetGraphicsRootSignature(resolve_root.Get());
            list->SetPipelineState(resolve_pipeline.Get());
            ID3D12DescriptorHeap *resolve_heaps[]{resolve_srvs.Get()};
            list->SetDescriptorHeaps(1, resolve_heaps);
            list->SetGraphicsRootDescriptorTable(0, resolve_srvs->GetGPUDescriptorHandleForHeapStart());
            const std::uint32_t resolve_constants[4]{telemetry.samples, 0, 0, 0};
            list->SetGraphicsRoot32BitConstants(1, 4, resolve_constants, 0);
            list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            list->DrawInstanced(3, 1, 0, 0);
            transition = barrier(polygon_color.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                 D3D12_RESOURCE_STATE_RENDER_TARGET);
            list->ResourceBarrier(1, &transition);
        }
        else if (telemetry.samples > 1)
        {
            transition =
                barrier(polygon_color.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
            list->ResourceBarrier(1, &transition);
            list->ResolveSubresource(frame.back.Get(), 0, polygon_color.Get(), 0, target_format);
            transition =
                barrier(polygon_color.Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
            list->ResourceBarrier(1, &transition);
        }
        auto completed_state =
            telemetry.samples > 1 && !linear ? D3D12_RESOURCE_STATE_RESOLVE_DEST : D3D12_RESOURCE_STATE_RENDER_TARGET;
        if (gamma)
        {
            if (fxaa)
            {
                // NEW HOST (R23): FXAA on the resolved image, combined with the
                // exact ramp lookup in the same full-screen pass (whole frame, HUD included).
                SGN_PROFILE_SCOPE("gpu.fxaa");
                list->EndQuery(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 4 + 2);
                gamma_backend->record(*list.Get(), frame.gamma, *frame.back.Get(), completed_state, polygon_rtv(index),
                                      telemetry.width, telemetry.height, true, dither);
                list->EndQuery(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 4 + 3);
            }
            else
                gamma_backend->record(*list.Get(), frame.gamma, *frame.back.Get(), completed_state, polygon_rtv(index),
                                      telemetry.width, telemetry.height, false, dither);
            completed_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
        }
        if (output)
        {
            const D3D12_RESOURCE_BARRIER to_copy[]{
                barrier(frame.back.Get(), completed_state, D3D12_RESOURCE_STATE_COPY_SOURCE),
                barrier(output->pixels.Get(), output->state, D3D12_RESOURCE_STATE_COPY_DEST)};
            list->ResourceBarrier(output->state == D3D12_RESOURCE_STATE_COPY_DEST ? 1u : 2u,
                                  to_copy); // pooled image re-entry
            list->CopyResource(output->pixels.Get(), frame.back.Get());
            const D3D12_RESOURCE_BARRIER copied[]{
                barrier(output->pixels.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE),
                barrier(frame.back.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT)};
            list->ResourceBarrier(2, copied);
            // Do not overwrite the last physical presentation's readback.
        }
        else
            record_frame_capture(frame, completed_state);
        list->EndQuery(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 4 + 1);
        list->ResolveQueryData(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 4, 2, timestamp_readback.Get(),
                               slot * 32);
        if (fxaa)
            list->ResolveQueryData(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 4 + 2, 2,
                                   timestamp_readback.Get(), slot * 32 + 16);
        hr(list->Close(), "polygon window frame close");
        if (before_polygon_submit)
        {
            auto callback = std::exchange(before_polygon_submit, {});
            callback();
        }
        if (next_fence == (std::numeric_limits<std::uint64_t>::max)())
            throw FormatError("polygon fence space exhausted");
        const auto retirement = next_fence++;
        polygon_submission_unfenced = true;
        ID3D12CommandList *lists[]{list.Get()};
        {
            SGN_PROFILE_SCOPE("gpu.execute");
            queue->ExecuteCommandLists(1, lists);
        }
        if (output)
            output->state = D3D12_RESOURCE_STATE_COPY_SOURCE;
        // If Signal fails, prohibit further draw/upload/resize reuse. The
        // existing destructor flush loop waits for retirement or confirmed
        // device removal before member destruction releases these resources.
        // New diagnostic: queued work exists but has no frame retirement fence.
        if (consume_failure(RenderFailurePoint::polygon_frame_signal) || (output && output->inject_signal_failure))
            hr(E_FAIL, "injected polygon frame signal");
        hr(queue->Signal(fence.Get(), retirement), "polygon frame signal");
        context.fence = retirement;
        context.timed = true;
        context.fxaa_timed = fxaa;
        if (gamma)
            frame.gamma_fence = retirement;
        polygon_submission_unfenced = false;
        if (output)
        {
            output->fence = retirement;
            return;
        }
        frame.fence = retirement;
        present_polygon_frame(index, gamma != nullptr);
    }
    void draw_polygons(const CapturedPolygonFrame &captured, const PolygonDeviceState &initial,
                       std::shared_ptr<const PolygonGpuTextures> bindings)
    {
        // Preserve the existing single-frame API and its hand-authored frames.
        PolygonRenderSegment segment{&captured, &initial, std::move(bindings), nullptr};
        draw_polygon_segments(std::span{&segment, std::size_t(1)});
    }
    void validate_submission(const CapturedPolygonSubmission &submission)
    {
        SGN_PROFILE_SCOPE("gpu.validate_submission");
        (void)validate_polygon_submission(submission);
    }
    // prevalidated: NEW R19. The immutable submission already passed the same
    // validation earlier in this call chain; repeating it cannot fail.
    void draw_polygon_submission(const CapturedPolygonSubmission &submission, const SdrGammaRamp *gamma = nullptr,
                                 const std::shared_ptr<PolygonSurfaceImage> &output = {},
                                 std::optional<PolygonLogicalRasterExtent> retained_canvas = std::nullopt,
                                 bool prevalidated = false, bool transient = false)
    {
        // Includes global (not per-segment) draw/upload budgets, exact CPU
        // event/command/state continuity, and the actual ordered resource banks.
        if (!prevalidated)
            validate_submission(submission);
        // NEW HOST (R30 perf_renderer): reused list (capacity kept; bank references
        // dropped on every exit), no per-frame allocation.
        auto &segments = segment_scratch;
        struct Release
        {
            std::vector<PolygonRenderSegment> &list;
            ~Release()
            {
                list.clear();
            }
        } release{segments};
        segments.clear();
        segments.reserve(submission.segments.size());
        for (const auto &segment : submission.segments)
        {
            const auto &frame = *segment.device_frame;
            segments.push_back(
                {frame.polygon_commands.get(), &frame.initial.polygon, segment.textures, &frame.final.polygon});
        }
        draw_polygon_segments(segments, gamma, output, retained_canvas, transient);
    }
    std::optional<PolygonLogicalRasterExtent> retained_polygon_canvas(const CapturedPolygonSubmission &submission)
    {
        validate_submission(submission);
        std::optional<PolygonLogicalRasterExtent> logical;
        for (const auto &segment : submission.segments)
            for (const auto &command : segment.device_frame->polygon_commands->commands)
            {
                if (const auto *draw = std::get_if<CapturedPolygonDraw>(&command))
                {
                    const auto w = draw->viewport.full_width, h = draw->viewport.full_height;
                    if (w <= 0 || h <= 0 || w > 16384 || h > 16384)
                        throw FormatError("retained submission has invalid logical canvas");
                    const PolygonLogicalRasterExtent next{static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h)};
                    if (logical && *logical != next)
                        throw FormatError("retained submission mixes logical canvas extents");
                    logical = next;
                }
            }
        // No draw command means no canvas to infer. The existing image path
        // still produces a physical clear with its captured gamma. Null roles
        // remain absent; zero-corner draw commands still declare their canvas.
        return logical;
    }
    std::shared_ptr<PolygonSurfaceImage> produce_polygon_image(
        const CapturedPolygonSubmission &submission, const SdrGammaRamp &gamma, bool fail_signal = false,
        std::optional<PolygonLogicalRasterExtent> retained_canvas = std::nullopt, bool prevalidated = false)
    {
        require_surface_thread();
        require_targets();
        check_device();
        // Validate before entering the new presentation mode or allocating.
        if (!prevalidated)
            validate_submission(submission);
        auto description = frames[swap->GetCurrentBackBufferIndex()].back->GetDesc();
        if (description.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || description.DepthOrArraySize != 1 ||
            description.MipLevels != 1 || description.SampleDesc.Count != 1 || description.Format != target_format ||
            description.Width != telemetry.width || description.Height != telemetry.height)
            throw FormatError("polygon surface image requires the current resolved RGBA8 target");
        description.Flags = D3D12_RESOURCE_FLAG_NONE;
        // NEW R19: a retired pooled allocation of the identical description,
        // or a fresh committed resource (same error text on failure).
        auto image = make_polygon_image(description);
        image->gamma = gamma;
        image->inject_signal_failure = fail_signal;
        polygon_roles_active = true;
        draw_polygon_submission(submission, &image->gamma, image, retained_canvas, true);
        return image;
    }
    std::uint64_t stage_polygon_back(const CapturedPolygonSubmission &submission, const SdrGammaRamp &gamma)
    {
        auto image = produce_polygon_image(submission, gamma);
        const auto identity = image->fence;
        polygon_back = std::move(image);
        return identity; // Nonthrowing after successful Signal.
    }
    PolygonSurfaceRoles restore_polygon_surfaces(const CapturedPolygonSubmission *front, const SdrGammaRamp &front_ramp,
                                                 const CapturedPolygonSubmission *back, const SdrGammaRamp &back_ramp)
    {
        require_surface_thread();
        require_targets();
        check_device();
        if (polygon_front || polygon_back)
            throw FormatError("surface restoration requires empty image roles");
        // Preflight BOTH complete recipes before producing either image.
        // Front/back may have DIFFERENT historical canvases; only draws within
        // one submission must agree across every resource segment.
        const auto front_canvas = front ? retained_polygon_canvas(*front) : std::nullopt;
        const auto back_canvas = back ? retained_polygon_canvas(*back) : std::nullopt;
        // Candidate owners are private until BOTH actual GPU submissions have
        // a retirement fence. A failure in back does not publish front alone.
        auto first = front ? produce_polygon_image(*front, front_ramp, false, front_canvas, true)
                           : std::shared_ptr<PolygonSurfaceImage>{};
        auto second = back ? produce_polygon_image(*back, back_ramp,
                                                   consume_failure(RenderFailurePoint::polygon_restore_back_signal),
                                                   back_canvas, true)
                           : std::shared_ptr<PolygonSurfaceImage>{};
        if (first)
            wait(first->fence);
        if (second)
            wait(second->fence);
        const PolygonSurfaceRoles result{true, first ? first->fence : 0, second ? second->fence : 0, telemetry.width,
                                         telemetry.height};
        polygon_front = std::move(first);
        polygon_back = std::move(second);
        polygon_roles_active = true;
        return result; // No throwing operation after publication; caller commits CPU pair next.
    }
    PolygonSurfaceFlip flip_polygon_surfaces()
    {
        require_surface_thread();
        require_targets();
        check_device();
        // NEW R19: no CPU wait on the back image fence. Exposure is ordered on
        // the GPU queue: the only consumer of the new front, the present copy,
        // is submitted after the stage on the same queue. Roles, fence ids and
        // results are unchanged; a device removal already visible is still
        // reported here by check_device() above.
        // Admission is NEW host policy, stronger than a historical HAL queue
        // acceptance. The source wrapper still calls it before BeginScene.
        polygon_roles_active = true;
        polygon_front.swap(polygon_back);
        return polygon_front ? PolygonSurfaceFlip::completed_front : PolygonSurfaceFlip::empty_front;
    }
    bool present_polygon_front()
    {
        require_surface_thread();
        require_targets();
        check_device();
        if (!polygon_roles_active)
            throw FormatError("polygon surface refresh requires explicit surface mode");
        realize_display_state(); // NEW HOST (R24) safe point before this list; an unchanged extent keeps the roles
        require_targets();
        if (!polygon_front)
            return false;
        const auto image = polygon_front;
        const auto description = image->pixels->GetDesc();
        if (description.Width != telemetry.width || description.Height != telemetry.height)
            throw FormatError("polygon front extent differs from current target");
        const auto index = swap->GetCurrentBackBufferIndex();
        auto &frame = frames[index];
        // R19: no swap-slot wait (no CPU-written slot data on this path).
        auto &context = acquire_context("polygon front allocator reset");
        const auto slot = context_slot(context);
        prepare_frame_capture(frame);
        context.polygon_image = image;
        hr(list->Reset(context.allocator.Get(), nullptr), "polygon front list reset");
        list->EndQuery(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 4);
        auto transition = barrier(frame.back.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
        list->ResourceBarrier(1, &transition);
        list->CopyResource(frame.back.Get(), image->pixels.Get());
        record_frame_capture(frame, D3D12_RESOURCE_STATE_COPY_DEST);
        list->EndQuery(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 4 + 1);
        list->ResolveQueryData(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 4, 2, timestamp_readback.Get(),
                               slot * 32);
        hr(list->Close(), "polygon front list close");
        if (before_polygon_submit)
        {
            auto callback = std::exchange(before_polygon_submit, {});
            callback();
        }
        if (next_fence == (std::numeric_limits<std::uint64_t>::max)())
            throw FormatError("polygon fence space exhausted");
        const auto retirement = next_fence++;
        polygon_submission_unfenced = true;
        ID3D12CommandList *lists[]{list.Get()};
        {
            SGN_PROFILE_SCOPE("gpu.execute");
            queue->ExecuteCommandLists(1, lists);
        }
        if (consume_failure(RenderFailurePoint::polygon_frame_signal))
            hr(E_FAIL, "injected polygon front signal");
        hr(queue->Signal(fence.Get(), retirement), "polygon front signal");
        context.fence = retirement;
        context.timed = true;
        frame.fence = retirement;
        polygon_submission_unfenced = false;
        telemetry.draws = 0;
        telemetry.instances = 0;
        telemetry.live_vertex_bytes = 0;
        present_polygon_frame(index, true);
        return true;
    }
    std::shared_ptr<const GpuStaticBuffer> create_static_buffer(std::span<const std::byte> bytes,
                                                                D3D12_RESOURCE_STATES final_state)
    {
        require_surface_thread();
        if (bytes.empty() || bytes.size() > (std::size_t(1) << 31))
            throw FormatError("static GPU buffer size outside 1..2 GiB");
        auto upload = make_buffer(bytes.size(), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        void *mapped{};
        D3D12_RANGE empty{};
        hr(upload->Map(0, &empty, &mapped), "static buffer upload map");
        std::memcpy(mapped, bytes.data(), bytes.size());
        upload->Unmap(0, nullptr);
        auto result = std::make_shared<GpuStaticBuffer>();
        result->resource = make_buffer(bytes.size(), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
        result->address = result->resource->GetGPUVirtualAddress();
        result->bytes = bytes.size();
        begin_upload();
        list->CopyBufferRegion(result->resource.Get(), 0, upload.Get(), 0, bytes.size());
        if (final_state != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            const auto ready = barrier(result->resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, final_state);
            list->ResourceBarrier(1, &ready);
        }
        end_upload();
        return result;
    }
    // NEW R22: block until the back buffer the next Present renders into has
    // retired its previous submission (the same fences draw_polygon_segments
    // would wait for), so a caller can sample time after the wait.
    // NEW HOST (R33b): non-blocking readiness of the next transient present: the back
    // buffer it renders into (its previous frame and gamma work) and the next command
    // context have retired their GPU work, so neither wait below would block.
    bool transient_present_ready() noexcept
    {
        if (!targets_ready || !swap || !fence)
            return false;
        const auto completed = fence->GetCompletedValue();
        if (completed == UINT64_MAX)
            return false; // removal: the blocking path reports it
        const auto &frame = frames[swap->GetCurrentBackBufferIndex()];
        return (std::max)(frame.fence, frame.gamma_fence) <= completed && contexts[next_context].fence <= completed;
    }
    void wait_transient_present_buffer()
    {
        require_surface_thread();
        require_targets();
        check_device();
        const auto &frame = frames[swap->GetCurrentBackBufferIndex()];
        wait((std::max)(frame.fence, frame.gamma_fence));
    }
    void present_polygon_submission_transient(const CapturedPolygonSubmission &submission, const SdrGammaRamp &gamma,
                                              bool fit_canvas)
    {
        require_surface_thread();
        std::optional<PolygonLogicalRasterExtent> canvas;
        bool validated = false;
        if (fit_canvas)
        {
            canvas = retained_polygon_canvas(submission);
            validated = true;
        }
        draw_polygon_submission(submission, &gamma, {}, canvas, validated, true);
    }
    // NEW HOST (R30 perf_renderer): the same presentation of a SHARED immutable
    // submission. Its CPU admission (validate_polygon_submission and, fitted, the
    // canvas inference) is a pure function of the immutable submission graph, so
    // an object admitted once is admitted identically again: it is remembered
    // per object. Identity is proven by the weak reference (the same control
    // block, still alive, the same pointer); it never extends the lifetime. The
    // presenter shows one capture several times (at N, pair end, first frame,
    // cut); its interpolated copies are new objects and are validated in full.
    // NEW HOST (R33) GPU re-projected presentation of an interpolated frame: the
    // BASE capture (admitted once per object, like the shared overload below) is
    // recorded with the polygon backend's kept per-base plans, and every draw with a
    // moving slot is re-projected by the GPU (polygon_reprojection.hpp). Everything
    // else of the presentation (gamma, FXAA, bloom/HUD split, MSAA and linear
    // resolve, frame capture, Present) is the same draw_polygon_segments path.
    void present_polygon_reprojected(const std::shared_ptr<const CapturedPolygonSubmission> &submission,
                                     const PolygonReprojection &reprojection, const SdrGammaRamp &gamma,
                                     bool fit_canvas)
    {
        require_surface_thread();
        if (!submission || !reprojection.base || reprojection.base->submission != submission)
            throw FormatError("re-projected presentation requires its base submission");
        std::optional<PolygonLogicalRasterExtent> canvas;
        bool admitted_before = false;
        for (const auto &admitted : transient_admissions)
            if (admitted.fit_canvas == fit_canvas && !admitted.submission.owner_before(submission) &&
                !submission.owner_before(admitted.submission) && admitted.submission.lock() == submission)
            {
                canvas = admitted.canvas;
                admitted_before = true;
                break;
            }
        if (!admitted_before)
        {
            if (fit_canvas)
                canvas = retained_polygon_canvas(*submission);
            else
                validate_submission(*submission);
            transient_admissions[next_transient_admission] = {submission, canvas, fit_canvas};
            next_transient_admission = (next_transient_admission + 1) % transient_admissions.size();
        }
        struct Active
        {
            const PolygonReprojection *&slot;
            ~Active()
            {
                slot = nullptr;
            }
        } active{active_reprojection};
        active_reprojection = &reprojection;
        const PolygonReprojectionScope post_process_scope(&reprojection); // emissive copies (host_post_process)
        draw_polygon_submission(*submission, &gamma, {}, canvas, true, true);
    }
    void present_polygon_submission_transient(const std::shared_ptr<const CapturedPolygonSubmission> &submission,
                                              const SdrGammaRamp &gamma, bool fit_canvas)
    {
        require_surface_thread();
        if (!submission)
            throw FormatError("transient presentation requires a submission");
        for (const auto &admitted : transient_admissions)
            if (admitted.fit_canvas == fit_canvas && !admitted.submission.owner_before(submission) &&
                !submission.owner_before(admitted.submission) && admitted.submission.lock() == submission)
            {
                sgn::host_profile::add("gpu.transient_admission_reused", 0);
                draw_polygon_submission(*submission, &gamma, {}, admitted.canvas, true, true);
                return;
            }
        std::optional<PolygonLogicalRasterExtent> canvas;
        if (fit_canvas)
            canvas = retained_polygon_canvas(*submission);
        else
            validate_submission(*submission);
        transient_admissions[next_transient_admission] = {submission, canvas, fit_canvas};
        next_transient_admission = (next_transient_admission + 1) % transient_admissions.size();
        draw_polygon_submission(*submission, &gamma, {}, canvas, true, true);
    }
    PolygonSurfaceRoles polygon_surface_roles() const
    {
        require_surface_thread();
        return {polygon_roles_active, polygon_front ? polygon_front->fence : 0, polygon_back ? polygon_back->fence : 0,
                telemetry.width, telemetry.height};
    }
    void create_pipelines()
    {
        D3D12_DESCRIPTOR_RANGE range{};
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.NumDescriptors = 1;
        range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        std::array<D3D12_ROOT_PARAMETER, 2> params{};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[0].Constants.Num32BitValues = 44;
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[1].DescriptorTable.NumDescriptorRanges = 1;
        params[1].DescriptorTable.pDescriptorRanges = &range;
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_ANISOTROPIC;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.MaxAnisotropy = 16;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC rs{};
        rs.NumParameters = 2;
        rs.pParameters = params.data();
        rs.NumStaticSamplers = 1;
        rs.pStaticSamplers = &sampler;
        rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        ComPtr<ID3DBlob> serialized, error;
        hr(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error),
           "serialize root signature");
        hr(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
                                       IID_PPV_ARGS(&root)),
           "root signature");
        auto vs = compile("vs", "vs_5_1"), ps = compile("ps", "ps_5_1");
        D3D12_INPUT_ELEMENT_DESC elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
        p.pRootSignature = root.Get();
        p.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
        p.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        p.InputLayout = {elements, 4};
        p.SampleMask = UINT_MAX;
        p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        p.NumRenderTargets = 1;
        p.RTVFormats[0] = srgb_format;
        p.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        p.SampleDesc.Count = telemetry.samples;
        p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        p.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        p.RasterizerState.DepthClipEnable = TRUE;
        p.RasterizerState.MultisampleEnable = TRUE;
        p.DepthStencilState.DepthEnable = TRUE;
        p.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        p.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        auto &blend = p.BlendState.RenderTarget[0];
        blend.SrcBlend = D3D12_BLEND_ONE;
        blend.DestBlend = D3D12_BLEND_ZERO;
        blend.BlendOp = D3D12_BLEND_OP_ADD;
        blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.DestBlendAlpha = D3D12_BLEND_ZERO;
        blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        hr(device->CreateGraphicsPipelineState(&p, IID_PPV_ARGS(&opaque)), "opaque pipeline");
        p.RasterizerState.FrontCounterClockwise = TRUE;
        hr(device->CreateGraphicsPipelineState(&p, IID_PPV_ARGS(&opaque_mirrored)), "mirrored opaque pipeline");
        p.RasterizerState.FrontCounterClockwise = FALSE;
        p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        hr(device->CreateGraphicsPipelineState(&p, IID_PPV_ARGS(&opaque_two_sided)), "opaque two-sided pipeline");
        p.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        blend.BlendEnable = TRUE;
        blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        p.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        hr(device->CreateGraphicsPipelineState(&p, IID_PPV_ARGS(&transparent)), "transparent pipeline");
        p.RasterizerState.FrontCounterClockwise = TRUE;
        hr(device->CreateGraphicsPipelineState(&p, IID_PPV_ARGS(&transparent_mirrored)),
           "mirrored transparent pipeline");
        p.RasterizerState.FrontCounterClockwise = FALSE;
        p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        hr(device->CreateGraphicsPipelineState(&p, IID_PPV_ARGS(&transparent_two_sided)),
           "transparent two-sided pipeline");
        p.DepthStencilState.DepthEnable = FALSE;
        hr(device->CreateGraphicsPipelineState(&p, IID_PPV_ARGS(&overlay_pipeline)), "overlay pipeline");
        p.DepthStencilState.DepthEnable = TRUE;
        p.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        for (unsigned i = 0; i < source_pipelines.size(); ++i)
        {
            const bool translucent = i >= 3;
            p.RasterizerState.CullMode = i % 3 == 2 ? D3D12_CULL_MODE_NONE : D3D12_CULL_MODE_BACK;
            p.RasterizerState.FrontCounterClockwise = i % 3 == 1;
            p.DepthStencilState.DepthWriteMask = translucent ? D3D12_DEPTH_WRITE_MASK_ZERO : D3D12_DEPTH_WRITE_MASK_ALL;
            blend.BlendEnable = translucent;
            hr(device->CreateGraphicsPipelineState(&p, IID_PPV_ARGS(&source_pipelines[i])), "source viewport pipeline");
        }
        // New GPU equivalent of authored one-/two-sided face selection.
        // Keep4338E0 blending separate from inspection lighting. The old
        // device used CULL_NONE only after its CPU face rejection step.
        for (unsigned i = 0; i < live_pipelines.size(); ++i)
        {
            const auto mode = static_cast<LiveBlend>((i / 3) % 5);
            const bool translucent = mode != LiveBlend::opaque;
            p.RasterizerState.CullMode = i % 3 == 2 ? D3D12_CULL_MODE_NONE : D3D12_CULL_MODE_BACK;
            p.RasterizerState.FrontCounterClockwise = i % 3 == 1;
            p.DepthStencilState.DepthFunc =
                i >= 15 ? D3D12_COMPARISON_FUNC_GREATER_EQUAL : D3D12_COMPARISON_FUNC_LESS_EQUAL;
            p.DepthStencilState.DepthWriteMask = translucent ? D3D12_DEPTH_WRITE_MASK_ZERO : D3D12_DEPTH_WRITE_MASK_ALL;
            blend.BlendEnable = translucent;
            blend.SrcBlend = mode == LiveBlend::multiply
                                 ? D3D12_BLEND_DEST_COLOR
                                 : (mode == LiveBlend::alpha || mode == LiveBlend::alpha_add ? D3D12_BLEND_SRC_ALPHA
                                                                                             : D3D12_BLEND_ONE);
            blend.DestBlend =
                mode == LiveBlend::alpha
                    ? D3D12_BLEND_INV_SRC_ALPHA
                    : (mode == LiveBlend::add || mode == LiveBlend::alpha_add ? D3D12_BLEND_ONE : D3D12_BLEND_ZERO);
            blend.SrcBlendAlpha = D3D12_BLEND_ONE;
            blend.DestBlendAlpha = D3D12_BLEND_ZERO;
            hr(device->CreateGraphicsPipelineState(&p, IID_PPV_ARGS(&live_pipelines[i])),
               "live source material pipeline");
        }
        pipeline_samples = telemetry.samples; // NEW HOST (R23): all inspection PSOs match the targets
    }
    void begin_upload()
    {
        if (polygon_submission_unfenced)
            throw FormatError("unfenced polygon work requires renderer teardown");
        flush();
        hr(upload_allocator->Reset(), "upload allocator reset");
        hr(list->Reset(upload_allocator.Get(), nullptr), "upload list reset");
    }
    void end_upload()
    {
        hr(list->Close(), "upload close");
        ID3D12CommandList *lists[] = {list.Get()};
        queue->ExecuteCommandLists(1, lists);
        try
        {
            flush();
        }
        catch (...)
        {
            // The upload caller still owns its staging/destination/readback
            // resources. In particular create_font runs inside Impl's
            // constructor, whose destructor is not called on construction
            // failure. Retire submitted work before those locals can unwind.
            // Keep the original Signal/Wait error, even if retirement needed
            // another native fence or confirmed that the device was removed.
            retire_submitted_work_noexcept();
            throw;
        }
    }
    ComPtr<ID3D12Resource> upload_texture(const std::vector<Image> &levels, unsigned slot,
                                          std::vector<ComPtr<ID3D12Resource>> &staging)
    {
        if (slot >= max_textures)
            throw std::runtime_error("texture descriptor capacity exceeded");
        const auto &image = levels.front();
        D3D12_RESOURCE_DESC d{};
        d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        d.Width = image.width;
        d.Height = image.height;
        d.DepthOrArraySize = 1;
        d.MipLevels = static_cast<UINT16>(levels.size());
        d.Format = target_format;
        d.SampleDesc.Count = 1;
        auto h = heap(D3D12_HEAP_TYPE_DEFAULT);
        ComPtr<ID3D12Resource> result;
        hr(device->CreateCommittedResource(&h, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                           IID_PPV_ARGS(&result)),
           "texture resource");
        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(levels.size());
        UINT64 total{};
        device->GetCopyableFootprints(&d, 0, static_cast<UINT>(levels.size()), 0, layouts.data(), nullptr, nullptr,
                                      &total);
        auto upload =
            make_buffer(static_cast<std::size_t>(total), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        void *mapped{};
        D3D12_RANGE empty{};
        hr(upload->Map(0, &empty, &mapped), "texture upload map");
        for (unsigned i = 0; i < levels.size(); ++i)
        {
            const auto &level = levels[i];
            for (unsigned y = 0; y < level.height; ++y)
                std::memcpy(static_cast<std::uint8_t *>(mapped) + layouts[i].Offset +
                                std::size_t(y) * layouts[i].Footprint.RowPitch,
                            level.rgba.data() + std::size_t(y) * level.width * 4, std::size_t(level.width) * 4);
            D3D12_TEXTURE_COPY_LOCATION dst{};
            dst.pResource = result.Get();
            dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.SubresourceIndex = i;
            D3D12_TEXTURE_COPY_LOCATION src{};
            src.pResource = upload.Get();
            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint = layouts[i];
            list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        }
        upload->Unmap(0, nullptr);
        auto b = barrier(result.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        list->ResourceBarrier(1, &b);
        D3D12_SHADER_RESOURCE_VIEW_DESC view{};
        view.Format = srgb_format;
        view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        view.Texture2D.MipLevels = d.MipLevels;
        device->CreateShaderResourceView(result.Get(), &view, srv_cpu(slot));
        staging.push_back(upload);
        return result;
    }
    static void validate_image(const Image &image)
    {
        if (!image.width || !image.height || image.width > 16384 || image.height > 16384 ||
            std::uint64_t(image.width) * image.height * 4 != image.rgba.size())
            throw std::invalid_argument("invalid replacement texture dimensions or pixels");
    }
    unsigned append_texture(const Image &image)
    {
        validate_image(image);
        if (textures.size() >= max_textures - 1)
            throw std::invalid_argument("source texture descriptor capacity exceeded");
        const auto slot = static_cast<unsigned>(textures.size());
        const auto levels = mip_chain(image);
        textures.reserve(textures.size() + 1);
        begin_upload();
        std::vector<ComPtr<ID3D12Resource>> staging;
        auto replacement = upload_texture(levels, slot, staging);
        end_upload();
        textures.push_back(std::move(replacement));
        for (const auto &level : levels)
            telemetry.uploaded_texture_bytes += level.rgba.size();
        return slot;
    }
    void replace_texture(unsigned slot, const Image &image)
    {
        validate_image(image);
        if (slot >= textures.size())
            throw std::out_of_range("replacement texture binding outside scene");
        const auto levels = mip_chain(image);
        begin_upload();
        std::vector<ComPtr<ID3D12Resource>> staging;
        auto replacement = upload_texture(levels, slot, staging);
        end_upload();
        textures[slot] = std::move(replacement);
        for (const auto &level : levels)
            telemetry.uploaded_texture_bytes += level.rgba.size();
    }
    Image read_texture(unsigned slot)
    {
        if (slot >= textures.size())
            throw std::out_of_range("texture readback binding outside scene");
        auto &texture = textures[slot];
        const auto desc = texture->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};
        UINT64 bytes{};
        device->GetCopyableFootprints(&desc, 0, 1, 0, &layout, nullptr, nullptr, &bytes);
        auto readback =
            make_buffer(static_cast<std::size_t>(bytes), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
        begin_upload();
        auto to_copy =
            barrier(texture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
        list->ResourceBarrier(1, &to_copy);
        D3D12_TEXTURE_COPY_LOCATION source{};
        source.pResource = texture.Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION target{};
        target.pResource = readback.Get();
        target.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        target.PlacedFootprint = layout;
        list->CopyTextureRegion(&target, 0, 0, 0, &source, nullptr);
        auto restore =
            barrier(texture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        list->ResourceBarrier(1, &restore);
        end_upload();
        Image image{static_cast<unsigned>(desc.Width), desc.Height, {}};
        image.rgba.resize(std::size_t(image.width) * image.height * 4);
        void *mapped{};
        const D3D12_RANGE range{0, static_cast<SIZE_T>(bytes)};
        hr(readback->Map(0, &range, &mapped), "texture readback map");
        for (unsigned y = 0; y < image.height; ++y)
            std::memcpy(image.rgba.data() + std::size_t(y) * image.width * 4,
                        static_cast<const std::uint8_t *>(mapped) + layout.Offset +
                            std::size_t(y) * layout.Footprint.RowPitch,
                        std::size_t(image.width) * 4);
        const D3D12_RANGE no_write{};
        readback->Unmap(0, &no_write);
        return image;
    }
    void create_font()
    {
        constexpr int width = 1024, height = 512;
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = width;
        bi.bmiHeader.biHeight = -height;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        HDC dc = CreateCompatibleDC(nullptr);
        if (!dc)
            throw std::runtime_error("font DC allocation failed");
        void *bits{};
        HBITMAP bitmap = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bitmap)
        {
            DeleteDC(dc);
            throw std::runtime_error("font bitmap allocation failed");
        }
        auto old_bitmap = SelectObject(dc, bitmap);
        HFONT typeface = CreateFontW(-40, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                     CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        if (!typeface)
        {
            SelectObject(dc, old_bitmap);
            DeleteObject(bitmap);
            DeleteDC(dc);
            throw std::runtime_error("font allocation failed");
        }
        auto old_font = SelectObject(dc, typeface);
        SetTextColor(dc, RGB(255, 255, 255));
        SetBkColor(dc, RGB(0, 0, 0));
        std::memset(bits, 0, width * height * 4);
        for (unsigned i = 0; i < 96; ++i)
        {
            wchar_t c = static_cast<wchar_t>(i + 32);
            TextOutW(dc, static_cast<int>(i % 16) * 64, static_cast<int>(i / 16) * 72, &c, 1);
            SIZE size{};
            GetTextExtentPoint32W(dc, &c, 1, &size);
            advance[i] = static_cast<float>(size.cx);
        }
        GdiFlush();
        Image image{width, height, Bytes(width * height * 4)};
        auto *pixels = static_cast<std::uint8_t *>(bits);
        for (std::size_t i = 0; i < image.rgba.size(); i += 4)
        {
            image.rgba[i] = image.rgba[i + 1] = image.rgba[i + 2] = 255;
            image.rgba[i + 3] = std::max({pixels[i], pixels[i + 1], pixels[i + 2]});
        }
        for (unsigned y = 500; y < 512; ++y)
            for (unsigned x = 1000; x < 1024; ++x)
                image.rgba[(y * width + x) * 4 + 3] = 255;
        SelectObject(dc, old_font);
        SelectObject(dc, old_bitmap);
        DeleteObject(typeface);
        DeleteObject(bitmap);
        DeleteDC(dc);
        begin_upload();
        std::vector<ComPtr<ID3D12Resource>> staging;
        font = upload_texture({std::move(image)}, max_textures - 1, staging);
        end_upload();
    }
    void load_scene(std::span<const Model> models, const AssetStore &store)
    {
        // Loading owns CPU decoding and allocation. Instance poses never rebuild meshes.
        if (models.empty() || models.size() > 1024)
            throw std::invalid_argument("scene mesh count outside 1..1024");
        std::vector<Vertex> combined;
        std::vector<Mesh> next_meshes;
        std::vector<std::vector<Image>> images;
        std::map<std::string, unsigned> texture_slots;
        std::uint64_t texture_bytes = 0;
        for (const auto &model : models)
        {
            if (model.vertices.size() > std::numeric_limits<UINT>::max() / sizeof(Vertex) - combined.size())
                throw std::invalid_argument("oversized scene mesh");
            std::vector<unsigned> remap;
            for (const auto &name : model.textures)
            {
                auto [it, inserted] = texture_slots.try_emplace(name, static_cast<unsigned>(images.size()));
                if (inserted)
                {
                    if (images.size() >= max_textures - 1)
                        throw std::invalid_argument("scene texture capacity exceeded");
                    images.push_back(mip_chain(store.texture(name)));
                    for (const auto &level : images.back())
                        texture_bytes += level.rgba.size();
                }
                remap.push_back(it->second);
            }
            Mesh mesh;
            mesh.parts = model.parts;
            mesh.texture_names = model.textures;
            mesh.texture_remap = remap;
            for (auto surface : model.surfaces)
            {
                if (surface.first > model.vertices.size() || surface.count > model.vertices.size() - surface.first ||
                    surface.texture >= remap.size() || surface.part >= model.parts)
                    throw std::invalid_argument("invalid scene surface");
                surface.first += static_cast<UINT>(combined.size());
                surface.texture = remap[surface.texture];
                mesh.surfaces.push_back(surface);
            }
            next_meshes.push_back(std::move(mesh));
            combined.insert(combined.end(), model.vertices.begin(), model.vertices.end());
        }
        begin_upload();
        std::vector<ComPtr<ID3D12Resource>> staging;
        std::vector<ComPtr<ID3D12Resource>> new_textures;
        for (unsigned i = 0; i < images.size(); ++i)
            new_textures.push_back(upload_texture(images[i], i, staging));
        // An empty live slot has no fabricated triangle. D3D12 still requires
        // nonzero buffer capacity before its first source-generated polygon.
        auto size = std::max(sizeof(Vertex), combined.size() * sizeof(Vertex));
        auto upload = make_buffer(size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        void *mapped{};
        D3D12_RANGE empty{};
        hr(upload->Map(0, &empty, &mapped), "vertex upload map");
        if (!combined.empty())
            std::memcpy(mapped, combined.data(), combined.size() * sizeof(Vertex));
        upload->Unmap(0, nullptr);
        auto new_vertices = make_buffer(size, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
        list->CopyBufferRegion(new_vertices.Get(), 0, upload.Get(), 0, size);
        auto b = barrier(new_vertices.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                         D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        list->ResourceBarrier(1, &b);
        end_upload();
        vertices = std::move(new_vertices);
        textures = std::move(new_textures);
        meshes = std::move(next_meshes);
        for (auto &frame : frames)
        {
            frame.live_upload.Reset();
            frame.live_vertices.Reset();
            frame.live_capacity = 0;
            frame.live_revision = 0;
            frame.live_view = {};
            frame.live_offsets.clear();
        }
        ++live_revision;
        vertex_view = {vertices->GetGPUVirtualAddress(), static_cast<UINT>(size), sizeof(Vertex)};
        telemetry.meshes = static_cast<unsigned>(meshes.size());
        telemetry.uploaded_vertex_bytes = size;
        telemetry.uploaded_texture_bytes = texture_bytes;
    }
    void stage_mesh(std::size_t slot, const LiveGeometry &geometry)
    {
        const auto &model = geometry.model;
        if (slot >= meshes.size())
            throw std::out_of_range("live mesh slot outside loaded scene");
        const auto &current = meshes[slot];
        if (model.parts != current.parts || model.textures != current.texture_names ||
            geometry.materials.size() != model.surfaces.size())
            throw std::invalid_argument("live mesh binding changed; reload scene resources first");
        if (model.vertices.size() > std::numeric_limits<UINT>::max() / sizeof(Vertex))
            throw std::length_error("live vertex capacity exceeded");
        for (const auto &v : model.vertices)
            for (float f : {v.position.x, v.position.y, v.position.z, v.normal.x, v.normal.y, v.normal.z, v.u, v.v})
                if (!std::isfinite(f))
                    throw std::invalid_argument("nonfinite live vertex");
        auto surfaces = model.surfaces;
        for (std::size_t i = 0; i < surfaces.size(); ++i)
        {
            auto &s = surfaces[i];
            const auto mode = geometry.materials[i].blend;
            if (s.first > model.vertices.size() || s.count > model.vertices.size() - s.first || s.count % 3 ||
                s.texture >= current.texture_remap.size() || s.part >= current.parts ||
                static_cast<unsigned>(mode) > static_cast<unsigned>(LiveBlend::multiply) ||
                s.translucent != (mode != LiveBlend::opaque))
                throw std::invalid_argument("invalid live surface or material");
            s.texture = current.texture_remap[s.texture];
        }
        auto vertices_copy = model.vertices;
        auto materials = geometry.materials;
        auto &next = meshes[slot];
        next.surfaces.swap(surfaces);
        next.live_vertices.swap(vertices_copy);
        next.live_materials.swap(materials);
        next.live = true;
        ++live_revision;
        ++telemetry.live_updates;
    }
    void upload_live(Frame &frame)
    {
        if (frame.live_revision == live_revision)
            return;
        std::size_t count = 0;
        frame.live_offsets.resize(meshes.size());
        for (std::size_t i = 0; i < meshes.size(); ++i)
        {
            frame.live_offsets[i] = static_cast<UINT>(count);
            if (meshes[i].live_vertices.size() > std::numeric_limits<UINT>::max() / sizeof(Vertex) - count)
                throw std::length_error("combined live vertex capacity exceeded");
            count += meshes[i].live_vertices.size();
        }
        const auto bytes = count * sizeof(Vertex);
        if (bytes)
        {
            bool allocated = false;
            if (bytes > frame.live_capacity)
            {
                const auto capacity =
                    std::max(bytes, std::min<std::size_t>(std::numeric_limits<UINT>::max(),
                                                          std::max<std::size_t>(4096, frame.live_capacity * 2)));
                auto upload = make_buffer(capacity, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
                auto target = make_buffer(capacity, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
                frame.live_upload = std::move(upload);
                frame.live_vertices = std::move(target);
                frame.live_capacity = capacity;
                telemetry.live_buffer_allocations += 2;
                allocated = true;
            }
            void *mapped{};
            D3D12_RANGE empty{};
            hr(frame.live_upload->Map(0, &empty, &mapped), "live geometry map");
            for (std::size_t i = 0; i < meshes.size(); ++i)
                if (!meshes[i].live_vertices.empty())
                    std::memcpy(static_cast<Vertex *>(mapped) + frame.live_offsets[i], meshes[i].live_vertices.data(),
                                meshes[i].live_vertices.size() * sizeof(Vertex));
            D3D12_RANGE written{0, bytes};
            frame.live_upload->Unmap(0, &written);
            if (!allocated)
            {
                auto b = barrier(frame.live_vertices.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
                                 D3D12_RESOURCE_STATE_COPY_DEST);
                list->ResourceBarrier(1, &b);
            }
            list->CopyBufferRegion(frame.live_vertices.Get(), 0, frame.live_upload.Get(), 0, bytes);
            auto b = barrier(frame.live_vertices.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                             D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            list->ResourceBarrier(1, &b);
            frame.live_view = {frame.live_vertices->GetGPUVirtualAddress(), static_cast<UINT>(bytes), sizeof(Vertex)};
            telemetry.live_vertex_bytes += bytes;
        }
        else
            frame.live_view = {};
        frame.live_revision = live_revision;
    }
    static void quad(std::vector<Vertex> &out, float x, float y, float w, float h, float u, float v, float uw, float vh,
                     std::uint32_t color)
    {
        if (out.size() + 6 > 8192)
            return;
        Vertex a{{x, y, 0}, {}, u, v, color}, b{{x + w, y, 0}, {}, u + uw, v, color},
            c{{x + w, y + h, 0}, {}, u + uw, v + vh, color}, d{{x, y + h, 0}, {}, u, v + vh, color};
        out.insert(out.end(), {a, b, c, a, c, d});
    }
    void text(std::vector<Vertex> &out, std::string_view value, float x, float y, float size, std::uint32_t tint)
    {
        float scale = size / 40;
        for (unsigned char c : value)
        {
            if (x >= static_cast<float>(telemetry.width) || out.size() + 6 > 8192)
                break;
            if (c < 32 || c > 126)
                c = '?';
            unsigned i = c - 32;
            quad(out, x, y, 64 * scale, 72 * scale, float(i % 16) * 64 / 1024, float(i / 16) * 72 / 512, 64.f / 1024,
                 72.f / 512, tint);
            x += advance[i] * scale;
        }
    }
    std::vector<std::string_view> text_lines(std::string_view value, float size, float available) const
    {
        std::vector<std::string_view> lines;
        while (!value.empty())
        {
            while (!value.empty() && value.front() == ' ')
                value.remove_prefix(1);
            if (value.empty())
                break;
            std::size_t end = 0, last_space = std::string_view::npos;
            float used = 0;
            while (end < value.size())
            {
                const auto c = static_cast<unsigned char>(value[end]);
                const auto glyph = c >= 32 && c <= 126 ? c : static_cast<unsigned char>('?');
                const float next = advance[glyph - 32] * size / 40;
                if (end && used + next > available)
                    break;
                used += next;
                if (c == ' ')
                    last_space = end;
                ++end;
            }
            if (end < value.size() && last_space != std::string_view::npos && last_space > 0)
                end = last_space;
            lines.push_back(value.substr(0, end));
            value.remove_prefix(end);
        }
        return lines;
    }
    void draw_scene(const View &camera_view, std::span<const SceneInstance> instances, std::string_view title,
                    std::string_view detail, float dpi, std::span<const LiveInstanceView> live_views = {})
    {
        require_targets();
        if (polygon_roles_active)
            throw FormatError("scene presentation cannot bypass polygon front/back roles");
        realize_display_state(); // NEW HOST (R24) safe point: fullscreen state for this frame's Present
        apply_host_graphics();   // NEW HOST (R23) safe point: before this frame's list
        if (pipeline_samples != telemetry.samples)
            create_pipelines(); // NEW HOST (R28b): inspection PSOs follow an MSAA change lazily
        require_targets();
        if (!vertices)
            return;
        const bool source_view = camera_view.retail_viewport.has_value();
        if (source_view)
        {
            if (!camera_view.world_to_view)
                throw std::invalid_argument("source viewport requires a recovered view matrix");
            const auto &v = *camera_view.retail_viewport;
            for (float value : {v.projection.x, v.projection.y, v.projection.z, v.projection.offset_z,
                                v.projection.clip_near, v.clip_far})
                if (!std::isfinite(value))
                    throw std::invalid_argument("nonfinite source viewport");
            if (v.projection.clip_near < 32 || v.clip_far <= v.projection.clip_near)
                throw std::invalid_argument("invalid source viewport clip range");
        }
        if (reverse_depth != source_view)
        {
            flush();
            create_depth(source_view);
        }
        validate_instances(instances, meshes.size());
        if (!live_views.empty() && live_views.size() != instances.size())
            throw std::invalid_argument("live view instance count mismatch");
        for (std::size_t i = 0; i < live_views.size(); ++i)
        {
            const auto &matrices = live_views[i].model_to_view;
            if (!matrices.empty() && matrices.size() != meshes[instances[i].mesh].parts)
                throw std::invalid_argument("live model-to-view part count mismatch");
            for (const auto &matrix : matrices)
                for (float f : matrix)
                    if (!std::isfinite(f))
                        throw std::invalid_argument("nonfinite live model-to-view matrix");
        }
        for (const auto &instance : instances)
            if (!instance.parts.empty() && instance.parts.size() != meshes[instance.mesh].parts)
                throw std::invalid_argument("scene part count does not match the uploaded mesh");
        auto index = swap->GetCurrentBackBufferIndex();
        auto &frame = frames[index];
        wait(frame.fence); // this slot's CPU-written overlay/live buffers
        auto &context = acquire_context("frame allocator reset");
        const auto slot = context_slot(context);
        prepare_frame_capture(frame);
        hr(list->Reset(context.allocator.Get(), opaque.Get()), "frame list reset");
        list->EndQuery(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 4);
        // This frame's fence was waited above. Other frames keep their buffers.
        upload_live(frame);
        auto b =
            barrier(frame.back.Get(), D3D12_RESOURCE_STATE_PRESENT,
                    telemetry.samples > 1 ? D3D12_RESOURCE_STATE_RESOLVE_DEST : D3D12_RESOURCE_STATE_RENDER_TARGET);
        list->ResourceBarrier(1, &b);
        auto target = rtv(telemetry.samples > 1 ? frame_count : index);
        auto ds = dsvs->GetCPUDescriptorHandleForHeapStart();
        const float clear[] = {0.012f, 0.018f, 0.028f, 1};
        list->ClearRenderTargetView(target, clear, 0, nullptr);
        list->ClearDepthStencilView(ds, D3D12_CLEAR_FLAG_DEPTH, reverse_depth ? 0.0f : 1.0f, 0, 0, nullptr);
        list->OMSetRenderTargets(1, &target, FALSE, &ds);
        D3D12_VIEWPORT viewport{0, 0, static_cast<float>(telemetry.width), static_cast<float>(telemetry.height), 0, 1};
        D3D12_RECT scissor{0, 0, static_cast<LONG>(telemetry.width), static_cast<LONG>(telemetry.height)};
        list->RSSetViewports(1, &viewport);
        list->RSSetScissorRects(1, &scissor);
        list->SetGraphicsRootSignature(root.Get());
        ID3D12DescriptorHeap *heaps[] = {srvs.Get()};
        list->SetDescriptorHeaps(1, heaps);
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        list->IASetVertexBuffers(0, 1, &vertex_view);
        const auto &e = camera_view.eye;
        const auto &t = camera_view.target;
        const auto &u = camera_view.up;
        auto view = XMMatrixIdentity();
        auto source_matrix = [](const std::array<float, 16> &source) {
            for (auto value : source)
                if (!std::isfinite(value))
                    throw std::invalid_argument("non-finite camera matrix");
            XMFLOAT4X4 matrix{};
            std::memcpy(&matrix, source.data(), sizeof(matrix));
            return XMLoadFloat4x4(&matrix);
        };
        if (camera_view.world_to_view)
            view = source_matrix(*camera_view.world_to_view);
        else
            view = XMMatrixLookAtLH(XMVectorSet(e.x, e.y, e.z, 1), XMVectorSet(t.x, t.y, t.z, 1),
                                    XMVectorSet(u.x, u.y, u.z, 0));
        float aspect = viewport.Width / viewport.Height;
        auto projection =
            XMMatrixPerspectiveFovLH(XMConvertToRadians(52), aspect, camera_view.near_plane, camera_view.far_plane);
        if (camera_view.source_projection)
        {
            projection = source_matrix(*camera_view.source_projection);
            // New widescreen policy: retain recovered vertical framing/depth.
            // This does not modify the original camera or its tick behavior.
            const auto &source = *camera_view.source_projection;
            projection.r[0] = XMVectorSetX(projection.r[0], std::copysign(std::abs(source[5]) / aspect, source[0]));
        }
        std::array<float, 44> constants{};
        if (source_view)
        {
            const auto &v = *camera_view.retail_viewport;
            const auto &p = v.projection;
            constants[35] = 1;
            constants[36] = p.x * (v.widen_horizontal ? (4.0f / 3.0f) / aspect : 1.0f);
            constants[37] = p.y;
            constants[38] = p.z;
            constants[39] = p.offset_z;
            constants[40] = p.clip_near;
            constants[41] = v.clip_far;
            constants[42] = 1.0f + 1.0f / viewport.Width;
            constants[43] = 1.0f + 1.0f / viewport.Height;
        }
        XMFLOAT4X4 matrix{};
        telemetry.draws = 0;
        telemetry.instances = static_cast<unsigned>(instances.size());
        for (bool translucent : {false, true})
            for (std::size_t instance_index = 0; instance_index < instances.size(); ++instance_index)
            {
                const auto &instance = instances[instance_index];
                const auto &mesh = meshes[instance.mesh];
                if (mesh.live && mesh.live_vertices.empty())
                    continue;
                list->IASetVertexBuffers(0, 1, mesh.live ? &frame.live_view : &vertex_view);
                for (std::size_t surface_index = 0; surface_index < mesh.surfaces.size(); ++surface_index)
                {
                    auto surface = mesh.surfaces[surface_index];
                    if (surface.translucent != translucent ||
                        (!instance.parts.empty() && !instance.parts[surface.part].visible))
                        continue;
                    const auto p = instance.parts.empty()
                                       ? instance.pose
                                       : compose_pose(instance.pose, instance.parts[surface.part].pose);
                    const bool mirrored = mirrored_pose(p);
                    auto world =
                        XMMatrixSet(p.right.x * p.scale, p.right.y * p.scale, p.right.z * p.scale, 0, p.up.x * p.scale,
                                    p.up.y * p.scale, p.up.z * p.scale, 0, p.forward.x * p.scale, p.forward.y * p.scale,
                                    p.forward.z * p.scale, 0, p.position.x, p.position.y, p.position.z, 1);
                    if (!live_views.empty() && !live_views[instance_index].model_to_view.empty())
                    {
                        // Full source44FBD0 matrix, already in camera space.
                        // Applying the world/camera matrices again changes both
                        // its coordinate system and original binary32 stores.
                        const auto ready = source_matrix(live_views[instance_index].model_to_view[surface.part]);
                        XMStoreFloat4x4(&matrix, source_view ? ready : ready * projection);
                    }
                    else
                        XMStoreFloat4x4(&matrix, source_view ? world * view : world * view * projection);
                    std::memcpy(constants.data(), &matrix, sizeof(matrix));
                    const auto n = lighting_normal_basis(p);
                    auto normals = XMMatrixSet(n[0].x, n[0].y, n[0].z, 0, n[1].x, n[1].y, n[1].z, 0, n[2].x, n[2].y,
                                               n[2].z, 0, 0, 0, 0, 1);
                    XMStoreFloat4x4(&matrix, normals);
                    std::memcpy(constants.data() + 16, &matrix, sizeof(matrix));
                    constants[32] = mesh.live ? 2.0f : 0.0f;
                    constants[33] = translucent ? 1.0f : 0.0f;
                    list->SetGraphicsRoot32BitConstants(0, 44, constants.data(), 0);
                    auto *pipeline =
                        translucent ? (surface.two_sided ? transparent_two_sided.Get()
                                                         : (mirrored ? transparent_mirrored.Get() : transparent.Get()))
                                    : (surface.two_sided ? opaque_two_sided.Get()
                                                         : (mirrored ? opaque_mirrored.Get() : opaque.Get()));
                    if (source_view)
                    {
                        // Source camera basis already reverses Y. Together with
                        // the viewport conversion, authored front faces remain
                        // clockwise in screen coordinates (00429B70 dot test).
                        const auto variant = surface.two_sided ? 2u : (mirrored ? 1u : 0u);
                        pipeline = source_pipelines[(translucent ? 3u : 0u) + variant].Get();
                    }
                    if (mesh.live)
                    {
                        const auto variant = surface.two_sided ? 2u : (mirrored ? 1u : 0u);
                        pipeline = live_pipelines[(source_view ? 15u : 0u) +
                                                  3u * static_cast<unsigned>(mesh.live_materials[surface_index].blend) +
                                                  variant]
                                       .Get();
                    }
                    list->SetPipelineState(pipeline);
                    list->SetGraphicsRootDescriptorTable(1, srv_gpu(surface.texture));
                    list->DrawInstanced(surface.count, 1,
                                        surface.first + (mesh.live ? frame.live_offsets[instance.mesh] : 0), 0);
                    ++telemetry.draws;
                }
            }
        float scale = std::clamp(dpi / 96.f, 0.75f, 4.f), width = viewport.Width / scale;
        overlay.clear();
        quad(overlay, 0, 0, viewport.Width, 110 * scale, 1008.f / 1024, 506.f / 512, 0, 0, 0xe5181510);
        quad(overlay, 24 * scale, 26 * scale, 3 * scale, 52 * scale, 1008.f / 1024, 506.f / 512, 0, 0, 0xffb7e757);
        text(overlay, "STUNT GP  /  NEW", 42 * scale, 20 * scale, 25 * scale, 0xfff3f5f6);
        text(overlay, title, 42 * scale, 58 * scale, 16 * scale, 0xffb7e757);
        std::string_view controls =
            options.race_interface
                ? (width > 850
                       ? "WASD / strzalki: jazda   Spacja: turbo   R: powrot na tor   F11: pelny ekran   V: VSync"
                       : "WASD: jazda   Spacja: turbo   R: powrot na tor   F11")
            : width > 850
                ? "Drag: orbit   Wheel: zoom   Arrows: model   T: track   C: car   L: LOD   F11: fullscreen   V: VSync"
                : "Drag / wheel   Arrows: model   T / C   F11   V";
        // New native layout: wrap actual font advances so result/controls remain
        // readable in narrow windows and at high desktop scaling.
        const auto available = std::max(1.f, viewport.Width - 48 * scale);
        const auto detail_lines = text_lines(detail, 15 * scale, available);
        const auto control_lines = text_lines(controls, 13 * scale, available);
        const float footer =
            (35 + 28 * static_cast<float>(detail_lines.size()) + 20 * static_cast<float>(control_lines.size())) * scale;
        quad(overlay, 0, viewport.Height - footer, viewport.Width, footer, 1008.f / 1024, 506.f / 512, 0, 0,
             0xe5181510);
        float line_y = viewport.Height - footer + 15 * scale;
        for (const auto line : detail_lines)
        {
            text(overlay, line, 24 * scale, line_y, 15 * scale, 0xffd5d0c9);
            line_y += 28 * scale;
        }
        for (const auto line : control_lines)
        {
            text(overlay, line, 24 * scale, line_y, 13 * scale, 0xffa69e95);
            line_y += 20 * scale;
        }
        auto bytes = overlay.size() * sizeof(Vertex);
        if (bytes > 2 * 1024 * 1024)
            throw std::runtime_error("overlay capacity exceeded");
        void *mapped{};
        D3D12_RANGE empty{};
        hr(frame.overlay->Map(0, &empty, &mapped), "overlay map");
        std::memcpy(mapped, overlay.data(), bytes);
        frame.overlay->Unmap(0, nullptr);
        D3D12_VERTEX_BUFFER_VIEW ui{frame.overlay->GetGPUVirtualAddress(), static_cast<UINT>(bytes), sizeof(Vertex)};
        list->IASetVertexBuffers(0, 1, &ui);
        list->SetPipelineState(overlay_pipeline.Get());
        constants[32] = 1;
        constants[33] = viewport.Width;
        constants[34] = viewport.Height;
        list->SetGraphicsRoot32BitConstants(0, 44, constants.data(), 0);
        list->SetGraphicsRootDescriptorTable(1, srv_gpu(max_textures - 1));
        list->DrawInstanced(static_cast<UINT>(overlay.size()), 1, 0, 0);
        ++telemetry.draws;
        if (telemetry.samples > 1)
        {
            b = barrier(color.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
            list->ResourceBarrier(1, &b);
            list->ResolveSubresource(frame.back.Get(), 0, color.Get(), 0, target_format);
            b = barrier(color.Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
            list->ResourceBarrier(1, &b);
        }
        record_frame_capture(frame, telemetry.samples > 1 ? D3D12_RESOURCE_STATE_RESOLVE_DEST
                                                          : D3D12_RESOURCE_STATE_RENDER_TARGET);
        list->EndQuery(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 4 + 1);
        list->ResolveQueryData(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 4, 2, timestamp_readback.Get(),
                               slot * 32);
        hr(list->Close(), "frame close");
        ID3D12CommandList *lists[] = {list.Get()};
        queue->ExecuteCommandLists(1, lists);
        frame.fence = next_fence++;
        context.fence = frame.fence;
        context.timed = true;
        hr(queue->Signal(fence.Get(), frame.fence), "frame signal");
        if (!present_to_swap_chain("Present")) // NEW HOST (R24) mode-aware Present
            return;
        last_back = index;
        submitted = true;
        ++telemetry.presents;
    }
    void resize(unsigned w, unsigned h)
    {
        if (polygon_roles_active)
            require_surface_thread();
        if (polygon_submission_unfenced)
            throw FormatError("unfenced polygon work requires renderer teardown");
        if (!w || !h)
            return;
        if (w > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || h > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
            throw FormatError("resize exceeds D3D12 texture dimension limit");
        // NEW HOST (R24): a DXGI fullscreen transition arrives here as WM_SIZE.
        (void)observe_fullscreen_state();
        if (w == telemetry.width && h == telemetry.height)
        {
            if (swap_transition_pending)
            {
                rebuild_swap_buffers_same_extent();
                return;
            }
            if (!targets_ready)
            {
                create_targets();
                submitted = false;
            }
            return;
        }
        flush();
        submitted = false;
        targets_ready = false;
        for (auto &f : frames)
        {
            f.back.Reset();
            f.capture_readback.Reset();
            f.capture_layout = {};
            f.capture_bytes = 0;
            f.gamma = {}; // flush above retired every reference, including failed-Present work.
        }
        color.Reset();
        polygon_color.Reset();
        polygon_rtvs.Reset();
        depth.Reset();
        const auto resized = consume_failure(RenderFailurePoint::resize_buffers)
                                 ? E_OUTOFMEMORY
                                 : swap->ResizeBuffers(frame_count, w, h, target_format, swap_flags);
        if (FAILED(resized))
        {
            // A rejected ResizeBuffers retains the original swap-chain size.
            // Restore its views if possible; otherwise drawing stays blocked.
            try
            {
                create_targets();
            }
            catch (...)
            {
                targets_ready = false;
            }
            hr(resized, "ResizeBuffers");
        }
        swap_transition_pending = false; // NEW HOST (R24): any fullscreen transition is realized
        // ResizeBuffers succeeded: old-size images are no longer admitted.
        // Keep roles on a rejected resize; invalidate even if new target view
        // creation fails afterwards. No resampling or invented replacement.
        polygon_front.reset();
        polygon_back.reset();
        for (auto &context : contexts)
            context.polygon_image.reset();
        image_pool->free.clear(); // old-extent allocations are never admitted again
        telemetry.width = w;
        telemetry.height = h;
        host_wide::set_output_size(w, h); // NEW R20 host layout: canvas follows the new target aspect
        create_targets();
        submitted = false;
    }
    // NEW HOST (R24): the flip model needs ResizeBuffers after every windowed/
    // fullscreen transition even at an unchanged extent (ResizeBuffers and
    // SetFullscreenState remarks). Same release discipline as resize(): all GPU
    // work retired, every back-buffer reference (buffers, RTV heaps, capture
    // copies, gamma caches) released first. Extent-bound owners (image roles,
    // pool, host_wide layout) stay valid because the extent is unchanged.
    void rebuild_swap_buffers_same_extent()
    {
        flush();
        submitted = false;
        targets_ready = false;
        for (auto &f : frames)
        {
            f.back.Reset();
            f.capture_readback.Reset();
            f.capture_layout = {};
            f.capture_bytes = 0;
            f.gamma = {};
        }
        color.Reset();
        polygon_color.Reset();
        polygon_rtvs.Reset();
        depth.Reset();
        const auto resized =
            consume_failure(RenderFailurePoint::resize_buffers)
                ? E_OUTOFMEMORY
                : swap->ResizeBuffers(frame_count, telemetry.width, telemetry.height, target_format, swap_flags);
        if (FAILED(resized))
        {
            try
            {
                create_targets();
            }
            catch (...)
            {
                targets_ready = false;
            }
            hr(resized, "ResizeBuffers (fullscreen transition)");
        }
        swap_transition_pending = false;
        create_targets();
    }
    void collect_errors()
    {
        if (!info)
            return;
        auto count = info->GetNumStoredMessagesAllowedByRetrievalFilter();
        for (UINT64 i = 0; i < count; ++i)
        {
            SIZE_T n{};
            info->GetMessage(i, nullptr, &n);
            std::vector<std::uint8_t> memory(n);
            auto *message = reinterpret_cast<D3D12_MESSAGE *>(memory.data());
            if (SUCCEEDED(info->GetMessage(i, message, &n)) && message->Severity <= D3D12_MESSAGE_SEVERITY_WARNING)
            {
                ++telemetry.debug_errors;
                diagnostics.push_back({static_cast<unsigned>(message->ID), static_cast<unsigned>(message->Severity),
                                       message->pDescription});
                OutputDebugStringA(message->pDescription);
            }
        }
        info->ClearStoredMessages();
    }
    void capture(const std::filesystem::path &path)
    {
        require_targets();
        if (!options.capture_frames || !submitted || !frames[last_back].capture_readback)
            throw std::runtime_error("capture needs enabled recording and a rendered frame");
        const auto &frame = frames[last_back];
        wait(frame.fence);
        const auto &readback = frame.capture_readback;
        const auto &layout = frame.capture_layout;
        const auto total = frame.capture_bytes;
        void *mapped{};
        D3D12_RANGE range{0, static_cast<SIZE_T>(total)};
        hr(readback->Map(0, &range, &mapped), "capture readback");
        std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary);
        BITMAPFILEHEADER file{};
        file.bfType = 0x4d42;
        file.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        file.bfSize = file.bfOffBits + telemetry.width * telemetry.height * 4;
        BITMAPINFOHEADER bitmap{};
        bitmap.biSize = sizeof(bitmap);
        bitmap.biWidth = static_cast<LONG>(telemetry.width);
        bitmap.biHeight = -static_cast<LONG>(telemetry.height);
        bitmap.biPlanes = 1;
        bitmap.biBitCount = 32;
        bitmap.biCompression = BI_RGB;
        out.write(reinterpret_cast<const char *>(&file), sizeof(file));
        out.write(reinterpret_cast<const char *>(&bitmap), sizeof(bitmap));
        Bytes row(std::size_t(telemetry.width) * 4);
        for (unsigned y = 0; y < telemetry.height; ++y)
        {
            auto *src =
                static_cast<const std::uint8_t *>(mapped) + layout.Offset + std::size_t(y) * layout.Footprint.RowPitch;
            for (unsigned x = 0; x < telemetry.width; ++x)
            {
                row[x * 4] = src[x * 4 + 2];
                row[x * 4 + 1] = src[x * 4 + 1];
                row[x * 4 + 2] = src[x * 4];
                row[x * 4 + 3] = 255;
            }
            out.write(reinterpret_cast<const char *>(row.data()), static_cast<std::streamsize>(row.size()));
        }
        D3D12_RANGE no_write{};
        readback->Unmap(0, &no_write);
        if (!out)
            throw std::runtime_error("failed to save capture");
    }
};
std::vector<RendererAdapterInfo> Renderer::enumerate_d3d12_adapters(bool software_only)
{
    ComPtr<IDXGIFactory6> factory;
    hr(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "enumerate D3D12 adapters factory");
    std::vector<RendererAdapterInfo> result;
    for (UINT index = 0;; ++index)
    {
        ComPtr<IDXGIAdapter1> adapter;
        HRESULT status = software_only
                             ? (index == 0 ? factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)) : DXGI_ERROR_NOT_FOUND)
                             : factory->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                                   IID_PPV_ARGS(&adapter));
        if (status == DXGI_ERROR_NOT_FOUND)
            break;
        hr(status, "enumerate D3D12 adapter");
        DXGI_ADAPTER_DESC1 desc{};
        hr(adapter->GetDesc1(&desc), "enumerated DXGI description");
        if (bool(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != software_only)
            continue;
        // A capability probe, with no device output and no second window.
        if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
            continue;
        result.push_back({std::uint64_t(desc.AdapterLuid.LowPart) |
                              (std::uint64_t(static_cast<std::uint32_t>(desc.AdapterLuid.HighPart)) << 32u),
                          desc.DedicatedVideoMemory, desc.SharedSystemMemory, desc.VendorId, desc.DeviceId,
                          bool(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE), narrow(desc.Description)});
    }
    return result;
}
RendererDeviceInfo Renderer::device_info() const
{
    impl_->require_surface_thread();
    RendererDeviceInfo out;
    out.adapter = impl_->adapter_info;
    auto format = [&](DXGI_FORMAT value) {
        D3D12_FEATURE_DATA_FORMAT_SUPPORT info{value};
        hr(impl_->device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &info, sizeof(info)),
           "selected device format support");
        return static_cast<std::uint32_t>(info.Support1);
    };
    out.rgba8_support = format(DXGI_FORMAT_R8G8B8A8_UNORM);
    out.a1rgb5_support = format(DXGI_FORMAT_B5G5R5A1_UNORM);
    out.depth32_support = format(DXGI_FORMAT_D32_FLOAT);
    D3D_FEATURE_LEVEL requested[]{D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1,
                                  D3D_FEATURE_LEVEL_11_0};
    D3D12_FEATURE_DATA_FEATURE_LEVELS levels{4, requested, D3D_FEATURE_LEVEL_11_0};
    hr(impl_->device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &levels, sizeof(levels)),
       "selected device feature level");
    out.feature_level = static_cast<std::uint32_t>(levels.MaxSupportedFeatureLevel);
    out.color_target_present =
        impl_->targets_ready && bool(impl_->frames[0].back) && (impl_->telemetry.samples == 1 || bool(impl_->color));
    out.depth_target_present = impl_->targets_ready && bool(impl_->depth);
    return out;
}
Renderer::Renderer(HWND w, RenderOptions options) : impl_(std::make_unique<Impl>(w, options)) {}
HWND Renderer::native_window() const noexcept
{
    return impl_->window;
}
RenderOptions Renderer::creation_options() const
{
    impl_->require_surface_thread();
    return impl_->options;
}
RendererHostState Renderer::host_state() const
{
    impl_->require_surface_thread();
    return {impl_->device->GetDeviceRemovedReason(), impl_->targets_ready, impl_->polygon_submission_unfenced};
}
void Renderer::require_polygon_texture_owner(const PolygonGpuTexture &texture) const
{
    impl_->require_surface_thread();
    if (!texture.resource)
        throw FormatError("polygon recovery texture has no native resource");
    ComPtr<IUnknown> actual, expected;
    hr(texture.resource->GetDevice(IID_PPV_ARGS(&actual)), "polygon recovery resource device");
    hr(impl_->device.As(&expected), "polygon recovery renderer device identity");
    if (actual.Get() != expected.Get())
        throw FormatError("polygon recovery resource belongs to another native device");
}
std::uint64_t Renderer::stage_polygon_back_fitted(const CapturedPolygonSubmission &submission,
                                                  const SdrGammaRamp &gamma)
{
    SGN_PROFILE_SCOPE("gpu.stage_polygon_back_fitted");
    const auto canvas = impl_->retained_polygon_canvas(submission);
    auto image = impl_->produce_polygon_image(submission, gamma, false, canvas, true);
    const auto identity = image->fence;
    impl_->polygon_back = std::move(image);
    return identity;
}
std::uint64_t Renderer::restage_polygon_front(const CapturedPolygonSubmission &submission, const SdrGammaRamp &gamma,
                                              bool fit_canvas)
{
    SGN_PROFILE_SCOPE("gpu.restage_polygon_front");
    std::optional<PolygonLogicalRasterExtent> canvas;
    if (fit_canvas)
        canvas = impl_->retained_polygon_canvas(submission); // validates
    auto image = impl_->produce_polygon_image(submission, gamma, false, canvas, fit_canvas);
    const auto identity = image->fence;
    impl_->polygon_front = std::move(image);
    return identity;
}
RendererPolygonTargetFormat Renderer::polygon_target_format() const
{
    impl_->require_surface_thread();
    return {target_format, DXGI_FORMAT_D32_FLOAT, DXGI_SAMPLE_DESC{impl_->telemetry.samples, 0}};
}
ID3D12Device &Renderer::native_device() const
{
    return *impl_->device.Get();
}
std::shared_ptr<const GpuStaticBuffer> Renderer::create_static_buffer(std::span<const std::byte> bytes,
                                                                      D3D12_RESOURCE_STATES final_state)
{
    SGN_PROFILE_SCOPE("gpu.create_static_buffer");
    return impl_->create_static_buffer(bytes, final_state);
}
void Renderer::wait_transient_present_buffer()
{
    SGN_PROFILE_SCOPE("gpu.wait_transient_present_buffer");
    impl_->wait_transient_present_buffer();
}
void Renderer::present_polygon_submission_transient(const CapturedPolygonSubmission &submission,
                                                    const SdrGammaRamp &ramp, bool fit_canvas)
{
    SGN_PROFILE_SCOPE("gpu.present_polygon_submission_transient");
    impl_->present_polygon_submission_transient(submission, ramp, fit_canvas);
}
void Renderer::present_polygon_reprojected(const std::shared_ptr<const CapturedPolygonSubmission> &submission,
                                           const PolygonReprojection &reprojection, const SdrGammaRamp &ramp,
                                           bool fit_canvas)
{
    SGN_PROFILE_SCOPE("gpu.present_polygon_reprojected");
    impl_->present_polygon_reprojected(submission, reprojection, ramp, fit_canvas);
}
void Renderer::present_polygon_submission_transient(const std::shared_ptr<const CapturedPolygonSubmission> &submission,
                                                    const SdrGammaRamp &ramp, bool fit_canvas)
{
    SGN_PROFILE_SCOPE("gpu.present_polygon_submission_transient");
    impl_->present_polygon_submission_transient(submission, ramp, fit_canvas);
}
std::uint64_t Renderer::completed_gpu_fence() const
{
    const auto value = impl_->fence->GetCompletedValue();
    if (value == UINT64_MAX)
    {
        impl_->check_device();
        throw GraphicsError(DXGI_ERROR_DEVICE_REMOVED, "GPU fence signaled removal");
    }
    return value;
}
std::uint64_t Renderer::last_submitted_gpu_fence() const noexcept
{
    return impl_->next_fence - 1;
}
void Renderer::retire_polygon_texture(Microsoft::WRL::ComPtr<ID3D12Resource> resource) noexcept
{
    impl_->retire_polygon_texture(std::move(resource));
}
void Renderer::note_polygon_replacement_profile(const PolygonTextureProfile &profile) noexcept
{
    try
    {
        if (impl_->polygon_backend)
            impl_->polygon_backend->note_replacement_profile(profile);
    }
    catch (...)
    {
        // a note only steers a later prewarm
    }
}
void Renderer::set_polygon_precise_vertex_mapping(bool enabled)
{
    impl_->require_surface_thread();
    impl_->polygons().set_precise_vertex_mapping(enabled);
}
bool GraphicsError::device_lost() const noexcept
{
    return result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET ||
           result == DXGI_ERROR_DEVICE_HUNG || result == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}
Renderer::~Renderer() = default;
void Renderer::resize(unsigned w, unsigned h)
{
    impl_->resize(w, h);
}
void Renderer::load(const Model &model, const AssetStore &assets)
{
    impl_->load_scene(std::span(&model, 1), assets);
}
void Renderer::load_scene(std::span<const Model> models, const AssetStore &assets)
{
    impl_->load_scene(models, assets);
}
void Renderer::stage_mesh(std::size_t slot, const LiveGeometry &geometry)
{
    impl_->stage_mesh(slot, geometry);
}
void Renderer::draw_polygons(const CapturedPolygonFrame &frame, const PolygonDeviceState &initial,
                             std::shared_ptr<const PolygonGpuTextures> textures)
{
    impl_->draw_polygons(frame, initial, std::move(textures));
}
void Renderer::draw_polygon_submission(const CapturedPolygonSubmission &submission)
{
    SGN_PROFILE_SCOPE("gpu.draw_polygon_submission");
    impl_->draw_polygon_submission(submission);
}
void Renderer::draw_polygon_submission(const CapturedPolygonSubmission &submission, const SdrGammaRamp &ramp)
{
    SGN_PROFILE_SCOPE("gpu.draw_polygon_submission_gamma");
    impl_->draw_polygon_submission(submission, &ramp);
}
std::uint64_t Renderer::stage_polygon_back(const CapturedPolygonSubmission &submission, const SdrGammaRamp &ramp)
{
    SGN_PROFILE_SCOPE("gpu.stage_polygon_back");
    return impl_->stage_polygon_back(submission, ramp);
}
PolygonSurfaceRoles Renderer::restore_polygon_surfaces(const CapturedPolygonSubmission *front,
                                                       const SdrGammaRamp &front_ramp,
                                                       const CapturedPolygonSubmission *back,
                                                       const SdrGammaRamp &back_ramp)
{
    return impl_->restore_polygon_surfaces(front, front_ramp, back, back_ramp);
}
PolygonSurfaceFlip Renderer::flip_polygon_surfaces()
{
    SGN_PROFILE_SCOPE("gpu.flip_polygon_surfaces");
    return impl_->flip_polygon_surfaces();
}
bool Renderer::present_polygon_front()
{
    SGN_PROFILE_SCOPE("gpu.present_polygon_front");
    return impl_->present_polygon_front();
}
PolygonSurfaceRoles Renderer::polygon_surface_roles() const
{
    return impl_->polygon_surface_roles();
}
std::vector<PolygonGpuTexture> Renderer::generate_polygon_textures(std::uint32_t primary, std::uint32_t alternate,
                                                                   std::uint32_t dimension)
{
    return impl_->generate_polygon_textures(primary, alternate, dimension);
}
PolygonGpuTexture Renderer::upload_polygon_texture(std::uint32_t token, std::uint32_t width, std::uint32_t height,
                                                   std::span<const std::uint16_t> pixels)
{
    SGN_PROFILE_SCOPE("gpu.upload_polygon_texture");
    const auto profile = impl_->polygon_texture_profile(); // NEW HOST (R28b): a new identity
    return impl_->upload_polygon_texture(token, width, height, pixels, profile);
}
PolygonTextureProfile Renderer::polygon_texture_profile()
{
    return impl_->polygon_texture_profile();
}
PolygonGpuTexture Renderer::upload_polygon_texture(std::uint32_t token, std::uint32_t width, std::uint32_t height,
                                                   std::span<const std::uint16_t> pixels,
                                                   const PolygonTextureProfile &profile)
{
    SGN_PROFILE_SCOPE("gpu.upload_polygon_texture");
    return impl_->upload_polygon_texture(token, width, height, pixels, profile);
}
unsigned Renderer::append_texture(const Image &image)
{
    return impl_->append_texture(image);
}
void Renderer::replace_texture(unsigned slot, const Image &image)
{
    impl_->replace_texture(slot, image);
}
unsigned Renderer::texture_index(unsigned mesh, unsigned local_texture) const
{
    if (mesh >= impl_->meshes.size() || local_texture >= impl_->meshes[mesh].texture_remap.size())
        throw std::out_of_range("mesh texture binding outside scene");
    return impl_->meshes[mesh].texture_remap[local_texture];
}
std::array<unsigned, 2> Renderer::texture_dimensions(unsigned slot) const
{
    if (slot >= impl_->textures.size())
        throw std::out_of_range("texture dimensions binding outside scene");
    const auto d = impl_->textures[slot]->GetDesc();
    return {static_cast<unsigned>(d.Width), d.Height};
}
Image Renderer::read_texture(unsigned slot)
{
    return impl_->read_texture(slot);
}
void Renderer::draw(const View &v, const std::string &title, const std::string &detail, float dpi)
{
    const SceneInstance identity{};
    impl_->draw_scene(v, std::span(&identity, 1), title, detail, dpi);
}
void Renderer::draw_scene(const View &v, std::span<const SceneInstance> instances, std::string_view title,
                          std::string_view detail, float dpi, std::span<const LiveInstanceView> live_views)
{
    impl_->draw_scene(v, instances, title, detail, dpi, live_views);
}
bool Renderer::exclusive_fullscreen_supported() const noexcept
{
    return impl_->mode_switch_enabled;
}
bool Renderer::enter_exclusive_fullscreen()
{
    SGN_PROFILE_SCOPE("gpu.enter_exclusive_fullscreen");
    return impl_->enter_exclusive_fullscreen();
}
bool Renderer::leave_exclusive_fullscreen()
{
    SGN_PROFILE_SCOPE("gpu.leave_exclusive_fullscreen");
    return impl_->leave_exclusive_fullscreen();
}
bool Renderer::exclusive_fullscreen()
{
    return impl_->exclusive_fullscreen();
}
void Renderer::vsync(bool enabled)
{
    impl_->options.vsync = enabled;
}
HANDLE Renderer::frame_event() const
{
    return impl_->latency_event.value;
}
bool Renderer::transient_present_ready() noexcept
{
    return impl_->transient_present_ready();
}
Renderer::DisplayStatistics Renderer::display_statistics() noexcept
{
    // NEW HOST (R35): read-only queries of the swap chain (no present, no state change).
    DisplayStatistics result;
    auto *swap = impl_->swap.Get();
    if (!swap)
        return result;
    UINT count = 0;
    result.present_ok = SUCCEEDED(swap->GetLastPresentCount(&count));
    result.last_present = count;
    DXGI_FRAME_STATISTICS stats{};
    if (!result.present_ok || FAILED(swap->GetFrameStatistics(&stats)) || !stats.SyncQPCTime.QuadPart)
        return result;
    result.ok = true;
    result.present_count = stats.PresentCount;
    result.present_refresh = stats.PresentRefreshCount;
    result.sync_refresh = stats.SyncRefreshCount;
    result.sync_qpc = stats.SyncQPCTime.QuadPart;
    return result;
}
bool Renderer::occluded() const
{
    if (!impl_->occluded)
        return false;
    auto result = impl_->swap->Present(0, DXGI_PRESENT_TEST);
    hr(result, "Present visibility test");
    return result == DXGI_STATUS_OCCLUDED;
}
const RenderStats &Renderer::stats() const
{
    return impl_->telemetry;
}
void Renderer::capture(const std::filesystem::path &path)
{
    impl_->capture(path);
}
void Renderer::finish()
{
    impl_->flush();
    impl_->collect_errors();
}
void Renderer::collect_diagnostics()
{
    impl_->collect_errors();
}
void Renderer::check_health() const
{
    impl_->check_device();
}
const std::vector<GpuDiagnostic> &Renderer::diagnostics() const
{
    return impl_->diagnostics;
}
void Renderer::inject_debug_error_for_test()
{
    if (!impl_->info)
        throw std::logic_error("debug injection requires debug layer");
    hr(impl_->info->AddApplicationMessage(D3D12_MESSAGE_SEVERITY_ERROR,
                                          "StuntGP NEW explicit diagnostic fault injection"),
       "debug fault injection");
}
void Renderer::inject_failure_for_test(RenderFailurePoint point)
{
    if (!impl_->options.debug)
        throw std::logic_error("failure injection requires debug layer");
    if (impl_->injected_failure)
        throw std::logic_error("previous renderer failure injection is pending");
    impl_->injected_failure = point;
}
void Renderer::before_polygon_submit_for_test(std::function<void()> callback)
{
    if (!impl_->options.debug)
        throw std::logic_error("submission hook requires debug layer");
    if (impl_->before_polygon_submit)
        throw std::logic_error("previous renderer submission hook is pending");
    impl_->before_polygon_submit = std::move(callback);
}
void Renderer::remove_device_for_test(bool defer_failure)
{
    impl_->flush();
    impl_->collect_errors();
    ComPtr<ID3D12Device5> device;
    hr(impl_->device.As(&device), "device removal diagnostic interface");
    device->RemoveDevice();
    if (SUCCEEDED(impl_->device->GetDeviceRemovedReason()))
        throw std::runtime_error("device removal injection did not remove the device");
    if (defer_failure)
        return;
    impl_->check_device();
    throw std::runtime_error("device removal injection did not remove the device");
}
} // namespace sgn
