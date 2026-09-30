#include "sgn/host_post_process_d3d12.hpp"
#include "sgn/host_shader_cache.hpp"
#include "sgn/polygon_reprojection.hpp"
#include "sgn/host_profiler.hpp"
#include "sgn/renderer.hpp"
#include <d3dcompiler.h>
#include <algorithm>
#include <chrono>
#include <future>
#include <limits>
#include <string>
#include <utility>
#include <variant>

// NEW HOST (R28) HUD split and selective bloom; see the header. Host
// presentation only: no source owner, captured command or source-visible
// value is changed. The emissive streams are private copies.
namespace sgn
{
using Microsoft::WRL::ComPtr;
namespace
{
using host_frame_phase::Marker;
void checked(HRESULT status, const char *operation)
{
    if (FAILED(status))
        throw GraphicsError(status, operation);
}
D3D12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE type)
{
    D3D12_HEAP_PROPERTIES value{};
    value.Type = type;
    value.CreationNodeMask = value.VisibleNodeMask = 1;
    return value;
}
D3D12_RESOURCE_BARRIER transition(ID3D12Resource &resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition = {&resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after};
    return b;
}
// Full-screen passes. rcp_target: 1/size of the render target; rcp_image and
// rcp_extra: 1/size of t0 and t1. All reads use one bilinear clamp sampler.
//   reduce4:   exact 4x4 box (four bilinear taps, each the mean of 2x2 texels),
//              full-resolution emissive -> quarter resolution.
//   down/up:   dual filter (Kawase / Bjorge 2015). up adds the same-level
//              reduction (t1), so the quarter level holds the 1/4 + 1/8 + 1/16 sum.
//   composite: FXAA on: scene copy (t0) -> FXAA 3.11 [+ strength * tent(bloom quarter, t1)],
//              written over the polygon target. Bloom only: strength * tent(t1)
//              added by the blend unit (ONE, ONE), so no scene copy is needed.
// The FXAA function is the R23 algorithm of sdr_gamma_d3d12.cpp (same
// constants, search steps and taps) without the ramp: the ramp is applied
// once, at the end of the frame, by the unchanged plain gamma pass.
constexpr char shader[] = R"(
Texture2D<float4> image : register(t0);
Texture2D<float4> extra : register(t1);
SamplerState bilinear : register(s0);
cbuffer Constants : register(b0) { float2 rcp_target; float2 rcp_image; float2 rcp_extra; float strength; float unused; };
float4 vs(uint id : SV_VertexID) : SV_Position {
    float2 p=float2((id<<1)&2,id&2);
    return float4(p*float2(2,-2)+float2(-1,1),0,1);
}
float3 at(Texture2D<float4> t,float2 uv) { return t.SampleLevel(bilinear,uv,0).rgb; }
float3 tent(Texture2D<float4> t,float2 uv,float2 h) {
    float3 sum=at(t,uv+float2(-2.0*h.x,0))+at(t,uv+float2(2.0*h.x,0))+at(t,uv+float2(0,-2.0*h.y))+at(t,uv+float2(0,2.0*h.y));
    sum+=(at(t,uv+float2(-h.x,-h.y))+at(t,uv+float2(h.x,-h.y))+at(t,uv+float2(-h.x,h.y))+at(t,uv+float2(h.x,h.y)))*2.0;
    return sum*(1.0/12.0);
}
float4 reduce4(float4 position : SV_Position) : SV_Target {
    float2 corner=floor(position.xy)*4.0;
    float3 sum=at(image,(corner+float2(1,1))*rcp_image)+at(image,(corner+float2(3,1))*rcp_image)
              +at(image,(corner+float2(1,3))*rcp_image)+at(image,(corner+float2(3,3))*rcp_image);
    return float4(sum*0.25,1);
}
float4 down(float4 position : SV_Position) : SV_Target {
    float2 uv=position.xy*rcp_target;
    float2 o=rcp_image;
    float3 sum=at(image,uv)*4.0+at(image,uv-o)+at(image,uv+o)+at(image,uv+float2(o.x,-o.y))+at(image,uv+float2(-o.x,o.y));
    return float4(sum*0.125,1);
}
float4 up(float4 position : SV_Position) : SV_Target {
    float2 uv=position.xy*rcp_target;
    return float4(tent(image,uv,rcp_image*0.5)+extra.Load(int3(int2(position.xy),0)).rgb,1);
}
#if FXAA
float luma(float3 rgb) { return dot(rgb,float3(0.299,0.587,0.114)); }
float luma_at(float2 p) { return luma(image.SampleLevel(bilinear,p,0).rgb); }
static const float steps[8]={1.0,1.5,2.0,2.0,2.0,2.0,4.0,8.0};
float3 fxaa(float2 posM,float3 rgbM) {
    float lumaM=luma(rgbM);
    float lumaS=luma(image.SampleLevel(bilinear,posM,0,int2( 0, 1)).rgb);
    float lumaE=luma(image.SampleLevel(bilinear,posM,0,int2( 1, 0)).rgb);
    float lumaN=luma(image.SampleLevel(bilinear,posM,0,int2( 0,-1)).rgb);
    float lumaW=luma(image.SampleLevel(bilinear,posM,0,int2(-1, 0)).rgb);
    float rangeMax=max(max(lumaN,lumaW),max(lumaE,max(lumaS,lumaM)));
    float rangeMin=min(min(lumaN,lumaW),min(lumaE,min(lumaS,lumaM)));
    float range=rangeMax-rangeMin;
    if(range<max(0.0833,rangeMax*0.166)) return rgbM;
    float lumaNW=luma(image.SampleLevel(bilinear,posM,0,int2(-1,-1)).rgb);
    float lumaSE=luma(image.SampleLevel(bilinear,posM,0,int2( 1, 1)).rgb);
    float lumaNE=luma(image.SampleLevel(bilinear,posM,0,int2( 1,-1)).rgb);
    float lumaSW=luma(image.SampleLevel(bilinear,posM,0,int2(-1, 1)).rgb);
    float lumaNS=lumaN+lumaS;
    float lumaWE=lumaW+lumaE;
    float lumaNESE=lumaNE+lumaSE;
    float lumaNWNE=lumaNW+lumaNE;
    float lumaNWSW=lumaNW+lumaSW;
    float lumaSWSE=lumaSW+lumaSE;
    float edgeHorz=abs(-2.0*lumaW+lumaNWSW)+abs(-2.0*lumaM+lumaNS)*2.0+abs(-2.0*lumaE+lumaNESE);
    float edgeVert=abs(-2.0*lumaS+lumaSWSE)+abs(-2.0*lumaM+lumaWE)*2.0+abs(-2.0*lumaN+lumaNWNE);
    bool horzSpan=edgeHorz>=edgeVert;
    float subpixA=(lumaNS+lumaWE)*2.0+lumaNWSW+lumaNESE;
    if(!horzSpan) { lumaN=lumaW; lumaS=lumaE; }
    float lengthSign=horzSpan?rcp_target.y:rcp_target.x;
    float subpixB=subpixA*(1.0/12.0)-lumaM;
    float gradientN=lumaN-lumaM;
    float gradientS=lumaS-lumaM;
    float lumaNN=lumaN+lumaM;
    float lumaSS=lumaS+lumaM;
    bool pairN=abs(gradientN)>=abs(gradientS);
    float gradient=max(abs(gradientN),abs(gradientS));
    if(pairN) lengthSign=-lengthSign;
    float subpixC=saturate(abs(subpixB)/range);
    float2 posB=posM;
    float2 offNP=horzSpan?float2(rcp_target.x,0.0):float2(0.0,rcp_target.y);
    if(horzSpan) posB.y+=lengthSign*0.5; else posB.x+=lengthSign*0.5;
    if(!pairN) lumaNN=lumaSS;
    float gradientScaled=gradient*0.25;
    float lumaMM=lumaM-lumaNN*0.5;
    bool lumaMLTZero=lumaMM<0.0;
    float2 posN=posB-offNP*steps[0];
    float2 posP=posB+offNP*steps[0];
    float lumaEndN=luma_at(posN)-lumaNN*0.5;
    float lumaEndP=luma_at(posP)-lumaNN*0.5;
    bool doneN=abs(lumaEndN)>=gradientScaled;
    bool doneP=abs(lumaEndP)>=gradientScaled;
    if(!doneN) posN-=offNP*steps[1];
    if(!doneP) posP+=offNP*steps[1];
    [unroll] for(uint i=2;i<8;++i) {
        if(!doneN) { lumaEndN=luma_at(posN)-lumaNN*0.5; doneN=abs(lumaEndN)>=gradientScaled; }
        if(!doneP) { lumaEndP=luma_at(posP)-lumaNN*0.5; doneP=abs(lumaEndP)>=gradientScaled; }
        if(!doneN) posN-=offNP*steps[i];
        if(!doneP) posP+=offNP*steps[i];
    }
    float dstN=horzSpan?posM.x-posN.x:posM.y-posN.y;
    float dstP=horzSpan?posP.x-posM.x:posP.y-posM.y;
    bool goodSpanN=(lumaEndN<0.0)!=lumaMLTZero;
    bool goodSpanP=(lumaEndP<0.0)!=lumaMLTZero;
    float spanLength=dstP+dstN;
    bool directionN=dstN<dstP;
    float dst=min(dstN,dstP);
    bool goodSpan=directionN?goodSpanN:goodSpanP;
    float subpixF=(-2.0*subpixC+3.0)*subpixC*subpixC;
    float pixelOffset=0.5-dst/spanLength;
    float pixelOffsetSubpix=max(goodSpan?pixelOffset:0.0,subpixF*subpixF*0.75);
    float2 pos=posM;
    if(horzSpan) pos.y+=pixelOffsetSubpix*lengthSign; else pos.x+=pixelOffsetSubpix*lengthSign;
    return image.SampleLevel(bilinear,pos,0).rgb;
}
#endif
float4 composite(float4 position : SV_Position) : SV_Target {
#if FXAA
    float4 pixel=image.Load(int3(int2(position.xy),0));
    float3 rgb=fxaa(position.xy*rcp_target,pixel.rgb);
#if BLOOM
    rgb+=strength*tent(extra,position.xy*rcp_target,rcp_extra*0.5);
#endif
    return float4(rgb,pixel.a);
#else
    return float4(strength*tent(extra,position.xy*rcp_target,rcp_extra*0.5),0);
#endif
}
)";
ComPtr<ID3DBlob> compile(const char *entry, const char *target, bool fxaa = false, bool bloom = false)
{
    const D3D_SHADER_MACRO macros[]{{"FXAA", fxaa ? "1" : "0"}, {"BLOOM", bloom ? "1" : "0"}, {nullptr, nullptr}};
    ComPtr<ID3DBlob> bytecode, errors;
    const auto status = host_shader_cache::compile(
        shader, sizeof(shader), "sgn-host-post-process", macros, entry, target,
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &bytecode,
        &errors);
    if (FAILED(status))
        throw GraphicsError(
            status, errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize())
                           : "host post-process shader compile");
    return bytecode;
}
constexpr DXGI_FORMAT level_format = DXGI_FORMAT_R11G11B10_FLOAT;
// Descriptor table pairs (t0,t1) of the shader-visible heap.
enum Table : unsigned
{
    table_composite = 0,
    table_reduce = 1,
    table_down_q = 2, // quarter -> eighth
    table_down_e = 3, // eighth -> sixteenth
    table_up_e = 4,   // sixteenth (+ eighth) -> eighth
    table_up_q = 5,   // eighth (+ quarter) -> quarter
    table_count = 6
};
// Render target views.
enum Target : unsigned
{
    target_emissive = 0,
    target_down_q = 1,
    target_down_e = 2,
    target_down_s = 3,
    target_up_e = 4,
    target_up_q = 5,
    target_scene = 6, // linear-light resolve output
    target_count = 7
};
constexpr unsigned timestamp_slots = 16; // >= the renderer's command contexts
struct PassConstants
{
    float rcp_target[2], rcp_image[2], rcp_extra[2], strength, unused;
};
static_assert(sizeof(PassConstants) == 8 * sizeof(float));
bool additive(const PolygonDeviceState &state) noexcept
{
    // D3D7 ALPHABLENDENABLE (27), SRCBLEND (19), DESTBLEND (20): ONE/ONE or SRCALPHA/ONE.
    const auto &r = state.render;
    return r[27] == 1u && r[20] == 2u && (r[19] == 2u || r[19] == 5u);
}
unsigned reduced(unsigned extent, unsigned factor) noexcept
{
    return (std::max)(1u, (extent + factor - 1) / factor);
}
// One size-dependent texture (the former Impl::texture body; device calls are free-threaded).
ComPtr<ID3D12Resource> make_texture(ID3D12Device &device, unsigned w, unsigned h, DXGI_FORMAT format,
                                    unsigned sample_count, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state,
                                    const D3D12_CLEAR_VALUE *clear, const char *what)
{
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    description.Width = w;
    description.Height = h;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.Format = format;
    description.SampleDesc = {sample_count, 0};
    description.Flags = flags;
    const auto properties = heap(D3D12_HEAP_TYPE_DEFAULT);
    ComPtr<ID3D12Resource> result;
    checked(device.CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &description, state, clear,
                                           IID_PPV_ARGS(&result)),
            what);
    return result;
}
// NEW HOST (R35) the size-dependent targets of one configuration, created ahead on a background thread
// (Renderer::start_prewarm -> prewarm_targets) with exactly the descriptions, states and clear values
// Impl::ensure uses, so the first FXAA / bloom frame (or the first after an MSAA or size change) does not
// create them on the render thread. Fresh resources either way: identical frames.
struct PrebuiltTargets
{
    unsigned width{}, height{}, samples{};
    DXGI_FORMAT color_format{DXGI_FORMAT_UNKNOWN}, resolve_format{DXGI_FORMAT_UNKNOWN};
    bool bloom{};
    ComPtr<ID3D12Resource> scene, emissive, emissive_resolved;
    std::array<ComPtr<ID3D12Resource>, 5> levels;
    bool same(unsigned w, unsigned h, unsigned s, DXGI_FORMAT color, DXGI_FORMAT resolve) const noexcept
    {
        return width == w && height == h && samples == s && color_format == color && resolve_format == resolve;
    }
};
PrebuiltTargets build_targets(ComPtr<ID3D12Device> device, unsigned width, unsigned height, unsigned samples,
                              DXGI_FORMAT color_format, DXGI_FORMAT resolve_format, bool bloom)
{
    PrebuiltTargets t{width, height, samples, color_format, resolve_format, bloom, {}, {}, {}, {}};
    const auto scene_format = resolve_format == color_format ? color_format : DXGI_FORMAT_R8G8B8A8_TYPELESS;
    t.scene = make_texture(*device.Get(), width, height, scene_format, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, "host post-process scene copy");
    if (!bloom)
        return t;
    D3D12_CLEAR_VALUE black{};
    black.Format = color_format;
    t.emissive =
        make_texture(*device.Get(), width, height, color_format, samples, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                     D3D12_RESOURCE_STATE_RENDER_TARGET, &black, "host bloom emissive target");
    if (samples > 1)
        t.emissive_resolved =
            make_texture(*device.Get(), width, height, color_format, 1, D3D12_RESOURCE_FLAG_NONE,
                         D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, "host bloom emissive resolve");
    const unsigned q_w = reduced(width, 4), q_h = reduced(height, 4);
    const std::array<std::array<unsigned, 2>, 5> extents{{{q_w, q_h},
                                                          {reduced(q_w, 2), reduced(q_h, 2)},
                                                          {reduced(q_w, 4), reduced(q_h, 4)},
                                                          {reduced(q_w, 2), reduced(q_h, 2)},
                                                          {q_w, q_h}}};
    for (std::size_t i = 0; i < t.levels.size(); ++i)
        t.levels[i] = make_texture(*device.Get(), extents[i][0], extents[i][1], level_format, 1,
                                   D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                   nullptr, "host bloom level");
    return t;
}
// add: blend ONE/ONE onto the target (bloom-only composite); otherwise the output replaces it.
// Device + D3DCompile only (both free-threaded): runs in the background build.
ComPtr<ID3D12PipelineState> build_pipeline(ID3D12Device &device, ID3D12RootSignature &root, const char *entry,
                                           DXGI_FORMAT format, unsigned sample_count, bool fxaa, bool bloom,
                                           bool add = false)
{
    const auto vs = compile("vs", "vs_5_0");
    const auto ps = compile(entry, "ps_5_0", fxaa, bloom);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};
    state.pRootSignature = &root;
    state.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    state.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    state.SampleMask = (std::numeric_limits<UINT>::max)();
    state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    state.NumRenderTargets = 1;
    state.RTVFormats[0] = format;
    state.SampleDesc = {sample_count, 0};
    state.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    state.RasterizerState.DepthClipEnable = TRUE;
    state.DepthStencilState.DepthEnable = FALSE;
    state.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    state.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    auto &blend = state.BlendState.RenderTarget[0];
    blend.BlendEnable = add ? TRUE : FALSE;
    blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlend = blend.DestBlendAlpha = add ? D3D12_BLEND_ONE : D3D12_BLEND_ZERO;
    blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.LogicOp = D3D12_LOGIC_OP_NOOP;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    ComPtr<ID3D12PipelineState> result;
    checked(device.CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&result)), "host post-process pipeline");
    return result;
}
// Every pipeline of one polygon target configuration, including the emissive
// recorder (its constructor compiles the polygon shaders).
struct PipelineSet
{
    unsigned samples{};
    DXGI_FORMAT format{DXGI_FORMAT_UNKNOWN};
    std::array<ComPtr<ID3D12PipelineState>, 4> composite; // [fxaa | bloom << 1]; [0] unused
    ComPtr<ID3D12PipelineState> reduce, down, up;
    std::unique_ptr<PolygonD3D12> emissive; // a second recorder with its own call scratch
};
PipelineSet build_pipeline_set(ComPtr<ID3D12Device> device, ComPtr<ID3D12RootSignature> root, unsigned samples,
                               DXGI_FORMAT format)
{
    PipelineSet set;
    set.samples = samples;
    set.format = format;
    auto &d = *device.Get();
    auto &r = *root.Get();
    set.composite[1] = build_pipeline(d, r, "composite", format, samples, true, false);
    set.composite[2] = build_pipeline(d, r, "composite", format, samples, false, true, true);
    set.composite[3] = build_pipeline(d, r, "composite", format, samples, true, true);
    set.reduce = build_pipeline(d, r, "reduce4", level_format, 1, false, false);
    set.down = build_pipeline(d, r, "down", level_format, 1, false, false);
    set.up = build_pipeline(d, r, "up", level_format, 1, false, false);
    set.emissive = std::make_unique<PolygonD3D12>(d, format, DXGI_FORMAT_D32_FLOAT, DXGI_SAMPLE_DESC{samples, 0});
    return set;
}
} // namespace

