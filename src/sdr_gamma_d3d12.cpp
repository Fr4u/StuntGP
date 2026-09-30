#include "sgn/sdr_gamma_d3d12.hpp"
#include "sgn/host_shader_cache.hpp"
#include "sgn/renderer.hpp"
#include <d3dcompiler.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <limits>
#include <utility>

namespace sgn
{
namespace
{
using Microsoft::WRL::ComPtr;
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
// Input and output are explicitly UNORM, never SRGB views. Every input texel
// is loaded once without filtering; its stored 8-bit RGB selects one U16 per
// channel. There is no interpolation between ramp entries. Alpha is retained.
constexpr char shader[] = R"(
Texture2D<float4> image : register(t0);
cbuffer Gamma : register(b0) { uint4 words[192]; };
float4 vs(uint id : SV_VertexID) : SV_Position {
    float2 p=float2((id<<1)&2,id&2);
    return float4(p*float2(2,-2)+float2(-1,1),0,1);
}
uint entry(uint index) { return words[index>>2][index&3]; }
float4 ps(float4 position : SV_Position) : SV_Target {
    float4 pixel=image.Load(int3(int2(position.xy),0));
    uint3 index=(uint3)floor(saturate(pixel.rgb)*255.0+0.5);
    float3 rgb=float3(entry(index.r),entry(256+index.g),entry(512+index.b))/65535.0;
    return float4(rgb,pixel.a);
}
)";
// NEW HOST (R23) post anti-aliasing: FXAA 3.11, "quality" algorithm (luma
// edge test, 3x3 edge orientation, two-sided end-of-edge search, sub-pixel
// blend), written for this pass. It reads the resolved UNORM image (display
// gamma values, i.e. perceptual luma) with a bilinear clamp sampler at exact
// pixel centres, and feeds its colour into the SAME exact ramp lookup as the
// plain pass. Alpha is the centre pixel's. Tuning: sub-pixel 0.75, edge
// threshold 0.166, minimum 0.0833, search steps 1,1.5,2,2,2,2,4,8 pixels.
constexpr char fxaa_shader[] = R"(
Texture2D<float4> image : register(t0);
SamplerState bilinear : register(s0);
cbuffer Gamma : register(b0) { uint4 words[192]; };
cbuffer Frame : register(b1) { float2 rcp_frame; float2 unused; };
float4 vs(uint id : SV_VertexID) : SV_Position {
    float2 p=float2((id<<1)&2,id&2);
    return float4(p*float2(2,-2)+float2(-1,1),0,1);
}
uint entry(uint index) { return words[index>>2][index&3]; }
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
    float lengthSign=horzSpan?rcp_frame.y:rcp_frame.x;
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
    float2 offNP=horzSpan?float2(rcp_frame.x,0.0):float2(0.0,rcp_frame.y);
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
float4 ps(float4 position : SV_Position) : SV_Target {
    float4 pixel=image.Load(int3(int2(position.xy),0));
    float3 smoothed=fxaa(position.xy*rcp_frame,pixel.rgb);
    uint3 index=(uint3)floor(saturate(smoothed)*255.0+0.5);
    float3 rgb=float3(entry(index.r),entry(256+index.g),entry(512+index.b))/65535.0;
    return float4(rgb,pixel.a);
}
)";
ComPtr<ID3DBlob> compile_text(const char *text, std::size_t size, const char *name, const char *entry,
                              const char *target)
{
    ComPtr<ID3DBlob> bytecode, errors;
    const auto status = host_shader_cache::compile(text, size, name, nullptr, entry, target,
                                                   D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS |
                                                       D3DCOMPILE_OPTIMIZATION_LEVEL3,
                                                   0, &bytecode, &errors);
    if (FAILED(status))
        throw GraphicsError(
            status, errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize())
                           : "gamma shader compile");
    return bytecode;
}
ComPtr<ID3DBlob> compile(const char *entry, const char *target)
{
    return compile_text(shader, sizeof(shader), "sgn-window-sdr-gamma", entry, target);
}
// NEW HOST (R27) smooth gradients: the dithered variant of a gamma pass is the
// same shader text with only the final store replaced (the plain and FXAA
// passes above are compiled from their unchanged bytes).
//  - Full scale: the game builds its ramp as 8-bit value x step with step 255
//    for the neutral ramp (retail_lighting.cpp set_light_gamma: entry i = i*255,
//    top 65025), while an identity D3D ramp is i*257 (top 65535). Dividing by
//    65535 turns the neutral game white into 253.02 (the audit's "white = 253").
//    The dithered pass divides each channel by its own top entry clamped to
//    [65025, 65535]: the neutral game ramp becomes the exact identity (white
//    255), an i*257 ramp is unchanged, a lifted ramp is rescaled by at most
//    1/65025 * its lift share (< 1 %) so its top also reaches 255.
//  - Dither: static interleaved-gradient noise n in [0,1) per pixel (a low-
//    discrepancy, blue-noise-like pattern), the same for R, G and B (luma-only,
//    no colour noise). code = floor(v*255 + n) is an unbiased +-0.5 LSB dither
//    (its mean is exactly v*255), so the 16-bit ramp precision survives the
//    8-bit store as fine noise instead of bands. Values that already sit on an
//    8-bit code (within 1/1000 LSB) are snapped first and NEVER change: black
//    stays 0, white stays 255, every exact code of an identity image stays
//    exact. The pattern depends only on the pixel position, never on time, so
//    it is temporally stable (no shimmer on static or interpolated frames).
std::string dithered_text(const char *text, bool has_frame_cbuffer)
{
    std::string result(text);
    const auto replace_once = [&result](const std::string &from, const std::string &to) {
        const auto at = result.find(from);
        if (at == std::string::npos || result.find(from, at + 1) != std::string::npos)
            throw FormatError("gamma dither variant: shader text anchor");
        result.replace(at, from.size(), to);
    };
    const std::string frame_cbuffer = "cbuffer Frame : register(b1) { float2 rcp_frame; float2 unused; };";
    const std::string smooth_cbuffer =
        "cbuffer Frame : register(b1) { float2 rcp_frame; float2 unused; float4 full_scale; };";
    const std::string entry = "uint entry(uint index) { return words[index>>2][index&3]; }";
    const std::string store =
        "float3 smooth_store(float3 rgb,float2 position) {\n"
        "    float noise=frac(52.9829189*frac(dot(floor(position),float2(0.06711056,0.00583715))));\n"
        "    float3 x=saturate(rgb)*255.0;\n"
        "    float3 nearest=round(x);\n"
        "    x=abs(x-nearest)<0.001?nearest:x;\n"
        "    return min(floor(x+noise),255.0)/255.0;\n"
        "}";
    if (has_frame_cbuffer)
    {
        replace_once(frame_cbuffer, smooth_cbuffer);
        replace_once(entry, entry + "\n" + store);
    }
    else
        replace_once(entry, entry + "\n" + smooth_cbuffer + "\n" + store);
    replace_once("    float3 rgb=float3(entry(index.r),entry(256+index.g),entry(512+index.b))/65535.0;\n"
                 "    return float4(rgb,pixel.a);",
                 "    float3 rgb=float3(entry(index.r),entry(256+index.g),entry(512+index.b))/full_scale.rgb;\n"
                 "    return float4(smooth_store(rgb,position.xy),pixel.a);");
    return result;
}
// Root of the FXAA and dithered passes: the plain pass's SRV table (t0) and
// ramp CBV (b0), plus `constants` root constants (b1) and one bilinear clamp
// sampler (s0).
ComPtr<ID3D12RootSignature> sampled_root(ID3D12Device &device, UINT constants, const char *serialization,
                                         const char *creation)
{
    D3D12_DESCRIPTOR_RANGE texture{};
    texture.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    texture.NumDescriptors = 1;
    D3D12_ROOT_PARAMETER parameters[3]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable = {1, &texture};
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[1].Descriptor.ShaderRegister = 0;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[2].Constants = {1, 0, constants};
    parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.MaxLOD = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC root{};
    root.NumParameters = 3;
    root.pParameters = parameters;
    root.NumStaticSamplers = 1;
    root.pStaticSamplers = &sampler;
    root.Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
    ComPtr<ID3DBlob> serialized, error;
    ComPtr<ID3D12RootSignature> result;
    checked(D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error), serialization);
    checked(device.CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
                                       IID_PPV_ARGS(&result)),
            creation);
    return result;
}
// Shared fixed-function state of both full-screen passes.
D3D12_GRAPHICS_PIPELINE_STATE_DESC fullscreen_state(ID3D12RootSignature &root, ID3DBlob &vs, ID3DBlob &ps)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};
    state.pRootSignature = &root;
    state.VS = {vs.GetBufferPointer(), vs.GetBufferSize()};
    state.PS = {ps.GetBufferPointer(), ps.GetBufferSize()};
    state.SampleMask = (std::numeric_limits<UINT>::max)();
    state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    state.NumRenderTargets = 1;
    state.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    state.SampleDesc.Count = 1;
    state.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    state.RasterizerState.DepthClipEnable = TRUE;
    state.DepthStencilState.DepthEnable = FALSE;
    state.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    state.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    state.DepthStencilState.FrontFace = {D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
                                         D3D12_COMPARISON_FUNC_ALWAYS};
    state.DepthStencilState.BackFace = state.DepthStencilState.FrontFace;
    auto &blend = state.BlendState.RenderTarget[0];
    blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.LogicOp = D3D12_LOGIC_OP_NOOP;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    return state;
}
} // namespace
SdrGammaD3D12::SdrGammaD3D12(ID3D12Device &device) : device_(&device)
{
    D3D12_DESCRIPTOR_RANGE texture{};
    texture.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    texture.NumDescriptors = 1;
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable = {1, &texture};
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[1].Descriptor.ShaderRegister = 0;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC root{};
    root.NumParameters = 2;
    root.pParameters = parameters;
    root.Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
    ComPtr<ID3DBlob> serialized, error;
    checked(D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error),
            "gamma root serialization");
    checked(device_->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
                                         IID_PPV_ARGS(&root_)),
            "gamma root");
    const auto vs = compile("vs", "vs_5_0"), ps = compile("ps", "ps_5_0");
    const auto state = fullscreen_state(*root_.Get(), *vs.Get(), *ps.Get());
    checked(device_->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&pipeline_)), "gamma fullscreen pipeline");
}
SdrGammaVariant SdrGammaD3D12::build_variant(ID3D12Device &device, bool fxaa, bool dither)
{
    if (!fxaa && !dither)
        throw FormatError("the plain gamma pass is not a variant");
    SdrGammaVariant result;
    if (dither)
    {
        // NEW HOST (R27): smooth-gradient variant of the requested pass (8 root constants).
        result.root = sampled_root(device, 8, "smooth gradient gamma root serialization", "smooth gradient gamma root");
        const auto text = fxaa ? dithered_text(fxaa_shader, true) : dithered_text(shader, false);
        const auto name = fxaa ? "sgn-window-fxaa-gamma-smooth" : "sgn-window-sdr-gamma-smooth";
        const auto vs = compile_text(text.c_str(), text.size() + 1, name, "vs", "vs_5_0");
        const auto ps = compile_text(text.c_str(), text.size() + 1, name, "ps", "ps_5_0");
        const auto state = fullscreen_state(*result.root.Get(), *vs.Get(), *ps.Get());
        checked(device.CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&result.pipeline)),
                "smooth gradient gamma fullscreen pipeline");
        return result;
    }
    // NEW HOST (R23): same SRV table and ramp CBV as the plain pass, plus the
    // reciprocal frame size (b1) and one bilinear clamp sampler (s0).
    result.root = sampled_root(device, 4, "FXAA root serialization", "FXAA root");
    const auto vs = compile_text(fxaa_shader, sizeof(fxaa_shader), "sgn-window-fxaa-gamma", "vs", "vs_5_0");
    const auto ps = compile_text(fxaa_shader, sizeof(fxaa_shader), "sgn-window-fxaa-gamma", "ps", "ps_5_0");
    const auto state = fullscreen_state(*result.root.Get(), *vs.Get(), *ps.Get());
    checked(device.CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&result.pipeline)),
            "FXAA gamma fullscreen pipeline");
    return result;
}
bool SdrGammaD3D12::has_variant(bool fxaa, bool dither) const noexcept
{
    if (dither)
        return (fxaa ? fxaa_dither_pipeline_ : dither_pipeline_) != nullptr;
    return !fxaa || fxaa_pipeline_ != nullptr;
}
void SdrGammaD3D12::adopt_variant(bool fxaa, bool dither, SdrGammaVariant variant)
{
    if (!variant.root || !variant.pipeline || (!fxaa && !dither))
        throw FormatError("gamma variant adoption requires a built fxaa/dither variant");
    if (has_variant(fxaa, dither))
        return; // keep the pipeline recorded lists already reference
    auto &root = dither ? (fxaa ? fxaa_dither_root_ : dither_root_) : fxaa_root_;
    auto &pipeline = dither ? (fxaa ? fxaa_dither_pipeline_ : dither_pipeline_) : fxaa_pipeline_;
    root = std::move(variant.root);
    pipeline = std::move(variant.pipeline);
}
void SdrGammaD3D12::prepare(SdrGammaFrame &frame, const D3D12_RESOURCE_DESC &back, const SdrGammaRamp &ramp, bool fxaa,
                            bool dither)
{
    // R28b: normally prebuilt off the render thread and adopted; built here only as a fallback.
    if ((fxaa || dither) && !has_variant(fxaa, dither))
        adopt_variant(fxaa, dither, build_variant(*device_.Get(), fxaa, dither));
    if (back.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || back.Format != DXGI_FORMAT_R8G8B8A8_UNORM ||
        back.SampleDesc.Count != 1 || back.DepthOrArraySize != 1 || back.MipLevels != 1 || !back.Width || !back.Height)
        throw FormatError("gamma pass requires resolved SDR R8G8B8A8_UNORM image");
    if (frame.input)
    {
        const auto d = frame.input->GetDesc();
        if (d.Width != back.Width || d.Height != back.Height)
            throw FormatError("gamma frame cache was not retired on resize");
    }
    else
    {
        SdrGammaFrame next;
        auto description = back;
        description.Flags = D3D12_RESOURCE_FLAG_NONE;
        const auto gpu = heap(D3D12_HEAP_TYPE_DEFAULT);
        checked(device_->CreateCommittedResource(&gpu, D3D12_HEAP_FLAG_NONE, &description,
                                                 D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&next.input)),
                "gamma image copy allocation");
        D3D12_RESOURCE_DESC buffer{};
        buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width = 768u * sizeof(std::uint32_t);
        buffer.Height = buffer.DepthOrArraySize = buffer.MipLevels = 1;
        buffer.SampleDesc.Count = 1;
        buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        static_assert((768u * sizeof(std::uint32_t)) % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0);
        const auto upload = heap(D3D12_HEAP_TYPE_UPLOAD);
        checked(device_->CreateCommittedResource(&upload, D3D12_HEAP_FLAG_NONE, &buffer,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                 IID_PPV_ARGS(&next.constants)),
                "gamma constants allocation");
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 1;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        checked(device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&next.descriptors)), "gamma descriptor heap");
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Texture2D.MipLevels = 1;
        device_->CreateShaderResourceView(next.input.Get(), &srv,
                                          next.descriptors->GetCPUDescriptorHandleForHeapStart());
        frame = std::move(next);
    }
    // Exactly one frame-local upload, overwritten only after its fence. The
    // expanded UINT representation matches uint4[192] without HLSL array padding.
    std::array<std::uint32_t, 768> expanded{};
    for (std::size_t i = 0; i < expanded.size(); ++i)
        expanded[i] = ramp[i];
    void *mapped{};
    D3D12_RANGE no_read{};
    checked(frame.constants->Map(0, &no_read, &mapped), "gamma constants map");
    std::memcpy(mapped, expanded.data(), sizeof(expanded));
    D3D12_RANGE written{0, sizeof(expanded)};
    frame.constants->Unmap(0, &written);
    // NEW HOST (R27): per-channel full scale of the dithered pass (see dithered_text).
    for (unsigned channel = 0; channel < 3; ++channel)
        frame.full_scale[channel] =
            dither ? static_cast<float>(std::clamp<unsigned>(ramp[channel * 256 + 255], 65025u, 65535u)) : 65535.0f;
}
void SdrGammaD3D12::record(ID3D12GraphicsCommandList &list, SdrGammaFrame &frame, ID3D12Resource &back,
                           D3D12_RESOURCE_STATES before, D3D12_CPU_DESCRIPTOR_HANDLE output, unsigned width,
                           unsigned height, bool fxaa, bool dither) const noexcept
{
    // prepare(...) created every pipeline asked for here; record never allocates.
    ID3D12PipelineState *const smooth_pipeline = fxaa ? fxaa_dither_pipeline_.Get() : dither_pipeline_.Get();
    ID3D12RootSignature *const smooth_root = fxaa ? fxaa_dither_root_.Get() : dither_root_.Get();
    const bool smooth = dither && smooth_root != nullptr && smooth_pipeline != nullptr; // NEW HOST (R27)
    const bool smoothing = !smooth && fxaa && fxaa_pipeline_ != nullptr;
    auto b = transition(back, before, D3D12_RESOURCE_STATE_COPY_SOURCE);
    list.ResourceBarrier(1, &b);
    list.CopyResource(frame.input.Get(), &back);
    std::array<D3D12_RESOURCE_BARRIER, 2> ready{
        transition(back, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
        transition(*frame.input.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)};
    list.ResourceBarrier(static_cast<UINT>(ready.size()), ready.data());
    list.OMSetRenderTargets(1, &output, FALSE, nullptr);
    D3D12_VIEWPORT viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
    D3D12_RECT rect{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    list.RSSetViewports(1, &viewport);
    list.RSSetScissorRects(1, &rect);
    list.SetGraphicsRootSignature(smooth ? smooth_root : smoothing ? fxaa_root_.Get() : root_.Get());
    list.SetPipelineState(smooth ? smooth_pipeline : smoothing ? fxaa_pipeline_.Get() : pipeline_.Get());
    ID3D12DescriptorHeap *heaps[]{frame.descriptors.Get()};
    list.SetDescriptorHeaps(1, heaps);
    list.SetGraphicsRootDescriptorTable(0, frame.descriptors->GetGPUDescriptorHandleForHeapStart());
    list.SetGraphicsRootConstantBufferView(1, frame.constants->GetGPUVirtualAddress());
    if (smoothing)
    {
        const float frame_size[4]{1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height), 0.0f, 0.0f};
        list.SetGraphicsRoot32BitConstants(2, 4, frame_size, 0);
    }
    if (smooth)
    {
        const float constants[8]{1.0f / static_cast<float>(width),
                                 1.0f / static_cast<float>(height),
                                 0.0f,
                                 0.0f,
                                 frame.full_scale[0],
                                 frame.full_scale[1],
                                 frame.full_scale[2],
                                 1.0f};
        list.SetGraphicsRoot32BitConstants(2, 8, constants, 0);
    }
    list.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list.DrawInstanced(3, 1, 0, 0);
    b = transition(*frame.input.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    list.ResourceBarrier(1, &b);
}
} // namespace sgn