struct HostPostProcessD3D12::Impl
{
    ComPtr<ID3D12Device> device;
    std::uint64_t frequency{};
    ComPtr<ID3D12RootSignature> root;
    // Pipelines are built in the background (R28b policy: no present waits for a
    // compile); until the set of this target configuration is adopted, frames keep
    // the single pass. A failed build disables the split for this renderer.
    std::optional<PipelineSet> pipelines;
    std::future<PipelineSet> pipeline_job;
    bool pipelines_failed{};
    // Size-dependent resources. Resting states between frames: scene, levels
    // and emissive_resolved PIXEL_SHADER_RESOURCE; emissive RENDER_TARGET.
    unsigned width{}, height{}, samples{};
    DXGI_FORMAT color_format{DXGI_FORMAT_UNKNOWN}, resolve_format{DXGI_FORMAT_UNKNOWN};
    ComPtr<ID3D12Resource> scene, emissive, emissive_resolved;
    std::array<ComPtr<ID3D12Resource>, 5> levels; // down q, down e, down s, up e, up q
    bool bloom_ready{};
    // NEW HOST (R35) targets built ahead (prewarm_targets); taken by ensure() when they match.
    std::future<PrebuiltTargets> target_job;
    std::optional<PrebuiltTargets> prebuilt;
    void adopt_prebuilt() noexcept
    {
        try
        {
            if (target_job.valid() && target_job.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                prebuilt = target_job.get();
        }
        catch (...)
        {
            prebuilt.reset(); // ensure() creates them itself, as before
        }
    }
    ComPtr<ID3D12DescriptorHeap> srvs, rtvs;
    UINT srv_step{}, rtv_step{};
    ComPtr<ID3D12QueryHeap> queries;
    ComPtr<ID3D12Resource> readback;
    struct Pending
    {
        bool timed{}, bloom{};
    };
    std::array<Pending, timestamp_slots> pending{};
    // Plan of the prepared frame.
    struct Stream
    {
        std::shared_ptr<CapturedPolygonFrame> frame;
        const PolygonDeviceState *initial{};
        std::shared_ptr<const PolygonGpuTextures> textures;
        std::vector<const CapturedPolygonDraw *> sources; // NEW HOST (R33): the draw each copy was taken from
    };
    std::vector<Stream> streams;
    bool planned{}, plan_fxaa{}, plan_bloom{};
    std::optional<PolygonLogicalRasterExtent> canvas;
    // Recording of the prepared frame.
    ID3D12GraphicsCommandList *list{};
    unsigned slot{};
    HostPostTargets targets{};
    bool recording{}, done{}, bloom_done{};
    std::vector<PolygonGpuRecording> retained;

    Impl(ID3D12Device &d, std::uint64_t timestamp_frequency) : device(&d), frequency(timestamp_frequency)
    {
        D3D12_DESCRIPTOR_RANGE range{};
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.NumDescriptors = 2;
        D3D12_ROOT_PARAMETER parameters[2]{};
        parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[0].DescriptorTable = {1, &range};
        parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[1].Constants = {0, 0, sizeof(PassConstants) / 4};
        parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.MaxAnisotropy = 1;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampler.MaxLOD = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC description{};
        description.NumParameters = 2;
        description.pParameters = parameters;
        description.NumStaticSamplers = 1;
        description.pStaticSamplers = &sampler;
        description.Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
        ComPtr<ID3DBlob> serialized, error;
        checked(D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error),
                "host post-process root serialization");
        checked(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
                                            IID_PPV_ARGS(&root)),
                "host post-process root");
        srv_step = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_DESCRIPTOR_HEAP_DESC heap_description{};
        heap_description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heap_description.NumDescriptors = target_count;
        checked(device->CreateDescriptorHeap(&heap_description, IID_PPV_ARGS(&rtvs)), "host post-process RTV heap");
        rtv_step = device->GetDescriptorHandleIncrementSize(heap_description.Type);
        D3D12_QUERY_HEAP_DESC query_description{};
        query_description.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        query_description.Count = timestamp_slots * 4;
        checked(device->CreateQueryHeap(&query_description, IID_PPV_ARGS(&queries)), "host post-process timestamps");
        D3D12_RESOURCE_DESC buffer{};
        buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width = timestamp_slots * 4 * sizeof(std::uint64_t);
        buffer.Height = buffer.DepthOrArraySize = buffer.MipLevels = 1;
        buffer.SampleDesc.Count = 1;
        buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        const auto properties = heap(D3D12_HEAP_TYPE_READBACK);
        checked(device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &buffer,
                                                D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)),
                "host post-process timestamp readback");
    }
    D3D12_CPU_DESCRIPTOR_HANDLE rtv(unsigned index) const
    {
        auto handle = rtvs->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += std::size_t(index) * rtv_step;
        return handle;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE table(Table pair) const
    {
        auto handle = srvs->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += std::uint64_t(pair) * 2u * srv_step;
        return handle;
    }
    ComPtr<ID3D12Resource> texture(unsigned w, unsigned h, DXGI_FORMAT format, unsigned sample_count,
                                   D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state,
                                   const D3D12_CLEAR_VALUE *clear, const char *what) const
    {
        return make_texture(*device.Get(), w, h, format, sample_count, flags, state, clear, what); // R35: shared
    }
    void write_srv(ID3D12DescriptorHeap &target, unsigned index, ID3D12Resource &resource, DXGI_FORMAT format) const
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC view{};
        view.Format = format;
        view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        view.Texture2D.MipLevels = 1;
        auto handle = target.GetCPUDescriptorHandleForHeapStart();
        handle.ptr += std::size_t(index) * srv_step;
        device->CreateShaderResourceView(&resource, &view, handle);
    }
    // A NEW shader-visible heap whenever the views change: submissions still in
    // flight keep reading their own heap (retained through their fence), so no
    // descriptor is rewritten under the GPU. Every table entry is a valid view
    // (unused t1 slots repeat t0).
    void write_descriptors()
    {
        D3D12_DESCRIPTOR_HEAP_DESC description{};
        description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        description.NumDescriptors = table_count * 2;
        description.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ComPtr<ID3D12DescriptorHeap> next;
        checked(device->CreateDescriptorHeap(&description, IID_PPV_ARGS(&next)), "host post-process SRV heap");
        auto &views = *next.Get();
        write_srv(views, table_composite * 2, *scene.Get(), color_format);
        write_srv(views, table_composite * 2 + 1, bloom_ready ? *levels[4].Get() : *scene.Get(),
                  bloom_ready ? level_format : color_format);
        if (bloom_ready)
        {
            auto &source = samples > 1 ? *emissive_resolved.Get() : *emissive.Get();
            write_srv(views, table_reduce * 2, source, color_format);
            write_srv(views, table_reduce * 2 + 1, source, color_format);
            const std::array<std::array<unsigned, 2>, 4> pairs{{{0, 0}, {1, 1}, {2, 1}, {3, 0}}};
            const std::array<Table, 4> tables{table_down_q, table_down_e, table_up_e, table_up_q};
            for (std::size_t i = 0; i < pairs.size(); ++i)
            {
                write_srv(views, tables[i] * 2, *levels[pairs[i][0]].Get(), level_format);
                write_srv(views, tables[i] * 2 + 1, *levels[pairs[i][1]].Get(), level_format);
            }
        }
        srvs = std::move(next);
    }
    // (Re)creates what this frame needs. Old objects stay alive through the
    // retention record of every submission that used them.
    void ensure(const HostPostRequest &r, bool bloom)
    {
        const auto scene_format = r.resolve_format == r.color_format ? r.color_format : DXGI_FORMAT_R8G8B8A8_TYPELESS;
        adopt_prebuilt(); // NEW HOST (R35)
        const bool use_prebuilt =
            prebuilt && prebuilt->same(r.width, r.height, r.samples.Count, r.color_format, r.resolve_format);
        if (!scene || width != r.width || height != r.height || samples != r.samples.Count ||
            color_format != r.color_format || resolve_format != r.resolve_format)
        {
            scene.Reset();
            emissive.Reset();
            emissive_resolved.Reset();
            for (auto &level : levels)
                level.Reset();
            bloom_ready = false;
            width = r.width;
            height = r.height;
            samples = r.samples.Count;
            color_format = r.color_format;
            resolve_format = r.resolve_format;
            if (use_prebuilt && prebuilt->scene)
                scene = std::move(prebuilt->scene); // NEW HOST (R35)
            else
                scene = texture(width, height, scene_format, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, "host post-process scene copy");
            D3D12_RENDER_TARGET_VIEW_DESC view{};
            view.Format = color_format;
            view.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            device->CreateRenderTargetView(scene.Get(), &view, rtv(target_scene));
            write_descriptors();
        }
        if (!bloom)
            return;
        if (!bloom_ready && use_prebuilt && prebuilt->bloom && prebuilt->emissive)
        {
            // NEW HOST (R35): the prebuilt bloom targets (same descriptions as below).
            emissive = std::move(prebuilt->emissive);
            emissive_resolved = std::move(prebuilt->emissive_resolved);
            levels = std::move(prebuilt->levels);
            prebuilt.reset();
            D3D12_RENDER_TARGET_VIEW_DESC view{};
            view.Format = color_format;
            view.ViewDimension = samples > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
            device->CreateRenderTargetView(emissive.Get(), &view, rtv(target_emissive));
            for (std::size_t i = 0; i < levels.size(); ++i)
                device->CreateRenderTargetView(levels[i].Get(), nullptr, rtv(target_down_q + unsigned(i)));
            bloom_ready = true;
            write_descriptors();
        }
        if (!bloom_ready)
        {
            D3D12_CLEAR_VALUE black{};
            black.Format = color_format;
            emissive = texture(width, height, color_format, samples, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                               D3D12_RESOURCE_STATE_RENDER_TARGET, &black, "host bloom emissive target");
            if (samples > 1)
                emissive_resolved =
                    texture(width, height, color_format, 1, D3D12_RESOURCE_FLAG_NONE,
                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, "host bloom emissive resolve");
            const unsigned q_w = reduced(width, 4), q_h = reduced(height, 4);
            const std::array<std::array<unsigned, 2>, 5> extents{{{q_w, q_h},
                                                                  {reduced(q_w, 2), reduced(q_h, 2)},
                                                                  {reduced(q_w, 4), reduced(q_h, 4)},
                                                                  {reduced(q_w, 2), reduced(q_h, 2)},
                                                                  {q_w, q_h}}};
            for (std::size_t i = 0; i < levels.size(); ++i)
                levels[i] =
                    texture(extents[i][0], extents[i][1], level_format, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, "host bloom level");
            D3D12_RENDER_TARGET_VIEW_DESC view{};
            view.Format = color_format;
            view.ViewDimension = samples > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
            device->CreateRenderTargetView(emissive.Get(), &view, rtv(target_emissive));
            for (std::size_t i = 0; i < levels.size(); ++i)
                device->CreateRenderTargetView(levels[i].Get(), nullptr, rtv(target_down_q + unsigned(i)));
            bloom_ready = true;
            write_descriptors();
        }
    }
    // Adopts a finished background build; true when the pipelines of this target
    // configuration exist. Never waits: a missing set starts (or keeps) its build.
    bool pipelines_ready(unsigned sample_count, DXGI_FORMAT format) noexcept
    {
        if (pipelines_failed)
            return false;
        try
        {
            if (pipeline_job.valid() && pipeline_job.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                pipelines = pipeline_job.get(); // the replaced set stays alive in the recordings that used it
            if (pipelines && pipelines->samples == sample_count && pipelines->format == format)
                return true;
            if (!pipeline_job.valid())
                pipeline_job = std::async(std::launch::async, build_pipeline_set, device, root, sample_count, format);
        }
        catch (const std::exception &failure)
        {
            pipelines_failed = true;
            OutputDebugStringA("StuntGP NEW: HUD split / bloom pipelines unavailable: ");
            OutputDebugStringA(failure.what());
            OutputDebugStringA("\n");
        }
        catch (...)
        {
            pipelines_failed = true;
        }
        return false;
    }
    // The emissive recorder follows the main recorder's sampling (A2C never
    // applies to blended draws). Its PS variants and the pipelines of the states
    // it has seen are prewarmed in the background (PolygonD3D12 R28b); until
    // they exist the frame has no bloom. A light state seen for the first time
    // is created on first use, like in the main recorder.
    bool emissive_ready(const HostPostRequest &r)
    {
        auto &emissive_recorder = *pipelines->emissive;
        auto quality = r.quality;
        quality.alpha_to_coverage = false;
        if (!emissive_recorder.pipelines_prewarmed(r.samples, quality))
            return false;
        emissive_recorder.set_raster_quality(quality);
        emissive_recorder.set_precise_vertex_mapping(r.precise_vertex_mapping);
        return true;
    }
    // The additive draws of the light regions (world3d and screen3d), each
    // preceded by exactly the state it was drawn with (depth writes off).
    // Returns the number of world_end markers of the whole submission.
    unsigned scan(std::span<const HostPostSegment> segments, bool bloom)
    {
        unsigned world_ends = 0;
        bool light_region = false;
        for (const auto &segment : segments)
        {
            const auto &frame = *segment.commands;
            std::shared_ptr<CapturedPolygonFrame> out;
            std::vector<const CapturedPolygonDraw *> sources; // NEW HOST (R33)
            std::optional<PolygonDeviceState> current, emitted;
            if (bloom)
            {
                current = *segment.initial;
                emitted = *segment.initial;
            }
            bool dirty = true;
            std::size_t next = 0;
            const auto markers_before = [&](std::size_t index) {
                for (; next < frame.gpu_insertions.size() && frame.gpu_insertions[next].before_command <= index; ++next)
                {
                    const auto marker = host_frame_phase::marker_of(frame.gpu_insertions[next]);
                    if (!marker)
                        continue;
                    world_ends += *marker == Marker::world_end ? 1u : 0u;
                    light_region = *marker == Marker::world_begin || *marker == Marker::screen_begin;
                }
            };
            if (!bloom)
            {
                markers_before(frame.commands.size());
                continue;
            }
            for (std::size_t index = 0; index < frame.commands.size(); ++index)
            {
                markers_before(index);
                const auto &command = frame.commands[index];
                if (const auto *render = std::get_if<CapturedRenderState>(&command))
                {
                    if (render->state < current->render.size())
                        current->render[render->state] = render->value;
                    dirty = true;
                }
                else if (const auto *stage = std::get_if<CapturedTextureStageState>(&command))
                {
                    if (stage->stage < 2 && stage->state < 32)
                        current->stage[stage->stage][stage->state] = stage->value;
                    dirty = true;
                }
                else if (const auto *bound = std::get_if<CapturedTexture>(&command))
                {
                    if (bound->stage < 2)
                        current->texture[bound->stage] = bound->token;
                    dirty = true;
                }
                else if (light_region && additive(*current))
                {
                    const auto &draw = std::get<CapturedPolygonDraw>(command);
                    if (!out)
                    {
                        out = std::make_shared<CapturedPolygonFrame>();
                        out->source_tick = frame.source_tick;
                        out->resource_epoch = frame.resource_epoch;
                    }
                    if (dirty)
                        emit_state(*out, *emitted, *current);
                    dirty = false;
                    out->commands.push_back(draw);
                    sources.push_back(&draw);
                    out->payload_bytes +=
                        draw.vertices.size() * sizeof(draw.vertices[0]) + draw.indices.size() * sizeof(draw.indices[0]);
                }
            }
            markers_before(frame.commands.size());
            if (out)
                streams.push_back({std::move(out), segment.initial, segment.textures, std::move(sources)});
        }
        return world_ends;
    }
    static void emit_state(CapturedPolygonFrame &out, PolygonDeviceState &emitted, const PolygonDeviceState &current)
    {
        for (std::uint32_t state = 0; state < current.render.size(); ++state)
        {
            // ZWRITEENABLE (14) off: the re-drawn lights never change the frame's depth.
            const auto wanted = state == 14 ? std::optional<std::uint32_t>(0u) : current.render[state];
            if (wanted && emitted.render[state] != wanted)
            {
                out.commands.push_back(CapturedRenderState{state, *wanted});
                emitted.render[state] = wanted;
            }
        }
        for (std::uint32_t stage = 0; stage < 2; ++stage)
        {
            for (std::uint32_t state = 0; state < 32; ++state)
            {
                const auto &wanted = current.stage[stage][state];
                if (wanted && emitted.stage[stage][state] != wanted)
                {
                    out.commands.push_back(CapturedTextureStageState{stage, state, *wanted});
                    emitted.stage[stage][state] = wanted;
                }
            }
            const auto &token = current.texture[stage];
            if (token && emitted.texture[stage] != token)
            {
                out.commands.push_back(CapturedTexture{stage, *token});
                emitted.texture[stage] = token;
            }
        }
    }
    void bind_passes(ID3D12GraphicsCommandList &command_list) const
    {
        command_list.SetGraphicsRootSignature(root.Get());
        ID3D12DescriptorHeap *heaps[]{srvs.Get()};
        command_list.SetDescriptorHeaps(1, heaps);
        command_list.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }
    // One full-screen pass into a bloom level (resting PIXEL_SHADER_RESOURCE).
    void level_pass(ID3D12GraphicsCommandList &command_list, ID3D12PipelineState &pso, unsigned target_level,
                    Table pair, ID3D12Resource &image, ID3D12Resource *extra) const
    {
        auto &target = *levels[target_level].Get();
        const auto size = target.GetDesc();
        const auto image_size = image.GetDesc();
        PassConstants constants{};
        constants.rcp_target[0] = 1.0f / static_cast<float>(size.Width);
        constants.rcp_target[1] = 1.0f / static_cast<float>(size.Height);
        constants.rcp_image[0] = 1.0f / static_cast<float>(image_size.Width);
        constants.rcp_image[1] = 1.0f / static_cast<float>(image_size.Height);
        if (extra)
        {
            const auto extra_size = extra->GetDesc();
            constants.rcp_extra[0] = 1.0f / static_cast<float>(extra_size.Width);
            constants.rcp_extra[1] = 1.0f / static_cast<float>(extra_size.Height);
        }
        auto b = transition(target, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        command_list.ResourceBarrier(1, &b);
        const auto output = rtv(target_down_q + target_level);
        command_list.OMSetRenderTargets(1, &output, FALSE, nullptr);
        const D3D12_VIEWPORT viewport{0, 0, static_cast<float>(size.Width), static_cast<float>(size.Height), 0, 1};
        const D3D12_RECT rect{0, 0, static_cast<LONG>(size.Width), static_cast<LONG>(size.Height)};
        command_list.RSSetViewports(1, &viewport);
        command_list.RSSetScissorRects(1, &rect);
        command_list.SetPipelineState(&pso);
        command_list.SetGraphicsRootDescriptorTable(0, table(pair));
        command_list.SetGraphicsRoot32BitConstants(1, sizeof(PassConstants) / 4, &constants, 0);
        command_list.DrawInstanced(3, 1, 0, 0);
        b = transition(target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        command_list.ResourceBarrier(1, &b);
    }
    // Emissive re-draw and blur chain. False: no bloom this frame (the
    // nested recorder validates and allocates before it touches the list).
    bool record_bloom(const PolygonGpuRecordContext &context)
    {
        SGN_PROFILE_SCOPE("gpu.bloom");
        auto &command_list = *list;
        const auto emissive_rtv = rtv(target_emissive);
        const float black[4]{};
        command_list.ClearRenderTargetView(emissive_rtv, black, 0, nullptr);
        command_list.OMSetRenderTargets(1, &emissive_rtv, FALSE, &targets.dsv);
        try
        {
            for (const auto &stream : streams)
                // NEW HOST (R33): in a GPU re-projected present the emissive copies move with their draws.
                retained.push_back(
                    polygon_reprojection_current
                        ? pipelines->emissive->record_reprojected_copy(
                              command_list, context.transient, *stream.frame, *stream.initial, stream.textures, width,
                              height, canvas, *polygon_reprojection_current, stream.sources)
                        : pipelines->emissive->record(command_list, context.transient, *stream.frame, *stream.initial,
                                                      stream.textures, width, height, canvas));
        }
        catch (...)
        {
            return false;
        }
        auto &source = samples > 1 ? *emissive_resolved.Get() : *emissive.Get();
        if (samples > 1)
        {
            const D3D12_RESOURCE_BARRIER before[]{
                transition(*emissive.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE),
                transition(source, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST)};
            command_list.ResourceBarrier(2, before);
            command_list.ResolveSubresource(&source, 0, emissive.Get(), 0, color_format);
            const D3D12_RESOURCE_BARRIER after[]{
                transition(*emissive.Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
                transition(source, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)};
            command_list.ResourceBarrier(2, after);
        }
        else
        {
            const auto b =
                transition(source, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            command_list.ResourceBarrier(1, &b);
        }
        bind_passes(command_list);
        level_pass(command_list, *pipelines->reduce.Get(), 0, table_reduce, source, nullptr);
        level_pass(command_list, *pipelines->down.Get(), 1, table_down_q, *levels[0].Get(), nullptr);
        level_pass(command_list, *pipelines->down.Get(), 2, table_down_e, *levels[1].Get(), nullptr);
        level_pass(command_list, *pipelines->up.Get(), 3, table_up_e, *levels[2].Get(), levels[1].Get());
        level_pass(command_list, *pipelines->up.Get(), 4, table_up_q, *levels[3].Get(), levels[0].Get());
        if (samples == 1)
        {
            const auto b =
                transition(source, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
            command_list.ResourceBarrier(1, &b);
        }
        return true;
    }
    // The finished 3D part as one shader-readable single-sample image (FXAA input).
    void copy_scene()
    {
        auto &command_list = *list;
        auto &color = *targets.color;
        const auto &linear = targets.linear_resolve;
        if (samples > 1 && linear.root && linear.pipeline && linear.srvs)
        {
            // The renderer's own linear-light resolve pass, drawn into the scene copy.
            const D3D12_RESOURCE_BARRIER before[]{
                transition(color, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
                transition(*scene.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                           D3D12_RESOURCE_STATE_RENDER_TARGET)};
            command_list.ResourceBarrier(2, before);
            const auto output = rtv(target_scene);
            command_list.OMSetRenderTargets(1, &output, FALSE, nullptr);
            const D3D12_VIEWPORT viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
            const D3D12_RECT rect{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
            command_list.RSSetViewports(1, &viewport);
            command_list.RSSetScissorRects(1, &rect);
            command_list.SetGraphicsRootSignature(linear.root);
            command_list.SetPipelineState(linear.pipeline);
            ID3D12DescriptorHeap *heaps[]{linear.srvs};
            command_list.SetDescriptorHeaps(1, heaps);
            command_list.SetGraphicsRootDescriptorTable(0, linear.srvs->GetGPUDescriptorHandleForHeapStart());
            const std::uint32_t constants[4]{samples, 0, 0, 0};
            command_list.SetGraphicsRoot32BitConstants(1, 4, constants, 0);
            command_list.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            command_list.DrawInstanced(3, 1, 0, 0);
            const D3D12_RESOURCE_BARRIER after[]{
                transition(color, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
                transition(*scene.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)};
            command_list.ResourceBarrier(2, after);
        }
        else if (samples > 1)
        {
            const D3D12_RESOURCE_BARRIER before[]{
                transition(color, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE),
                transition(*scene.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                           D3D12_RESOURCE_STATE_RESOLVE_DEST)};
            command_list.ResourceBarrier(2, before);
            command_list.ResolveSubresource(scene.Get(), 0, &color, 0, resolve_format);
            const D3D12_RESOURCE_BARRIER after[]{
                transition(color, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
                transition(*scene.Get(), D3D12_RESOURCE_STATE_RESOLVE_DEST,
                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)};
            command_list.ResourceBarrier(2, after);
        }
        else
        {
            const D3D12_RESOURCE_BARRIER before[]{
                transition(color, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE),
                transition(*scene.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST)};
            command_list.ResourceBarrier(2, before);
            command_list.CopyResource(scene.Get(), &color);
            const D3D12_RESOURCE_BARRIER after[]{
                transition(color, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
                transition(*scene.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)};
            command_list.ResourceBarrier(2, after);
        }
    }
    // FXAA: scene copy -> FXAA [+ bloom] over every sample of the polygon target.
    // Bloom only: the glow is blended (ONE, ONE) onto every sample; no copy.
    void record_composite(bool fxaa, bool bloom)
    {
        auto &command_list = *list;
        if (fxaa)
            copy_scene();
        bind_passes(command_list);
        command_list.OMSetRenderTargets(1, &targets.rtv, FALSE, nullptr);
        const D3D12_VIEWPORT viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
        const D3D12_RECT rect{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
        command_list.RSSetViewports(1, &viewport);
        command_list.RSSetScissorRects(1, &rect);
        PassConstants constants{};
        constants.rcp_target[0] = constants.rcp_image[0] = 1.0f / static_cast<float>(width);
        constants.rcp_target[1] = constants.rcp_image[1] = 1.0f / static_cast<float>(height);
        constants.rcp_extra[0] = 1.0f / static_cast<float>(reduced(width, 4));
        constants.rcp_extra[1] = 1.0f / static_cast<float>(reduced(height, 4));
        constants.strength = host_bloom_strength / 3.0f;
        command_list.SetPipelineState(pipelines->composite[(fxaa ? 1u : 0u) | (bloom ? 2u : 0u)].Get());
        command_list.SetGraphicsRootDescriptorTable(0, table(table_composite));
        command_list.SetGraphicsRoot32BitConstants(1, sizeof(PassConstants) / 4, &constants, 0);
        command_list.DrawInstanced(3, 1, 0, 0);
    }
    void split(const PolygonGpuRecordContext &context)
    {
        SGN_PROFILE_SCOPE("gpu.hud_split");
        auto &command_list = *list;
        command_list.EndQuery(queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 4);
        const bool bloom = plan_bloom && record_bloom(context);
        command_list.EndQuery(queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 4 + 1);
        if (plan_fxaa || bloom)
            record_composite(plan_fxaa, bloom);
        // The polygon recorder rebinds root, heaps, viewport, scissor and buffers after an insertion.
        command_list.OMSetRenderTargets(1, &targets.rtv, FALSE, &targets.dsv);
        command_list.EndQuery(queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 4 + 2);
        bloom_done = bloom;
    }
    void stop() noexcept
    {
        if (host_frame_phase::active_recorder() == recorder)
            host_frame_phase::set_active_recorder(nullptr);
        recording = false;
        list = nullptr;
    }
    // One record keeps every post object the list references alive through its
    // fence. The module itself also owns them until a later prepare() replaces
    // them, and the nested recordings' pipelines, textures and arena ranges are
    // also owned by the emissive recorder, the segment recordings and the context
    // arena, so a failed allocation here cannot free anything the list uses.
    void retain(std::vector<PolygonGpuRecording> &out) noexcept
    {
        try
        {
            PolygonGpuRecording keep;
            keep.retained_root = root;
            keep.descriptors = srvs;
            for (const auto &resource : {scene, emissive, emissive_resolved})
                if (resource)
                    keep.retained_textures.push_back(resource);
            for (const auto &level : levels)
                if (level)
                    keep.retained_textures.push_back(level);
            for (const auto &pipeline : pipelines->composite)
                if (pipeline)
                    keep.retained_pipelines.push_back(pipeline);
            for (const auto &pipeline : {pipelines->reduce, pipelines->down, pipelines->up})
                keep.retained_pipelines.push_back(pipeline);
            out.push_back(std::move(keep));
            for (auto &nested : retained)
                out.push_back(std::move(nested));
        }
        catch (...)
        {
        }
    }
    host_frame_phase::MarkerRecorder *recorder{}; // the owning HostPostProcessD3D12
};

HostPostProcessD3D12::HostPostProcessD3D12(ID3D12Device &device, std::uint64_t timestamp_frequency)
    : impl_(std::make_unique<Impl>(device, timestamp_frequency))
{
    impl_->recorder = this;
}
HostPostProcessD3D12::~HostPostProcessD3D12()
{
    impl_->stop();
}
bool HostPostProcessD3D12::prepare(std::span<const HostPostSegment> segments, const HostPostRequest &request)
{
    auto &p = *impl_;
    p.planned = false;
    p.streams.clear();
    if (!request.fxaa && !request.bloom)
        return false;
    // Starts the background build on the first frame that wants a post effect
    // (menus included), so the first race frame normally finds it ready.
    if (!p.pipelines_ready(request.samples.Count, request.color_format))
        return false;
    unsigned world_ends = 0;
    {
        SGN_PROFILE_SCOPE("gpu.post_plan");
        world_ends = p.scan(segments, request.bloom);
    }
    // Exactly one race view: one 3D -> 2D boundary. Anything else keeps the single pass.
    const bool bloom = request.bloom && !p.streams.empty() && p.emissive_ready(request);
    if (world_ends != 1 || (!request.fxaa && !bloom))
    {
        p.streams.clear();
        return false;
    }
    p.ensure(request, bloom);
    p.plan_fxaa = request.fxaa;
    p.plan_bloom = bloom;
    p.canvas = request.canvas;
    p.planned = true;
    return true;
}
void HostPostProcessD3D12::prewarm_targets(unsigned width, unsigned height, DXGI_SAMPLE_DESC samples,
                                           DXGI_FORMAT color_format, DXGI_FORMAT resolve_format, bool bloom) noexcept
{
    auto &p = *impl_;
    try
    {
        p.adopt_prebuilt();
        const bool current = p.scene && p.width == width && p.height == height && p.samples == samples.Count &&
                             p.color_format == color_format && p.resolve_format == resolve_format &&
                             (!bloom || p.bloom_ready);
        if (current || width == 0 || height == 0)
            return;
        if (p.prebuilt && p.prebuilt->same(width, height, samples.Count, color_format, resolve_format) &&
            (!bloom || p.prebuilt->bloom))
            return;
        if (p.target_job.valid())
            return; // one build at a time; the next prewarm after it restarts a mismatching one
        p.target_job = std::async(std::launch::async, build_targets, p.device, width, height, samples.Count,
                                  color_format, resolve_format, bloom);
    }
    catch (...)
    {
        // ensure() creates the targets itself, as before.
    }
}
void HostPostProcessD3D12::prewarm(DXGI_SAMPLE_DESC samples, DXGI_FORMAT color_format,
                                   const PolygonRasterQuality &quality, bool bloom) noexcept
{
    auto &p = *impl_;
    if (!p.pipelines_ready(samples.Count, color_format) || !bloom)
        return; // the set is still building; emissive_ready() prewarms the rest later
    try
    {
        auto emissive_quality = quality;
        emissive_quality.alpha_to_coverage = false;
        p.pipelines->emissive->prewarm_pipelines(samples, emissive_quality);
    }
    catch (...)
    {
        // Retried by emissive_ready(); until then the frame has no bloom.
    }
}
void HostPostProcessD3D12::record_marker(host_frame_phase::Marker marker, const PolygonGpuRecordContext &context)
{
    auto &p = *impl_;
    if (marker != Marker::world_end || !p.recording || p.done || &context.list != p.list)
        return;
    p.done = true;
    p.split(context);
}
HostPostProcessD3D12::Recording::Recording(HostPostProcessD3D12 &owner, ID3D12GraphicsCommandList &list,
                                           unsigned context_slot, const HostPostTargets &targets) noexcept
    : owner_(&owner)
{
    auto &p = *owner.impl_;
    p.list = &list;
    p.slot = context_slot % timestamp_slots;
    p.targets = targets;
    p.recording = p.planned && targets.color;
    p.done = false;
    p.bloom_done = false;
    p.retained.clear();
    if (p.recording)
        host_frame_phase::set_active_recorder(&owner);
}
HostPostProcessD3D12::Recording::~Recording()
{
    if (!owner_)
        return;
    auto &p = *owner_->impl_;
    p.stop();
    p.retained.clear(); // the unexecuted list is closed by the caller; nothing is queued
    p.planned = false;
    p.streams.clear();
}
bool HostPostProcessD3D12::Recording::finish(std::vector<PolygonGpuRecording> &retained)
{
    auto &p = *owner_->impl_;
    const bool split = p.done;
    auto &command_list = *p.list;
    p.stop();
    if (split)
    {
        command_list.ResolveQueryData(p.queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, p.slot * 4, 3, p.readback.Get(),
                                      std::uint64_t(p.slot) * 4 * sizeof(std::uint64_t));
        p.pending[p.slot] = {true, p.bloom_done};
        p.retain(retained);
    }
    p.retained.clear();
    p.planned = false;
    p.streams.clear();
    owner_ = nullptr;
    return split;
}
void HostPostProcessD3D12::retire_context(unsigned context_slot) noexcept
{
    auto &p = *impl_;
    const auto slot = context_slot % timestamp_slots;
    const auto pending = std::exchange(p.pending[slot], {});
    if (!pending.timed || !sgn::host_profile::enabled() || !p.frequency)
        return;
    void *mapped{};
    const D3D12_RANGE range{slot * 4 * sizeof(std::uint64_t), (slot * 4 + 3) * sizeof(std::uint64_t)};
    if (FAILED(p.readback->Map(0, &range, &mapped)))
        return;
    const auto *times = static_cast<const std::uint64_t *>(mapped) + slot * 4;
    const auto nanoseconds = [&](std::uint64_t from, std::uint64_t to) {
        return static_cast<std::int64_t>(static_cast<double>(to - from) * 1e9 / static_cast<double>(p.frequency));
    };
    if (times[2] >= times[0])
        sgn::host_profile::add("gpu.hud_split_execution", nanoseconds(times[0], times[2]));
    if (pending.bloom && times[1] >= times[0])
        sgn::host_profile::add("gpu.bloom_execution", nanoseconds(times[0], times[1]));
    const D3D12_RANGE none{};
    p.readback->Unmap(0, &none);
}
} // namespace sgn
