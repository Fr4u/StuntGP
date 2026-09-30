#include "sgn/polygon_d3d12.hpp"
#include "sgn/host_shader_cache.hpp"
#include "sgn/polygon_reprojection.hpp"
#include "sgn/host_profiler.hpp"
#include "sgn/host_texture_processing.hpp"
#include "sgn/host_wide_layout.hpp"
#include "sgn/format_require.hpp"
#include <d3dcompiler.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <future>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <vector>
#include <sstream>
#include <string>
#include <unordered_map>

namespace sgn
{
using Microsoft::WRL::ComPtr;
namespace
{
void check(HRESULT value, const char *action)
{
    if (FAILED(value))
    {
        std::ostringstream s;
        s << action << " HRESULT 0x" << std::hex << static_cast<std::uint32_t>(value);
        throw std::runtime_error(s.str());
    }
}
using sgn::format_check::require;
std::uint32_t value(const std::optional<std::uint32_t> &v)
{
    require(v.has_value(), "polygon device state has no explicit initialization");
    return *v;
}
std::uint32_t rs(const PolygonDeviceState &s, unsigned i)
{
    return value(s.render.at(i));
}
std::uint32_t ts(const PolygonDeviceState &s, unsigned stage, unsigned i)
{
    return value(s.stage.at(stage).at(i));
}
std::uint32_t boolean(std::uint32_t v)
{
    require(v <= 1, "polygon device Boolean state outside supported domain");
    return v;
}
std::uint32_t comparison(std::uint32_t v)
{
    require(v >= 1 && v <= 8, "polygon device comparison state outside supported domain");
    return v;
}
std::uint32_t blend(std::uint32_t v)
{
    require(v >= 1 && v <= 10, "polygon device blend state outside supported domain");
    return v;
}
// D3D12 has the same first ten blend/comparison enum values as the legacy API.
struct PipelineKey
{
    std::uint32_t z, zwrite, zfunc, blend_enabled, source, destination, cull, topology;
    std::uint32_t precise_vs{};           // NEW HOST (R19) opt-in VS variant (gpu_world hybrid mode)
    std::uint32_t alpha_to_coverage{};    // NEW HOST (R23): A2C PS variant + AlphaToCoverageEnable
    std::uint32_t smooth_magnification{}; // NEW HOST (R28): Catmull-Rom magnification PS variant
    std::uint32_t reproject{};            // NEW HOST (R33): GPU re-projection VS + slot stream (same PS/state)
    auto operator<=>(const PipelineKey &) const = default;
};
// NEW HOST (R23): the sample count is part of the external key (bits 21..26)
// because an insertion's cache outlives an MSAA change of the recorder.
std::uint64_t pipeline_key64(const PipelineKey &k, std::uint32_t sample_count)
{
    return std::uint64_t(k.z) | std::uint64_t(k.zwrite) << 1 | std::uint64_t(k.zfunc) << 2 |
           std::uint64_t(k.blend_enabled) << 6 | std::uint64_t(k.source) << 7 | std::uint64_t(k.destination) << 11 |
           std::uint64_t(k.cull) << 15 | std::uint64_t(k.topology) << 17 | std::uint64_t(k.precise_vs) << 19 |
           std::uint64_t(k.alpha_to_coverage) << 20 | std::uint64_t(sample_count & 63u) << 21 |
           std::uint64_t(k.smooth_magnification) << 27 | std::uint64_t(k.reproject) << 28;
}
// NEW HOST (R19) plan record: no per-draw heap allocation. Vertex/index positions
// address the recording-wide vertex region and its R32 absolute index region.
struct PlannedDraw
{
    const CapturedPolygonDraw *source; // null for a planned host insertion
    std::uint32_t type{};
    PipelineKey pipeline;
    ID3D12PipelineState *pso{};
    std::array<std::uint32_t, 24> constants{};
    std::array<const PolygonGpuTexture *, 2> textures{};
    std::size_t first_vertex{}, first_index{}, index_count{}, sampler_pair{}, texture_pair{};
    bool uses_indices{};
    bool drop_nonpositive_rhw{};               // NEW HOST policy (PORT-FIX-012): 432F60 fan triangles behind the camera
    bool coverage_capable{}, smooth_capable{}; // NEW HOST (R28b): variant eligibility, quality-independent
};
using SamplerKey = std::array<std::uint32_t, 13>;
using SamplerPairKey = std::array<SamplerKey, 2>;
SamplerKey sampler_key(const D3D12_SAMPLER_DESC &s)
{
    // Explicit fields avoid padding-dependent equality. Float payload bits,
    // including signed zero, are retained rather than numerically compared.
    return {static_cast<std::uint32_t>(s.Filter),           static_cast<std::uint32_t>(s.AddressU),
            static_cast<std::uint32_t>(s.AddressV),         static_cast<std::uint32_t>(s.AddressW),
            std::bit_cast<std::uint32_t>(s.MipLODBias),     s.MaxAnisotropy,
            static_cast<std::uint32_t>(s.ComparisonFunc),   std::bit_cast<std::uint32_t>(s.BorderColor[0]),
            std::bit_cast<std::uint32_t>(s.BorderColor[1]), std::bit_cast<std::uint32_t>(s.BorderColor[2]),
            std::bit_cast<std::uint32_t>(s.BorderColor[3]), std::bit_cast<std::uint32_t>(s.MinLOD),
            std::bit_cast<std::uint32_t>(s.MaxLOD)};
}
constexpr char shader[] = R"(
cbuffer State:register(b0) {
    float2 size; uint alpha_enable,alpha_ref;
    uint alpha_func,specular_enable,enabled0,enabled1;
    uint4 color0,alpha0,color1,alpha1;
};
Texture2D texture0:register(t0); Texture2D texture1:register(t1);
SamplerState sampler0:register(s0); SamplerState sampler1:register(s1);
struct Input {float4 p:POSITION;uint diffuse:COLOR0;uint specular:COLOR1;float4 uv:TEXCOORD0;};
struct Output {float4 p:SV_POSITION;noperspective float4 diffuse:COLOR0;noperspective float4 specular:COLOR1;float4 uv:TEXCOORD0;};
float4 unpack(uint x){return float4((x>>16)&255,(x>>8)&255,x&255,x>>24)/255.0;}
Output vs(Input v){Output o;float w=rcp(v.p.w);
    // New D3D12 raster convention: legacy pixel centers are at integer XY.
    o.p=float4(((v.p.xy+0.5)/size*2.0-1.0)*float2(1,-1)*w,v.p.z*w,w);
    o.diffuse=unpack(v.diffuse);o.specular=unpack(v.specular);o.uv=v.uv;return o;
}
float4 argument(uint arg,float4 diffuse,float4 current,float4 tex){return arg==0?diffuse:arg==1?current:tex;}
float4 operation(uint op,float4 a,float4 b){
    if(op==2)return a;if(op==3)return b;if(op==4)return a*b;
    if(op==5)return saturate(a*b*2);if(op==6)return saturate(a*b*4);return saturate(a+b);
}
float4 stage(uint4 c,uint4 a,float4 diffuse,float4 current,float4 tex){
    float3 rgb=operation(c.x,argument(c.y,diffuse,current,tex),argument(c.z,diffuse,current,tex)).rgb;
    float alpha=operation(a.x,argument(a.y,diffuse,current,tex),argument(a.z,diffuse,current,tex)).a;
    return saturate(float4(rgb,alpha));
}
bool compare_alpha(uint a,uint b){
    if(alpha_func==1)return false;if(alpha_func==2)return a<b;if(alpha_func==3)return a==b;
    if(alpha_func==4)return a<=b;if(alpha_func==5)return a>b;if(alpha_func==6)return a!=b;
    return alpha_func==7?a>=b:true;
}
float4 ps(Output i):SV_TARGET {
    float4 c=i.diffuse;
    if(enabled0!=0){c=stage(color0,alpha0,i.diffuse,c,texture0.Sample(sampler0,color0.w==0?i.uv.xy:i.uv.zw));
        if(enabled1!=0)c=stage(color1,alpha1,i.diffuse,c,texture1.Sample(sampler1,color1.w==0?i.uv.xy:i.uv.zw));}
    if(specular_enable!=0)c.rgb=saturate(c.rgb+i.specular.rgb);
    if(alpha_enable!=0&&!compare_alpha((uint)round(saturate(c.a)*255.0),alpha_ref))discard;
    return c;
}
)";
// NEW HOST (R33) GPU re-projection VS, compiled from the polygon text above plus
// this function (the default VS/PS bytes are compiled from the unchanged text).
// A vertex whose slot moves is un-projected with the exact inverse of its draw's
// 429A40/41E410 mapping (b1: kx, ky and the centre including the host canvas
// offset), moved by its slot's affine motion (the R28 trail blend included, in
// the same order as the CPU presenter), and re-projected to clip space with the
// true w (no division, so w <= 0 is clipped by the hardware); SV_ClipDistance0
// clips at the source near plane (w = clip_near + 1) as the CPU near clip did.
// clip z keeps the captured Z/RHW ratio (the CPU path's Z' = Z*RHW'/RHW, then
// z = Z'/RHW'). Every other vertex takes the unchanged polygon mapping.
constexpr char reprojection_shader[] = R"(
cbuffer Reprojection:register(b1){
    float kx,ky,cxo,cyo;
    float w_near; uint mapping_valid; uint2 reprojection_pad;
    float4 world_x,world_y,world_z;
};
struct Motion {float4 x,y,z,tx,ty,tz,older,axis;};
StructuredBuffer<Motion> motions:register(t2);
struct RInput {float4 p:POSITION;uint diffuse:COLOR0;uint specular:COLOR1;float4 uv:TEXCOORD0;uint slot:BLENDINDICES;};
struct ROutput {float4 p:SV_POSITION;noperspective float4 diffuse:COLOR0;noperspective float4 specular:COLOR1;float4 uv:TEXCOORD0;float clip:SV_ClipDistance0;};
float3 affine(float4 x,float4 y,float4 z,float3 c){return float3(dot(c,x.xyz)+x.w,dot(c,y.xyz)+y.w,dot(c,z.xyz)+z.w);}
ROutput rvs(RInput v){
    ROutput o;o.diffuse=unpack(v.diffuse);o.specular=unpack(v.specular);o.uv=v.uv;o.clip=1.0;
    float rhw=v.p.w;uint flags=0;Motion m=(Motion)0;
    if(v.slot!=65535&&mapping_valid!=0&&rhw>0){m=motions[v.slot];flags=asuint(m.axis.w);}
    [branch] if((flags&1)==0){float w=rcp(rhw);o.p=float4(((v.p.xy+0.5)/size*2.0-1.0)*float2(1,-1)*w,v.p.z*w,w);return o;}
    float w=32.0/rhw;
    float3 c=float3((v.p.x-cxo)*w/kx,(v.p.y-cyo)*w/ky,w-1.0);
    float3 moved=affine(m.x,m.y,m.z,c);
    if((flags&2)!=0){
        float weight=1.0;
        if(m.older.w>0){float3 q=affine(world_x,world_y,world_z,c);weight=saturate(dot(q-m.older.xyz,m.axis.xyz)*m.older.w);}
        moved+=(affine(m.tx,m.ty,m.tz,c)-moved)*weight;
    }
    float w2=moved.z+1.0;
    o.p=float4(((kx*moved.x+(cxo+0.5)*w2)*2.0/size.x-w2)/32.0,
               -((ky*moved.y+(cyo+0.5)*w2)*2.0/size.y-w2)/32.0,
               asuint(v.p.z)==asuint(v.p.w)?1.0:v.p.z/rhw,
               w2/32.0);
    o.clip=w2-w_near;
    return o;
}
)";
ComPtr<ID3DBlob> compile_reprojection_vertex_shader()
{
    const std::string text = std::string(shader) + reprojection_shader;
    ComPtr<ID3DBlob> result, errors;
    const auto status = host_shader_cache::compile(
        text.c_str(), text.size() + 1, "polygon-reprojection", nullptr, "rvs", "vs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &result,
        &errors);
    if (FAILED(status))
        throw std::runtime_error(errors ? static_cast<const char *>(errors->GetBufferPointer())
                                        : "polygon re-projection HLSL compilation failed");
    return result;
}
ComPtr<ID3DBlob> compile(const char *entry, const char *profile)
{
    ComPtr<ID3DBlob> result, errors;
    auto status = host_shader_cache::compile(shader, sizeof(shader), "polygon-pretransformed", nullptr, entry, profile,
                                             D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS |
                                                 D3DCOMPILE_OPTIMIZATION_LEVEL3,
                                             0, &result, &errors);
    if (FAILED(status))
        throw std::runtime_error(errors ? static_cast<const char *>(errors->GetBufferPointer())
                                        : "polygon HLSL compilation failed");
    return result;
}
// NEW HOST (R19) opt-in variant: the same shader text with only the XYZRHW mapping
// marked `precise` (no contraction/reassociation), for gpu_world mode 1 whose
// static-geometry VS must produce the same clip positions from the same bits.
// The default shader above is compiled from its unchanged bytes.
ComPtr<ID3DBlob> compile_precise_vertex_shader()
{
    std::string text(shader);
    const std::string mapping_w = "Output o;float w=rcp(v.p.w);";
    const std::string mapping_p = "o.p=float4(((v.p.xy+0.5)/size*2.0-1.0)*float2(1,-1)*w,v.p.z*w,w);";
    const auto w_at = text.find(mapping_w), p_at = text.find(mapping_p);
    require(w_at != std::string::npos && p_at != std::string::npos && w_at < p_at, "polygon precise VS mapping text");
    text.replace(p_at, mapping_p.size(),
                 "precise float4 p=float4(((v.p.xy+0.5)/size*2.0-1.0)*float2(1,-1)*w,v.p.z*w,w);o.p=p;");
    text.replace(w_at, mapping_w.size(), "Output o;precise float w=rcp(v.p.w);");
    ComPtr<ID3DBlob> result, errors;
    const auto status = host_shader_cache::compile(
        text.c_str(), text.size() + 1, "polygon-pretransformed-precise", nullptr, "vs", "vs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &result,
        &errors);
    if (FAILED(status))
        throw std::runtime_error(errors ? static_cast<const char *>(errors->GetBufferPointer())
                                        : "polygon precise HLSL compilation failed");
    return result;
}
// NEW HOST (R23) alpha-to-coverage PS variant: the same shader text with only
// the alpha-test discard replaced. For the ordered comparisons (LESS, LESSEQUAL,
// GREATER, GREATEREQUAL) the output alpha becomes the pass coverage, sharpened
// around the exact byte threshold of the D3D7 test (half coverage there, full
// or none one screen-space alpha gradient away), and the PSO enables
// AlphaToCoverage. NEVER/EQUAL/NOTEQUAL/ALWAYS keep the exact discard. The
// default PS above is compiled from its unchanged bytes.
void apply_alpha_to_coverage_text(std::string &text)
{
    const std::string test =
        "    if(alpha_enable!=0&&!compare_alpha((uint)round(saturate(c.a)*255.0),alpha_ref))discard;";
    const auto at = text.find(test);
    require(at != std::string::npos && text.find(test, at + 1) == std::string::npos,
            "polygon alpha-to-coverage PS test text");
    // Embedded HLSL: one shader line per string literal, kept unsplit.
    // clang-format off
    text.replace(at, test.size(),
        "    float coverage_alpha=saturate(c.a)*255.0;float coverage_edge=max(fwidth(coverage_alpha),0.0625);\n"
        "    if(alpha_enable!=0){\n"
        "        if(alpha_func==2||alpha_func==4||alpha_func==5||alpha_func==7){\n"
        "            float threshold=(float)alpha_ref+((alpha_func==4||alpha_func==5)?0.5:-0.5);\n"
        "            c.a=saturate(((alpha_func>=5)?coverage_alpha-threshold:threshold-coverage_alpha)/coverage_edge+0.5);\n"
        "        }else if(!compare_alpha((uint)round(saturate(c.a)*255.0),alpha_ref))discard;\n"
        "    }");
    // clang-format on
}
ComPtr<ID3DBlob> compile_pixel_text(const std::string &text, const char *name)
{
    ComPtr<ID3DBlob> result, errors;
    const auto status = host_shader_cache::compile(text.c_str(), text.size() + 1, name, nullptr, "ps", "ps_5_0",
                                                   D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS |
                                                       D3DCOMPILE_OPTIMIZATION_LEVEL3,
                                                   0, &result, &errors);
    if (FAILED(status))
        throw std::runtime_error(errors ? static_cast<const char *>(errors->GetBufferPointer())
                                        : "polygon pixel shader variant HLSL compilation failed");
    return result;
}
ComPtr<ID3DBlob> compile_alpha_to_coverage_pixel_shader()
{
    std::string text(shader);
    apply_alpha_to_coverage_text(text);
    return compile_pixel_text(text, "polygon-pretransformed-a2c");
}
// NEW HOST (R28) smooth magnification PS variant (opt-in): the same shader text
// with only the two stage Sample calls replaced. Where the texture is
// magnified (screen-space UV derivatives under one texel on both axes) the
// stage is reconstructed with a Catmull-Rom bicubic (9 bilinear taps at level
// 0, the separable weights folded into bilinear positions); elsewhere it is
// sampled with SampleGrad and the pixel's own UV gradients (same filtering as
// Sample). The recorder selects this variant only for draws whose enabled
// stages all request LINEAR min and mag filtering and whose vertices are not
// all at one RHW (screen-flat 2D art such as the HUD, text and menus keeps the
// default PS; point-sampled stages never get it). Composable with A2C.
void apply_smooth_magnification_text(std::string &text)
{
    const auto replace_once = [&text](const std::string &from, const std::string &to) {
        const auto at = text.find(from);
        require(at != std::string::npos && text.find(from, at + 1) == std::string::npos,
                "polygon smooth magnification PS text");
        text.replace(at, from.size(), to);
    };
    replace_once("texture0.Sample(sampler0,", "smooth_sample(texture0,sampler0,");
    replace_once("texture1.Sample(sampler1,", "smooth_sample(texture1,sampler1,");
    // Embedded HLSL: one shader line per string literal, kept unsplit.
    // clang-format off
    replace_once("float4 ps(Output i):SV_TARGET {",
        "float4 catmull_rom(Texture2D t,SamplerState s,float2 uv,float2 size){\n"
        "    float2 p=uv*size;float2 t1=floor(p-0.5)+0.5;float2 f=p-t1;\n"
        "    float2 w0=f*(-0.5+f*(1.0-0.5*f));float2 w1=1.0+f*f*(-2.5+1.5*f);\n"
        "    float2 w2=f*(0.5+f*(2.0-1.5*f));float2 w3=f*f*(-0.5+0.5*f);float2 w12=w1+w2;\n"
        "    float2 c0=(t1-1.0)/size;float2 c12=(t1+w2/w12)/size;float2 c3=(t1+2.0)/size;\n"
        "    float4 r=t.SampleLevel(s,float2(c0.x,c0.y),0)*(w0.x*w0.y)+t.SampleLevel(s,float2(c12.x,c0.y),0)*(w12.x*w0.y)+t.SampleLevel(s,float2(c3.x,c0.y),0)*(w3.x*w0.y);\n"
        "    r+=t.SampleLevel(s,float2(c0.x,c12.y),0)*(w0.x*w12.y)+t.SampleLevel(s,float2(c12.x,c12.y),0)*(w12.x*w12.y)+t.SampleLevel(s,float2(c3.x,c12.y),0)*(w3.x*w12.y);\n"
        "    r+=t.SampleLevel(s,float2(c0.x,c3.y),0)*(w0.x*w3.y)+t.SampleLevel(s,float2(c12.x,c3.y),0)*(w12.x*w3.y)+t.SampleLevel(s,float2(c3.x,c3.y),0)*(w3.x*w3.y);\n"
        "    return saturate(r);\n"
        "}\n"
        "float4 smooth_sample(Texture2D t,SamplerState s,float2 uv){\n"
        "    float2 gx=ddx(uv);float2 gy=ddy(uv);uint w,h,levels;t.GetDimensions(0,w,h,levels);\n"
        "    float2 size=float2(w,h);float2 tx=gx*size;float2 ty=gy*size;\n"
        "    float4 result;\n"
        "    [branch] if(max(dot(tx,tx),dot(ty,ty))<1.0) result=catmull_rom(t,s,uv,size); else result=t.SampleGrad(s,uv,gx,gy);\n"
        "    return result;\n"
        "}\n"
        "float4 ps(Output i):SV_TARGET {");
    // clang-format on
}
ComPtr<ID3DBlob> compile_smooth_pixel_shader(bool coverage)
{
    std::string text(shader);
    if (coverage)
        apply_alpha_to_coverage_text(text);
    apply_smooth_magnification_text(text);
    return compile_pixel_text(text, coverage ? "polygon-pretransformed-a2c-smooth" : "polygon-pretransformed-smooth");
}
constexpr D3D12_INPUT_ELEMENT_DESC polygon_input[]{
    {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"COLOR", 0, DXGI_FORMAT_R32_UINT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"COLOR", 1, DXGI_FORMAT_R32_UINT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
// NEW HOST (R33): the same stream plus the per-vertex motion slot (R32, stream 1).
constexpr D3D12_INPUT_ELEMENT_DESC reprojection_input[]{
    {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"COLOR", 0, DXGI_FORMAT_R32_UINT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"COLOR", 1, DXGI_FORMAT_R32_UINT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"BLENDINDICES", 0, DXGI_FORMAT_R32_UINT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
D3D12_SAMPLER_DESC sampler(const PolygonDeviceState &state, unsigned stage, const PolygonRasterQuality &quality,
                           const PolygonGpuTexture &bound)
{
    const auto u = ts(state, stage, 13), v = ts(state, stage, 14), mag = ts(state, stage, 16),
               min = ts(state, stage, 17), mip = ts(state, stage, 18);
    require(u >= 1 && u <= 3 && v >= 1 && v <= 3, "polygon sampler address outside wrap/mirror/clamp domain");
    require(mag >= 1 && mag <= 2 && min >= 1 && min <= 2 && mip <= 2,
            "polygon sampler filter outside point/linear domain");
    D3D12_SAMPLER_DESC s{};
    s.Filter = static_cast<D3D12_FILTER>((min == 2 ? 0x10 : 0) | (mag == 2 ? 4 : 0) | (mip == 2 ? 1 : 0));
    s.AddressU = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(u);
    s.AddressV = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(v);
    s.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    s.MaxAnisotropy = 1;
    s.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    s.MaxLOD = mip == 0 ? 0 : D3D12_FLOAT32_MAX;
    // NEW HOST (R23). Only a stage whose game state asks for LINEAR min AND mag
    // (D3DTFN_LINEAR/D3DTFG_LINEAR) is upgraded; its MIPFILTER (the game has no
    // mips, so it requests D3DTFP_NONE) is overridden to use the whole chain.
    // POINT requests and Filter::original keep the descriptor above, except
    // that D3DTFP_NONE (1) on a texture that now HAS mips samples level 0 only,
    // which is what NONE means (the pre-R23 mapping never met a mipped texture).
    if (quality.filter != PolygonRasterQuality::Filter::original && min == 2 && mag == 2)
    {
        const bool anisotropic = quality.filter == PolygonRasterQuality::Filter::anisotropic;
        s.Filter = anisotropic ? D3D12_FILTER_ANISOTROPIC : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        s.MaxAnisotropy = anisotropic ? quality.anisotropy : 1u;
        s.MinLOD = 0;
        s.MaxLOD = D3D12_FLOAT32_MAX;
        // NEW HOST (R28): texture_sharpness, higher = sharper (upgraded stages only).
        if (quality.texture_sharpness != 0)
            s.MipLODBias = -0.25f * static_cast<float>(quality.texture_sharpness);
    }
    else if (mip == 1 && bound.resource->GetDesc().MipLevels > 1)
        s.MaxLOD = 0;
    return s;
}
// NEW HOST (R30 perf_renderer): the per-token texture() scan moved into record()
// (cached_texture: one flat bank index, the same checks, order and messages).
void validate_stage(std::array<std::uint32_t, 24> &constants, std::array<D3D12_SAMPLER_DESC, 2> &samplers,
                    const PolygonDeviceState &state, unsigned stage, const PolygonRasterQuality &quality,
                    const PolygonGpuTexture &bound)
{
    const auto start = 8 + stage * 8;
    for (unsigned component = 0; component < 2; ++component)
    {
        const auto offset = start + component * 4;
        const auto op = ts(state, stage, 1 + component * 3);
        if (component == 1 && stage != 0 && op == 1)
        {
            // NEW HOST platform policy (not recovered behavior): source world
            // multitexture (433F40 route) enables stage1 COLOROP=ADD but never
            // writes stage1 ALPHAOP, which keeps the D3D7 default DISABLE. D3D7
            // leaves that pair undefined; fixed-function hardware passes the
            // previous stage alpha through. Encode exactly that: SELECTARG1(CURRENT).
            constants[offset] = 2;
            constants[offset + 1] = 1;
            constants[offset + 2] = 1;
            continue;
        }
        require(op >= 2 && op <= 7, "polygon texture operation outside qualified platform subset");
        constants[offset] = op;
        for (unsigned a = 1; a <= 2; ++a)
        {
            auto arg = ts(state, stage, 1 + component * 3 + a);
            require(arg <= 2, "polygon texture argument outside diffuse/current/texture subset");
            constants[offset + a] = arg;
        }
    }
    constants[start + 3] = ts(state, stage, 11);
    require(constants[start + 3] <= 1, "polygon texture coordinate index outside FVF2C4");
    samplers[stage] = sampler(state, stage, quality, bound);
}
std::size_t align_up(std::size_t value, std::size_t alignment)
{
    require(alignment != 0 && (alignment & (alignment - 1)) == 0 &&
                value <= (std::numeric_limits<std::size_t>::max)() - alignment,
            "polygon transient alignment");
    return (value + alignment - 1) & ~(alignment - 1);
}
D3D12_PRIMITIVE_TOPOLOGY topology(std::uint32_t type)
{
    return type == 2   ? D3D_PRIMITIVE_TOPOLOGY_LINELIST
           : type == 5 ? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
                       : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}
using TexturePair = std::array<const PolygonGpuTexture *, 2>;
// NEW HOST (R30 perf_renderer) finiteness of a binary32 word without the CRT
// classification call: finite exactly when the exponent is not all ones, which
// is std::isfinite(std::bit_cast<float>(word)) for every one of the 2^32 words.
constexpr bool finite_word(std::uint32_t word) noexcept
{
    return (word & 0x7F800000u) != 0x7F800000u;
}
static_assert(finite_word(0x00000000u) && finite_word(0x80000001u) && finite_word(0x7F7FFFFFu) &&
              !finite_word(0x7F800000u) && !finite_word(0xFF800000u) && !finite_word(0x7FC00000u) &&
              !finite_word(0xFFFFFFFFu));
// NEW HOST (R30 perf_renderer) open-addressing index from a 64-bit key to a
// 32-bit value, replacing the per-frame std::map/std::unordered_map scratch
// (one heap node per insert). clear() is O(1) (generation stamps) and keeps
// the capacity, so a steady-state frame performs no heap allocation. Several
// slots may carry one key (a hash of a longer key); find() then asks `same`
// which value is the wanted one. Deletion is only by clear().
class FlatIndex
{
    struct Slot
    {
        std::uint64_t key{};
        std::uint32_t value{}, generation{};
    };
    std::vector<Slot> slots_;
    std::size_t size_{};
    std::uint32_t generation_{1}; // a slot is occupied when its generation equals this
    static std::size_t spread(std::uint64_t key) noexcept
    {
        key ^= key >> 33;
        key *= 0xFF51AFD7ED558CCDull;
        key ^= key >> 33;
        return static_cast<std::size_t>(key);
    }
    void place(std::uint64_t key, std::uint32_t value) noexcept
    {
        const auto mask = slots_.size() - 1;
        auto at = spread(key) & mask;
        while (slots_[at].generation == generation_)
            at = (at + 1) & mask;
        slots_[at] = {key, value, generation_};
        ++size_;
    }

  public:
    void clear() noexcept
    {
        size_ = 0;
        if (++generation_ == 0)
        {
            for (auto &slot : slots_)
                slot.generation = 0;
            generation_ = 1;
        }
    }
    void reserve(std::size_t count)
    {
        if (count * 2 <= slots_.size())
            return;
        std::size_t capacity = 64;
        while (capacity < count * 2)
            capacity *= 2;
        std::vector<Slot> old(capacity);
        old.swap(slots_);
        const auto live = generation_;
        generation_ = 1;
        size_ = 0;
        for (const auto &slot : old)
            if (slot.generation == live)
                place(slot.key, slot.value);
    }
    template <class Same> std::uint32_t *find(std::uint64_t key, const Same &same) noexcept
    {
        if (slots_.empty())
            return nullptr;
        const auto mask = slots_.size() - 1;
        for (auto at = spread(key) & mask; slots_[at].generation == generation_; at = (at + 1) & mask)
            if (slots_[at].key == key && same(slots_[at].value))
                return &slots_[at].value;
        return nullptr;
    }
    std::uint32_t *find(std::uint64_t key) noexcept
    {
        return find(key, [](std::uint32_t) { return true; });
    }
    // The caller has established that no slot of this key has an equal value.
    void insert(std::uint64_t key, std::uint32_t value)
    {
        reserve(size_ + 1);
        place(key, value);
    }
};
// 64-bit hash of a word sequence (FlatIndex key of a longer value).
template <std::size_t N> std::uint64_t words_hash(const std::array<std::uint32_t, N> &words) noexcept
{
    std::uint64_t h = 0xCBF29CE484222325ull;
    for (const auto word : words)
        h = (h ^ word) * 0x100000001B3ull;
    return h;
}
std::uint64_t sampler_pair_hash(const SamplerPairKey &key) noexcept
{
    return words_hash(key[0]) * 31u + words_hash(key[1]);
}
// NEW HOST (R30 perf_renderer) CPU-only (non-shader-visible) descriptor slots.
// A cached view is written once and copied into each frame's shader-visible
// arena range with CopyDescriptors; a descriptor copy is the identical view.
class CpuDescriptorPool
{
    static constexpr std::uint32_t chunk = 256;
    ID3D12Device *device_{};
    D3D12_DESCRIPTOR_HEAP_TYPE type_{};
    UINT step_{};
    std::vector<ComPtr<ID3D12DescriptorHeap>> heaps_;
    std::vector<SIZE_T> starts_;
    std::vector<std::uint32_t> free_;
    std::uint32_t next_{};

  public:
    static constexpr std::uint32_t none = 0xFFFFFFFFu;
    void init(ID3D12Device &device, D3D12_DESCRIPTOR_HEAP_TYPE type)
    {
        device_ = &device;
        type_ = type;
        step_ = device.GetDescriptorHandleIncrementSize(type);
    }
    std::uint32_t allocate()
    {
        if (!free_.empty())
        {
            const auto slot = free_.back();
            free_.pop_back();
            return slot;
        }
        if (next_ == heaps_.size() * chunk)
        {
            require(heaps_.size() < 4096, "polygon descriptor cache exhausted");
            D3D12_DESCRIPTOR_HEAP_DESC desc{};
            desc.Type = type_;
            desc.NumDescriptors = chunk;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            ComPtr<ID3D12DescriptorHeap> heap;
            check(device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)), "polygon descriptor cache allocation");
            // Capacity for every slot of the pool, so release() never allocates.
            free_.reserve((heaps_.size() + 1) * chunk);
            starts_.reserve(heaps_.size() + 1);
            heaps_.reserve(heaps_.size() + 1);
            starts_.push_back(heap->GetCPUDescriptorHandleForHeapStart().ptr);
            heaps_.push_back(std::move(heap));
        }
        return next_++;
    }
    void release(std::uint32_t slot) noexcept
    {
        free_.push_back(slot); // capacity reserved by allocate()
    }
    void release_all() noexcept
    {
        free_.clear();
        next_ = 0;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE handle(std::uint32_t slot) const noexcept
    {
        return {starts_[slot / chunk] + std::size_t(slot % chunk) * step_};
    }
};
// NEW HOST (R30 perf_renderer) facts of one texture resource that never change
// during its lifetime (description, owning device) and its cached CPU SRV. The
// record owns a reference, so its key (the address) can never name another object.
struct ResourceRecord
{
    ComPtr<ID3D12Resource> resource;
    D3D12_RESOURCE_DESC desc{};
    HRESULT device_lookup{};
    bool same_device{};
    std::uint32_t srv{CpuDescriptorPool::none};
    std::uint64_t last_record{};
};
// NEW HOST (R30 perf_renderer) PSO lookup memo of one pipeline key64: the PSO
// (never replaced in the cache), the last recording that retained it, and the
// (coverage_capable, smooth_capable) combinations already remembered.
struct PipelineMemo
{
    ID3D12PipelineState *pso{};
    std::uint64_t retained_record{};
    std::uint8_t remembered{};
};
// Records unused for this many record() calls release their reference even if
// another owner keeps the resource alive (the pool sweep runs every 64 calls).
constexpr std::uint64_t resource_idle_records = 1024;
struct PendingInsertion
{
    std::size_t draw_index;
    const PolygonGpuInsertion *insertion;
    PolygonDeviceState state;
    std::optional<PlannedDraw> plan; // planned insertion: same fixed-function plan as a draw
};
// NEW HOST (R30): the edge fix, 8-bit expansion and mip chains moved to
// host_texture_processing.cpp (unchanged arithmetic, banded and cached there).
} // namespace
// NEW HOST (R19) transient arena (perf_gpu): pages are persistently mapped UPLOAD
// buffers; heaps are shader visible. Nothing is released before destruction,
// so a steady-state frame performs no D3D12 object creation at all.
struct PolygonTransientArena::Impl
{
    ComPtr<ID3D12Device> device;
    std::size_t page_bytes;
    struct Page
    {
        ComPtr<ID3D12Resource> buffer;
        std::byte *cpu{};
        D3D12_GPU_VIRTUAL_ADDRESS gpu{};
        std::size_t size{}, used{};
    };
    std::vector<Page> pages;
    std::size_t current{};
    struct Heap
    {
        ComPtr<ID3D12DescriptorHeap> heap;
        std::size_t capacity{}, used{}, limit{}, initial{};
        UINT step{};
        D3D12_DESCRIPTOR_HEAP_TYPE type{};
    };
    Heap srv, sampler;
    std::vector<ComPtr<ID3D12DescriptorHeap>> replaced; // still bound by already recorded lists
    Impl(ID3D12Device &d, std::size_t page) : device(&d), page_bytes(std::max<std::size_t>(page, 64u * 1024u))
    {
        srv.type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srv.limit = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1;
        srv.initial = 4096;
        sampler.type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        sampler.limit = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;
        sampler.initial = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;
        srv.step = device->GetDescriptorHandleIncrementSize(srv.type);
        sampler.step = device->GetDescriptorHandleIncrementSize(sampler.type);
    }
    PolygonGpuUpload allocate(std::size_t bytes, std::size_t alignment)
    {
        bytes = std::max<std::size_t>(bytes, 4);
        for (; current < pages.size(); ++current)
        {
            auto &page = pages[current];
            const auto offset = align_up(page.used, alignment);
            if (offset <= page.size && bytes <= page.size - offset)
            {
                page.used = offset + bytes;
                return {page.cpu + offset, page.gpu + offset, page.buffer.Get()};
            }
        }
        Page page;
        page.size = std::max(page_bytes, align_up(bytes, 64u * 1024u));
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        heap.CreationNodeMask = heap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC buffer{};
        buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width = page.size;
        buffer.Height = buffer.DepthOrArraySize = buffer.MipLevels = 1;
        buffer.SampleDesc.Count = 1;
        buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_GENERIC_READ,
                                              nullptr, IID_PPV_ARGS(&page.buffer)),
              "polygon frame upload allocation");
        void *mapped{};
        D3D12_RANGE no_read{};
        check(page.buffer->Map(0, &no_read, &mapped), "polygon frame upload map");
        // Persistently mapped (legal for UPLOAD heaps); a fresh page starts 64 KiB aligned.
        page.cpu = static_cast<std::byte *>(mapped);
        page.gpu = page.buffer->GetGPUVirtualAddress();
        page.used = bytes;
        pages.push_back(std::move(page));
        current = pages.size() - 1;
        return {pages.back().cpu, pages.back().gpu, pages.back().buffer.Get()};
    }
    PolygonGpuDescriptors descriptors(Heap &h, std::size_t count)
    {
        require(count <= h.limit, "polygon frame descriptor range exceeds heap limit");
        if (!h.heap || h.capacity - h.used < count)
        {
            const auto capacity = std::min(h.limit, std::max({count, h.capacity * 2, h.initial}));
            D3D12_DESCRIPTOR_HEAP_DESC desc{};
            desc.Type = h.type;
            desc.NumDescriptors = static_cast<UINT>(capacity);
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            ComPtr<ID3D12DescriptorHeap> next;
            check(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&next)), "polygon frame descriptor allocation");
            if (h.heap)
                replaced.push_back(std::move(h.heap));
            h.heap = std::move(next);
            h.capacity = capacity;
            h.used = 0;
        }
        auto cpu = h.heap->GetCPUDescriptorHandleForHeapStart();
        auto gpu = h.heap->GetGPUDescriptorHandleForHeapStart();
        cpu.ptr += h.used * h.step;
        gpu.ptr += static_cast<UINT64>(h.used) * h.step;
        h.used += count;
        return {h.heap.Get(), cpu, gpu, h.step};
    }
};
PolygonTransientArena::PolygonTransientArena(ID3D12Device &device, std::size_t page_bytes)
    : impl_(std::make_unique<Impl>(device, page_bytes))
{
}
PolygonTransientArena::~PolygonTransientArena() = default;
void PolygonTransientArena::reset() noexcept
{
    for (auto &page : impl_->pages)
        page.used = 0;
    impl_->current = 0;
    impl_->srv.used = impl_->sampler.used = 0;
    impl_->replaced.clear();
}
PolygonGpuUpload PolygonTransientArena::allocate(std::size_t bytes, std::size_t alignment)
{
    return impl_->allocate(bytes, alignment);
}
PolygonGpuDescriptors PolygonTransientArena::srvs(std::size_t count)
{
    return impl_->descriptors(impl_->srv, count);
}
PolygonGpuDescriptors PolygonTransientArena::samplers(std::size_t count)
{
    return impl_->descriptors(impl_->sampler, count);
}
ComPtr<ID3D12Resource> PolygonTransientArena::page(const PolygonGpuUpload &upload) const
{
    return ComPtr<ID3D12Resource>(upload.resource);
}
ComPtr<ID3D12DescriptorHeap> PolygonTransientArena::heap(const PolygonGpuDescriptors &range) const
{
    return ComPtr<ID3D12DescriptorHeap>(range.heap);
}
namespace
{
// NEW HOST (R28b): everything a PSO description reads, as a value that a
// background prewarm job can own (the render thread keeps changing Impl).
struct PipelineInputs
{
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3DBlob> vs, precise_vs, ps, coverage_ps, smooth_ps, coverage_smooth_ps;
    ComPtr<ID3DBlob> reproject_vs; // NEW HOST (R33)
    DXGI_FORMAT color_format{}, depth_format{};
    DXGI_SAMPLE_DESC samples{};
};
// Complete PSO description of one key (also handed to planned insertions).
void describe_pipeline(const PipelineInputs &in, const PipelineKey &key, D3D12_GRAPHICS_PIPELINE_STATE_DESC &d)
{
    d = {};
    d.pRootSignature = in.root.Get();
    const auto &vertex = key.reproject ? in.reproject_vs : key.precise_vs ? in.precise_vs : in.vs;
    require(vertex != nullptr, "polygon precise/re-projection VS was not enabled");
    const auto &pixel = key.smooth_magnification ? (key.alpha_to_coverage ? in.coverage_smooth_ps : in.smooth_ps)
                        : key.alpha_to_coverage  ? in.coverage_ps
                                                 : in.ps;
    require(pixel != nullptr, "polygon alpha-to-coverage/smooth magnification PS was not enabled");
    d.VS = {vertex->GetBufferPointer(), vertex->GetBufferSize()};
    d.PS = {pixel->GetBufferPointer(), pixel->GetBufferSize()};
    d.BlendState.AlphaToCoverageEnable = key.alpha_to_coverage ? TRUE : FALSE; // NEW HOST (R23)
    d.InputLayout = key.reproject ? D3D12_INPUT_LAYOUT_DESC{reprojection_input, 5}
                                  : D3D12_INPUT_LAYOUT_DESC{polygon_input, 4}; // NEW HOST (R33)
    d.SampleMask = UINT_MAX;
    d.PrimitiveTopologyType = static_cast<D3D12_PRIMITIVE_TOPOLOGY_TYPE>(key.topology);
    d.NumRenderTargets = 1;
    d.RTVFormats[0] = in.color_format;
    d.DSVFormat = in.depth_format;
    d.SampleDesc = in.samples;
    d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    d.RasterizerState.CullMode = key.cull == 1 ? D3D12_CULL_MODE_NONE : D3D12_CULL_MODE_BACK;
    d.RasterizerState.FrontCounterClockwise = key.cull == 2;
    d.RasterizerState.DepthClipEnable = TRUE;
    d.RasterizerState.MultisampleEnable = in.samples.Count > 1;
    d.DepthStencilState.DepthEnable = static_cast<BOOL>(key.z);
    d.DepthStencilState.DepthWriteMask = key.zwrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    d.DepthStencilState.DepthFunc = static_cast<D3D12_COMPARISON_FUNC>(key.zfunc);
    auto &blend_state = d.BlendState.RenderTarget[0];
    blend_state.BlendEnable = static_cast<BOOL>(key.blend_enabled);
    blend_state.SrcBlend = static_cast<D3D12_BLEND>(key.source);
    blend_state.DestBlend = static_cast<D3D12_BLEND>(key.destination);
    blend_state.BlendOp = D3D12_BLEND_OP_ADD;
    const auto alpha_blend = [](std::uint32_t v) {
        return static_cast<D3D12_BLEND>(v == 3 ? 5 : v == 4 ? 6 : v == 9 ? 7 : v == 10 ? 8 : v);
    };
    blend_state.SrcBlendAlpha = alpha_blend(key.source);
    blend_state.DestBlendAlpha = alpha_blend(key.destination);
    blend_state.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend_state.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
}
// NEW HOST (R28b) a remembered pipeline state: its key without the variant
// bits, and whether the state could take each variant.
struct SeenPipeline
{
    PipelineKey base;
    bool coverage_capable{}, smooth_capable{};
    auto operator<=>(const SeenPipeline &) const = default;
};
std::uint64_t encode_seen(const SeenPipeline &seen)
{
    return pipeline_key64(seen.base, 0) | std::uint64_t(seen.coverage_capable) << 40 |
           std::uint64_t(seen.smooth_capable) << 41;
}
std::optional<SeenPipeline> decode_seen(std::uint64_t code)
{
    const auto bits = [code](unsigned shift, unsigned width) {
        return static_cast<std::uint32_t>((code >> shift) & ((1ull << width) - 1));
    };
    SeenPipeline seen;
    seen.base = {bits(0, 1),  bits(1, 1),  bits(2, 4),  bits(6, 1), bits(7, 4),
                 bits(11, 4), bits(15, 2), bits(17, 2), bits(19, 1)};
    seen.coverage_capable = bits(40, 1) != 0;
    seen.smooth_capable = bits(41, 1) != 0;
    const auto &k = seen.base;
    const bool valid = (code & ~((1ull << 20) - 1) & ~(3ull << 40)) == 0 && k.zfunc >= 1 && k.zfunc <= 8 &&
                       k.source >= 1 && k.source <= 10 && k.destination >= 1 && k.destination <= 10 && k.cull >= 1 &&
                       k.cull <= 3 && (k.topology == 2 || k.topology == 3);
    return valid ? std::optional<SeenPipeline>(seen) : std::nullopt;
}
PipelineKey variant_key(const SeenPipeline &seen, DXGI_SAMPLE_DESC samples, const PolygonRasterQuality &quality,
                        bool smooth)
{
    auto key = seen.base;
    key.alpha_to_coverage = quality.alpha_to_coverage && samples.Count > 1 && seen.coverage_capable ? 1u : 0u;
    key.smooth_magnification = smooth ? 1u : 0u;
    return key;
}
struct PrewarmTarget
{
    std::uint32_t samples{};
    bool coverage{}, smooth{};
    bool operator==(const PrewarmTarget &) const = default;
};
PrewarmTarget prewarm_target(DXGI_SAMPLE_DESC samples, const PolygonRasterQuality &quality)
{
    return {samples.Count, quality.alpha_to_coverage && samples.Count > 1, quality.smooth_magnification};
}
struct PrewarmResult
{
    PrewarmTarget target;
    ComPtr<ID3DBlob> precise_vs, coverage_ps, smooth_ps, coverage_smooth_ps;
    std::vector<std::pair<PipelineKey, ComPtr<ID3D12PipelineState>>> pipelines;
};
// NEW HOST (R33) recorder refactor: the planned frame is uploaded and emitted by
// the functions below, shared by record() (per call, arena upload) and the kept
// GPU re-projection plans (polygon_reprojection.hpp). The arithmetic, validation,
// state filtering, batching and command order are the pre-R33 record() code,
// moved; with no re-projection the emitted command stream is identical.
struct FrameView
{
    D3D12_VIEWPORT viewport;
    D3D12_RECT scissor;
};
FrameView frame_view(std::uint32_t width, std::uint32_t height, const PolygonLogicalRasterExtent &logical,
                     bool wide_canvas, const host_wide::Layout &wide_layout)
{
    // Keep the shader's existing +0.5 in logical coordinates exactly once.
    // D3D12's physical viewport performs scaling; RHW/UV/depth are untouched.
    const double scale = std::min(double(width) / logical.width, double(height) / logical.height);
    const float fitted_width = static_cast<float>(logical.width * scale),
                fitted_height = static_cast<float>(logical.height * scale);
    const D3D12_VIEWPORT viewport =
        wide_canvas ?
                    // NEW HOST (R20): whole canvas -> whole target (origin 0 unless the canvas clamp left bars).
            D3D12_VIEWPORT{wide_layout.output_origin_x,
                           wide_layout.output_origin_y,
                           static_cast<float>(double(wide_layout.canvas_width) * wide_layout.pixels_per_unit),
                           static_cast<float>(double(wide_layout.canvas_height) * wide_layout.pixels_per_unit),
                           0,
                           1}
                    : D3D12_VIEWPORT{(static_cast<float>(width) - fitted_width) * 0.5f,
                                     (static_cast<float>(height) - fitted_height) * 0.5f,
                                     fitted_width,
                                     fitted_height,
                                     0,
                                     1};
    return {viewport, D3D12_RECT{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)}};
}
// Vertex payload (with the R20 canvas offset) and absolute R32 indices of the planned draws.
void write_polygon_geometry(const std::vector<PlannedDraw> &draws, std::byte *vertex_out, std::uint32_t *index_out,
                            float canvas_offset_x, float canvas_offset_y, PolygonGpuRecording &result)
{
    const bool offset_canvas = canvas_offset_x != 0.0f || canvas_offset_y != 0.0f;
    for (const auto &draw : draws)
    {
        const auto &c = *draw.source;
        const auto n = c.vertices.size() * 40;
        if (n && !offset_canvas)
            std::memcpy(vertex_out + draw.first_vertex * 40, c.vertices.data(), n);
        else if (n)
        {
            // NEW HOST (R20) canvas: offset the uploaded copy of X/Y (FVF 2C4
            //  words 0/1, 40-byte stride) into canvas coordinates.
            // NEW HOST (R30 perf_renderer): the offset is applied to the captured
            // words on their way into the upload page (one sequential write per
            // vertex) instead of reading the words back from the write-combined
            // (uncached) page. Same binary32 additions of the same bits.
            auto *words = vertex_out + draw.first_vertex * 40;
            for (const auto &vertex : c.vertices)
            {
                auto moved = vertex.words;
                const float x = std::bit_cast<float>(moved[0]) + canvas_offset_x;
                const float y = std::bit_cast<float>(moved[1]) + canvas_offset_y;
                moved[0] = std::bit_cast<std::uint32_t>(x);
                moved[1] = std::bit_cast<std::uint32_t>(y);
                std::memcpy(words, moved.data(), 40);
                words += 40;
            }
        }
        result.vertex_bytes += n;
        result.index_bytes += draw.index_count * 2; // captured WORD payload / fan expansion, as before
        if (!draw.uses_indices)
            continue;
        // Absolute R32 indices into this recording's single vertex region.
        const auto base = static_cast<std::uint32_t>(draw.first_vertex);
        auto *out = index_out + draw.first_index;
        if (c.type == 6)
        {
            const auto used = c.indexed ? c.indices.size() : c.vertices.size();
            const auto at = [&](std::size_t i) -> std::uint32_t {
                return base + (c.indexed ? c.indices[i] : static_cast<std::uint32_t>(i));
            };
            const auto behind = [&](std::size_t i) { return !(std::bit_cast<float>(c.vertices[i].words[3]) > 0); };
            for (std::size_t i = 1; i + 1 < used; ++i)
            {
                if (draw.drop_nonpositive_rhw && (behind(0) || behind(i) || behind(i + 1)))
                { // PORT-FIX-012: zero-area, same index count
                    *out++ = at(0);
                    *out++ = at(0);
                    *out++ = at(0);
                    continue;
                }
                *out++ = at(0);
                *out++ = at(i);
                *out++ = at(i + 1);
            }
        }
        else
            for (const auto index : c.indices)
                *out++ = base + index;
    }
}
// NEW HOST (R33) per planned draw of a re-projected frame: its re-projection PSO
// (the same key with the re-projection VS), the moving bits of its slots, its
// largest slot and its b1 words 0..5 (kx, ky, centre + canvas offset, near w, valid).
struct ReprojectDraw
{
    ID3D12PipelineState *pso{};
    std::uint64_t mask{};
    std::uint32_t max_slot{};
    std::array<std::uint32_t, 6> constants{};
};
struct EmitInput
{
    const CapturedPolygonFrame &frame;
    const PolygonGpuTextures &textures;
    const std::vector<PlannedDraw> &draws;
    const std::vector<PendingInsertion> &insertions;
    const std::vector<TexturePair> &texture_pairs;
    const std::vector<std::array<D3D12_SAMPLER_DESC, 2>> &sampler_pairs;
    D3D12_GPU_VIRTUAL_ADDRESS vertex_gpu, index_gpu, slot_gpu;
    std::size_t vertex_bytes, index_bytes, slot_bytes;
    D3D12_VIEWPORT viewport;
    D3D12_RECT scissor;
    PolygonLogicalRasterExtent logical;
    float canvas_offset_x, canvas_offset_y;
    bool kept_plan; // texture records are resolved by resource (the call scratch belongs to other banks)
    const PolygonReprojection *reprojection;
    const std::vector<ReprojectDraw> *reprojected;
};
} // namespace
// NEW HOST (R33) one re-projected record call (see record_reprojected / record_reprojected_copy).
struct PolygonD3D12::Reproject
{
    const PolygonReprojection *reprojection{};
    std::size_t segment{};                               // segment mode: segment of reprojection->base
    std::span<const CapturedPolygonDraw *const> sources; // copy mode: the source draw of every draw
    bool copy{};
};
struct PolygonD3D12::Impl
{
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3DBlob> vs, ps, precise_vs;
    ComPtr<ID3DBlob> coverage_ps; // NEW HOST (R23) alpha-to-coverage variant, compiled on first enable
    ComPtr<ID3DBlob> smooth_ps, coverage_smooth_ps; // NEW HOST (R28) smooth magnification variants
    ComPtr<ID3DBlob> reproject_vs;                  // NEW HOST (R33) GPU re-projection VS
    bool precise{};
    PolygonRasterQuality quality; // NEW HOST (R23); default == exact pre-R23 path
    // (R30: the former single-entry edge/mip/8-bit caches are host_texture_processing's result cache.)
    DXGI_FORMAT color_format, depth_format;
    DXGI_SAMPLE_DESC samples;
    // NEW HOST (R28b): PSOs per sample count (an MSAA change keeps the others).
    std::map<std::pair<std::uint32_t, PipelineKey>, ComPtr<ID3D12PipelineState>> pipelines;
    std::set<SeenPipeline> seen; // every state the recorder used (and the persisted ones)
    struct PrewarmJob
    {
        PrewarmTarget target;
        std::future<PrewarmResult> result;
    };
    std::vector<PrewarmJob> jobs; // running or finished, not yet adopted
    // NEW HOST (R19) call scratch for record(); capacity is reused, contents never are.
    // NEW HOST (R30 perf_renderer): the maps are flat indexes (no per-insert node).
    std::vector<PlannedDraw> scratch_draws;
    std::vector<std::array<D3D12_SAMPLER_DESC, 2>> scratch_sampler_pairs;
    std::vector<SamplerPairKey> scratch_sampler_pair_keys;
    FlatIndex scratch_sampler_indices;
    FlatIndex scratch_tokens;                          // bank token -> entry index (| duplicate_token)
    std::vector<std::uint64_t> scratch_entry_verified; // record_serial at which an entry passed its checks
    std::vector<std::uint32_t> scratch_entry_resource; // its ResourceRecord index
    std::vector<TexturePair> scratch_texture_pairs;
    FlatIndex scratch_texture_indices;
    std::vector<PendingInsertion> scratch_insertions;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> scratch_sources;
    std::vector<UINT> scratch_ones;
    std::vector<ID3D12PipelineState *> scratch_retained_pipelines; // collected, then one exact reserve
    // NEW HOST (R30 perf_renderer) persistent caches (render thread only).
    std::uint64_t record_serial{};
    std::vector<ResourceRecord> resources;
    FlatIndex resource_index; // resource address -> resources index
    CpuDescriptorPool srv_cache, sampler_cache;
    std::uint32_t null_srv{CpuDescriptorPool::none};
    std::vector<SamplerKey> sampler_keys; // sampler_cache slot of each unique sampler description
    std::vector<std::uint32_t> sampler_slots;
    FlatIndex sampler_index;
    std::vector<PipelineMemo> pipeline_memo;
    FlatIndex pipeline_index; // pipeline_key64 -> pipeline_memo index
    static constexpr std::uint32_t duplicate_token = 0x80000000u;
    Impl(ID3D12Device &d, DXGI_FORMAT color, DXGI_FORMAT depth, DXGI_SAMPLE_DESC sample)
        : device(&d), color_format(color), depth_format(depth), samples(sample)
    {
        srv_cache.init(d, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        sampler_cache.init(d, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        require(color == DXGI_FORMAT_R8G8B8A8_UNORM || color == DXGI_FORMAT_B8G8R8A8_UNORM,
                "polygon target must preserve unorm color arithmetic");
        require(depth == DXGI_FORMAT_D32_FLOAT && sample.Count != 0,
                "polygon target requires a valid D32 depth buffer");
        D3D12_DESCRIPTOR_RANGE ranges[2]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, 0},
                                         {D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 2, 0, 0, 0}};
        D3D12_ROOT_PARAMETER params[5]{};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[0].Constants = {0, 0, 24};
        for (unsigned i = 0; i < 2; ++i)
        {
            params[i + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            params[i + 1].DescriptorTable = {1, &ranges[i]};
            params[i + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        }
        // NEW HOST (R33) re-projection VS inputs (unused by every other pipeline): b1 =
        // 20 constants (draw mapping, near plane, camera->world), t2 = the slot motions.
        params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[3].Constants = {1, 0, 20};
        params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[4].Descriptor = {2, 0};
        params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        D3D12_ROOT_SIGNATURE_DESC description{5, params, 0, nullptr,
                                              D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
        ComPtr<ID3DBlob> blob, errors;
        check(D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors),
              "polygon root serialization");
        check(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root)),
              "polygon root creation");
        vs = compile("vs", "vs_5_0");
        ps = compile("ps", "ps_5_0");
        reproject_vs = compile_reprojection_vertex_shader(); // NEW HOST (R33)
    }
    PipelineInputs inputs(DXGI_SAMPLE_DESC target) const
    {
        return {root,         vs,           precise_vs,   ps,    coverage_ps, smooth_ps, coverage_smooth_ps,
                reproject_vs, color_format, depth_format, target};
    }
    // Complete PSO description of one key (also handed to planned insertions).
    void describe(const PipelineKey &key, D3D12_GRAPHICS_PIPELINE_STATE_DESC &d) const
    {
        // The description's raw pointers stay valid: this Impl owns the same objects.
        describe_pipeline(inputs(samples), key, d);
    }
    ID3D12PipelineState *pipeline(PipelineKey key)
    {
        if (auto i = pipelines.find({samples.Count, key}); i != pipelines.end())
            return i->second.Get();
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d;
        describe(key, d);
        ComPtr<ID3D12PipelineState> result;
        check(device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&result)), "polygon pipeline creation");
        return pipelines.emplace(std::pair{samples.Count, key}, std::move(result)).first->second.Get();
    }
    void remember(const PlannedDraw &draw)
    {
        auto base = draw.pipeline;
        base.alpha_to_coverage = base.smooth_magnification = base.reproject = 0;
        seen.insert({base, draw.coverage_capable, draw.smooth_capable});
    }
    // NEW HOST (R30 perf_renderer) record of an actual texture resource: GetDesc and
    // GetDevice run once per resource lifetime instead of once per token per frame.
    std::uint32_t resource_record(ID3D12Resource &resource)
    {
        const auto key = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&resource));
        if (const auto *found = resource_index.find(key))
            return *found;
        ResourceRecord record;
        record.resource = &resource;
        record.desc = resource.GetDesc();
        ComPtr<ID3D12Device> owner;
        record.device_lookup = resource.GetDevice(IID_PPV_ARGS(&owner));
        record.same_device = SUCCEEDED(record.device_lookup) && owner.Get() == device.Get();
        const auto index = static_cast<std::uint32_t>(resources.size());
        resources.push_back(std::move(record));
        resource_index.insert(key, index);
        return index;
    }
    // Releases the references of records that only this cache still owns (a
    // retired texture) or that were idle for resource_idle_records calls. Every
    // recording retains its own textures, so this never frees a resource that a
    // queued list uses. Only at the start of record(), between recordings.
    void sweep_resources()
    {
        std::size_t kept = 0;
        for (std::size_t i = 0; i < resources.size(); ++i)
        {
            auto &record = resources[i];
            bool evict = record.last_record + resource_idle_records < record_serial;
            if (!evict)
            {
                record.resource->AddRef();
                evict = record.resource->Release() == 1; // the count after Release: ours alone
            }
            if (evict)
            {
                if (record.srv != CpuDescriptorPool::none)
                    srv_cache.release(record.srv);
                record.resource.Reset();
                continue;
            }
            if (kept != i)
                resources[kept] = std::move(record);
            ++kept;
        }
        resources.erase(resources.begin() + static_cast<std::ptrdiff_t>(kept), resources.end());
        resource_index.clear(); // the capacity already holds every remaining record: no allocation
        for (std::uint32_t i = 0; i < resources.size(); ++i)
        {
            const auto key = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(resources[i].resource.Get()));
            resource_index.insert(key, i);
        }
    }
    // CPU SRV of a checked texture (format == its resource format), written on first use.
    D3D12_CPU_DESCRIPTOR_HANDLE texture_srv(std::uint32_t record_index, DXGI_FORMAT format)
    {
        auto &record = resources[record_index];
        if (record.srv == CpuDescriptorPool::none)
        {
            const auto slot = srv_cache.allocate();
            D3D12_SHADER_RESOURCE_VIEW_DESC d{};
            d.Format = format;
            d.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            d.Texture2D.MipLevels = record.desc.MipLevels;
            device->CreateShaderResourceView(record.resource.Get(), &d, srv_cache.handle(slot));
            record.srv = slot;
        }
        return srv_cache.handle(record.srv);
    }
    // The inactive-stage view of the pre-R30 recorder (null resource, RGBA8, one level).
    D3D12_CPU_DESCRIPTOR_HANDLE empty_srv()
    {
        if (null_srv == CpuDescriptorPool::none)
        {
            const auto slot = srv_cache.allocate();
            D3D12_SHADER_RESOURCE_VIEW_DESC d{};
            d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            d.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            d.Texture2D.MipLevels = 1;
            device->CreateShaderResourceView(nullptr, &d, srv_cache.handle(slot));
            null_srv = slot;
        }
        return srv_cache.handle(null_srv);
    }
    // Bound for a pathological stream: before a frame that could push the cache past
    // 8192 samplers, forget every cached one (no handle of an earlier frame is kept:
    // descriptor copies are immediate). Never called while a frame collects handles.
    void bound_sampler_cache(std::size_t frame_samplers) noexcept
    {
        if (sampler_keys.size() + frame_samplers <= 8192)
            return;
        sampler_keys.clear();
        sampler_slots.clear();
        sampler_index.clear();
        sampler_cache.release_all();
    }
    // CPU sampler of one exact description (keyed by all of its bits), written on first use.
    D3D12_CPU_DESCRIPTOR_HANDLE sampler_descriptor(const D3D12_SAMPLER_DESC &description)
    {
        const auto key = sampler_key(description);
        const auto hash = words_hash(key);
        if (const auto *found = sampler_index.find(hash, [&](std::uint32_t i) { return sampler_keys[i] == key; }))
            return sampler_cache.handle(sampler_slots[*found]);
        const auto slot = sampler_cache.allocate();
        device->CreateSampler(&description, sampler_cache.handle(slot));
        sampler_keys.push_back(key);
        sampler_slots.push_back(slot);
        sampler_index.insert(hash, static_cast<std::uint32_t>(sampler_keys.size() - 1));
        return sampler_cache.handle(slot);
    }
    // Adopts every finished job (its blobs and PSOs). Throws a job's error.
    void adopt_finished_jobs()
    {
        for (auto job = jobs.begin(); job != jobs.end();)
        {
            if (job->result.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            {
                ++job;
                continue;
            }
            auto result = job->result.get();
            job = jobs.erase(job);
            if (!precise_vs)
                precise_vs = result.precise_vs;
            if (!coverage_ps)
                coverage_ps = result.coverage_ps;
            if (!smooth_ps)
                smooth_ps = result.smooth_ps;
            if (!coverage_smooth_ps)
                coverage_smooth_ps = result.coverage_smooth_ps;
            for (auto &[key, pso] : result.pipelines)
                pipelines.emplace(std::pair{result.target.samples, key}, std::move(pso));
        }
    }
    // What a target still lacks: PS variants, the precise VS and the PSOs of
    // every remembered state (with the variant bits this target gives it).
    struct Needs
    {
        PipelineInputs inputs;
        std::vector<PipelineKey> keys;
        bool precise{}, coverage{}, smooth{}, coverage_smooth{};
        bool any() const noexcept
        {
            return !keys.empty() || precise || coverage || smooth || coverage_smooth;
        }
    };
    Needs needs(DXGI_SAMPLE_DESC target_samples, const PolygonRasterQuality &target_quality) const
    {
        const auto target = prewarm_target(target_samples, target_quality);
        Needs result{inputs(target_samples),
                     {},
                     false,
                     target.coverage && !coverage_ps,
                     target.smooth && !smooth_ps,
                     target.coverage && target.smooth && !coverage_smooth_ps};
        for (const auto &state : seen)
            for (const bool smooth : {false, true})
            {
                if (smooth && !(target.smooth && state.smooth_capable))
                    continue;
                const auto key = variant_key(state, target_samples, target_quality, smooth);
                // NEW HOST (R33): every state also gets its GPU re-projection variant.
                auto moving = key;
                moving.reproject = 1;
                if (!pipelines.contains({target_samples.Count, moving}))
                    result.keys.push_back(moving);
                if (pipelines.contains({target_samples.Count, key}))
                    continue;
                result.keys.push_back(key);
                result.precise = result.precise || (key.precise_vs != 0 && !precise_vs);
            }
        return result;
    }
    // NEW HOST (R33) GPU re-projection: call scratch of the copy mode, kept base
    // plans and the pool of their persistent upload buffers (render thread only).
    std::vector<ReprojectDraw> scratch_reproject;
    std::vector<ID3D12PipelineState *> scratch_reproject_pipelines;
    struct ReprojectionEntry
    {
        std::weak_ptr<const PolygonReprojectionBase> base;
        std::size_t segment{};
        const CapturedPolygonFrame *frame{};
        std::shared_ptr<const PolygonGpuTextures> textures;
        std::uint32_t width{}, height{};
        std::optional<PolygonLogicalRasterExtent> canvas;
        bool original_4x3{}, precise{};
        DXGI_SAMPLE_DESC samples{};
        PolygonRasterQuality quality;
        std::vector<PlannedDraw> draws;
        std::vector<PendingInsertion> insertions;
        std::vector<TexturePair> texture_pairs;
        std::vector<std::array<D3D12_SAMPLER_DESC, 2>> sampler_pairs;
        std::vector<ReprojectDraw> reprojected;
        std::vector<ComPtr<ID3D12PipelineState>> pipelines;
        PolygonDeviceState final_state;
        std::uint64_t source_tick{}, resource_epoch{};
        std::size_t stat_vertex_bytes{}, stat_index_bytes{};
        ComPtr<ID3D12Resource> buffer;
        D3D12_GPU_VIRTUAL_ADDRESS vertex_gpu{}, index_gpu{}, slot_gpu{};
        std::size_t vertex_bytes{}, index_bytes{}, slot_bytes{};
        FrameView view{};
        PolygonLogicalRasterExtent logical{};
        float canvas_offset_x{}, canvas_offset_y{};
    };
    std::vector<ReprojectionEntry> reprojection_entries; // at most reprojection_entry_limit
    static constexpr std::size_t reprojection_entry_limit = 8;
    struct PooledBuffer
    {
        ComPtr<ID3D12Resource> resource;
        std::byte *cpu{};
        std::size_t bytes{};
    };
    std::vector<PooledBuffer> reprojection_buffers;
    // A persistently mapped UPLOAD buffer that no recording or kept plan references
    // any more (the pool holds the only reference), or a new one.
    PooledBuffer take_buffer(std::size_t bytes)
    {
        for (auto &buffer : reprojection_buffers)
            if (buffer.bytes >= bytes)
            {
                buffer.resource->AddRef();
                if (buffer.resource->Release() == 1)
                    return buffer;
            }
        if (reprojection_buffers.size() >= 2 * reprojection_entry_limit)
        {
            // Drop unused buffers first (a bounded pool; used ones stay alive through their owners).
            std::erase_if(reprojection_buffers, [](PooledBuffer &buffer) {
                buffer.resource->AddRef();
                return buffer.resource->Release() == 1;
            });
        }
        PooledBuffer created;
        created.bytes = align_up(bytes, std::size_t{1} << 20);
        const D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_UPLOAD};
        D3D12_RESOURCE_DESC d{};
        d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        d.Width = created.bytes;
        d.Height = d.DepthOrArraySize = d.MipLevels = 1;
        d.SampleDesc.Count = 1;
        d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_GENERIC_READ,
                                              nullptr, IID_PPV_ARGS(&created.resource)),
              "polygon re-projection buffer");
        void *mapped = nullptr;
        const D3D12_RANGE none{};
        check(created.resource->Map(0, &none, &mapped), "polygon re-projection buffer map");
        created.cpu = static_cast<std::byte *>(mapped);
        reprojection_buffers.push_back(created);
        return created;
    }
    // Slot stream and per-draw re-projection data of `draws` (segment mode: the
    // base segment's draws in order; copy mode: `sources`). A draw without base
    // information keeps passthrough slots (drawn as captured).
    void prepare_reproject_draws(const PolygonD3D12::Reproject &rp, const std::vector<PlannedDraw> &draws,
                                 float canvas_offset_x, float canvas_offset_y, std::uint32_t *slot_out,
                                 std::vector<ReprojectDraw> &out, std::vector<ID3D12PipelineState *> &pipelines_out);
    ReprojectionEntry *find_reprojection(const PolygonD3D12::Reproject &rp, const CapturedPolygonFrame &frame,
                                         const std::shared_ptr<const PolygonGpuTextures> &textures, std::uint32_t width,
                                         std::uint32_t height, const std::optional<PolygonLogicalRasterExtent> &canvas)
    {
        std::erase_if(reprojection_entries, [](const ReprojectionEntry &entry) { return entry.base.expired(); });
        for (auto &entry : reprojection_entries)
        {
            if (entry.base.lock() != rp.reprojection->base || entry.segment != rp.segment)
                continue;
            if (entry.frame == &frame && entry.textures == textures && entry.width == width && entry.height == height &&
                entry.canvas == canvas && entry.original_4x3 == host_wide::original_4x3() && entry.precise == precise &&
                entry.samples.Count == samples.Count && entry.samples.Quality == samples.Quality &&
                entry.quality == quality)
                return &entry;
            entry.base.reset(); // another target configuration: rebuilt below
        }
        return nullptr;
    }
    ReprojectionEntry &store_reprojection(const PolygonD3D12::Reproject &rp, const CapturedPolygonFrame &frame,
                                          const std::shared_ptr<const PolygonGpuTextures> &textures,
                                          std::uint32_t width, std::uint32_t height,
                                          const std::optional<PolygonLogicalRasterExtent> &canvas,
                                          const FrameView &view, const PolygonLogicalRasterExtent &logical,
                                          float canvas_offset_x, float canvas_offset_y, std::size_t vertices_total,
                                          std::size_t indices_total, const PolygonDeviceState &final_state);
    PolygonGpuRecording emit_reprojection_entry(ID3D12GraphicsCommandList &list, PolygonTransientArena &arena,
                                                const ReprojectionEntry &entry, const PolygonReprojection &reprojection,
                                                std::uint64_t serial);
    void emit(ID3D12GraphicsCommandList &list, PolygonTransientArena &arena, const EmitInput &in,
              PolygonGpuRecording &result);
};
PolygonD3D12::PolygonD3D12(ID3D12Device &d, DXGI_FORMAT c, DXGI_FORMAT z, DXGI_SAMPLE_DESC s)
    : impl_(std::make_unique<Impl>(d, c, z, s))
{
}
PolygonD3D12::~PolygonD3D12() = default;
PolygonTextureUpload PolygonD3D12::upload_a1r5g5b5(ID3D12GraphicsCommandList &list, std::uint32_t token,
                                                   std::uint32_t width, std::uint32_t height,
                                                   std::span<const std::uint16_t> pixels,
                                                   const PolygonTextureProfile &profile,
                                                   PolygonTransientArena *staging_arena)
{
    require(token != 0 && width > 0 && height > 0 && width <= 16384 && height <= 16384 &&
                pixels.size() == std::size_t(width) * height,
            "polygon A1R5G5B5 upload dimensions/token");
    // NEW HOST (R28b): every conversion step below follows `profile` (the
    // texture identity's recorded chain), never the current quality.
    auto recovery = std::make_shared<PolygonTexturePixels>();
    recovery->width = width;
    recovery->height = height;
    recovery->words.assign(pixels.begin(), pixels.end()); // Allocate before recording any GPU commands.
    ComPtr<ID3D12Device> command_device;
    check(list.GetDevice(IID_PPV_ARGS(&command_device)), "polygon upload command device");
    require(command_device.Get() == impl_->device.Get(), "polygon upload command list belongs to another device");
    auto &device = *impl_->device.Get();
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support{DXGI_FORMAT_B5G5R5A1_UNORM};
    check(device.CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)),
          "polygon A1R5G5B5 support query");
    constexpr auto required = D3D12_FORMAT_SUPPORT1_TEXTURE2D | D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE;
    require((support.Support1 & required) == required, "device cannot sample native A1R5G5B5 textures");
    // NEW HOST (R28): a high-precision profile uploads B8G8R8A8 (exact 5->8 bit
    // replication, smooth-gradient deband, 8-bit mips); otherwise the 16-bit path
    // below is unchanged. R28b: the profile decides, and the device must sample
    // B8G8R8A8 (texture_profile() only records it where it can).
    D3D12_FEATURE_DATA_FORMAT_SUPPORT wide_support{DXGI_FORMAT_B8G8R8A8_UNORM};
    const bool wide = profile.high_precision;
    if (wide)
    {
        check(device.CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &wide_support, sizeof(wide_support)),
              "polygon B8G8R8A8 support query");
        require((wide_support.Support1 & required) == required, "device cannot sample B8G8R8A8 textures");
    }
    // NEW HOST (R23): optional full mip chain, built on the CPU before any GPU
    // command is recorded. Without MIP format support the upload stays single-level.
    const auto mip_support = wide ? wide_support.Support1 : support.Support1;
    // NEW HOST (R32 perf-swap): the request is request_for(shape, extent), the same function the
    // replacement prewarm uses: mips when the profile asks for them, the format supports them and the
    // extent exceeds 1x1; levels = bit_width(max(width, height)), as before.
    const host_texture_processing::Shape shape{profile.edge_fix, wide,
                                               profile.mipmaps && (mip_support & D3D12_FORMAT_SUPPORT1_MIP) != 0};
    const auto request = host_texture_processing::request_for(shape, width, height);
    const auto levels = request.levels;
    // NEW HOST (R27 edge fix, R28 8-bit path, R23 mips): the CPU conversion chain
    // (R30: host_texture_processing, banded on the host pool, cached by exact input).
    // The recovery words above stay literal; a rebind re-derives the same levels.
    const auto prepared = host_texture_processing::prepare(width, height, pixels, request);
    if (prepared && !wide && !prepared->edge_fixed.empty())
        pixels = prepared->edge_fixed;
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    description.Width = width;
    description.Height = height;
    description.DepthOrArraySize = 1;
    description.MipLevels = static_cast<UINT16>(levels);
    description.Format = wide ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_B5G5R5A1_UNORM;
    description.SampleDesc.Count = 1;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap.CreationNodeMask = heap.VisibleNodeMask = 1;
    PolygonTextureUpload result;
    result.texture.token = token;
    result.texture.format = description.Format;
    result.texture.recovery = std::move(recovery);
    result.texture.profile = profile; // NEW HOST (R28b)
    check(device.CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_COPY_DEST,
                                         nullptr, IID_PPV_ARGS(&result.texture.resource)),
          "polygon A1R5G5B5 texture allocation");
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(levels);
    UINT64 bytes{};
    device.GetCopyableFootprints(&description, 0, levels, 0, footprints.data(), nullptr, nullptr, &bytes);
    void *mapped{};
    UINT64 staging_offset = 0; // NEW HOST (R32): offset of the footprints inside the staging resource
    if (staging_arena)
    {
        // NEW HOST (R32 perf-swap): a range of the caller's persistently mapped upload arena instead of
        // a new committed buffer (whose creation and zero fill cost a large part of an in-race swap).
        const auto region =
            staging_arena->allocate(static_cast<std::size_t>(bytes), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        result.staging = staging_arena->page(region);
        staging_offset = region.gpu - region.resource->GetGPUVirtualAddress();
        mapped = region.cpu;
    }
    else
    {
        D3D12_RESOURCE_DESC buffer{};
        buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width = bytes;
        buffer.Height = buffer.DepthOrArraySize = buffer.MipLevels = 1;
        buffer.SampleDesc.Count = 1;
        buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        check(device.CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_GENERIC_READ,
                                             nullptr, IID_PPV_ARGS(&result.staging)),
              "polygon A1R5G5B5 staging allocation");
        D3D12_RANGE no_read{};
        check(result.staging->Map(0, &no_read, &mapped), "polygon A1R5G5B5 staging map");
    }
    const std::size_t texel_bytes = wide ? 4u : 2u; // NEW HOST (R28)
    for (std::uint32_t level = 0; level < levels; ++level)
    {
        const auto &footprint = footprints[level];
        const std::size_t level_width = footprint.Footprint.Width, level_height = footprint.Footprint.Height;
        const auto *level_bytes =
            wide ? reinterpret_cast<const std::byte *>(prepared->wide[level].data())
                 : reinterpret_cast<const std::byte *>(level == 0 ? pixels.data() : prepared->mips[level - 1].data());
        for (std::size_t y = 0; y < level_height; ++y)
            std::memcpy(static_cast<std::byte *>(mapped) + footprint.Offset + y * footprint.Footprint.RowPitch,
                        level_bytes + y * level_width * texel_bytes, level_width * texel_bytes);
    }
    if (!staging_arena)
        result.staging->Unmap(0, nullptr); // an arena page stays persistently mapped
    for (std::uint32_t level = 0; level < levels; ++level)
    {
        D3D12_TEXTURE_COPY_LOCATION source{}, destination{};
        source.pResource = result.staging.Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source.PlacedFootprint = footprints[level];
        source.PlacedFootprint.Offset += staging_offset;
        destination.pResource = result.texture.resource.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destination.SubresourceIndex = level;
        list.CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    }
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition = {result.texture.resource.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                          D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
    list.ResourceBarrier(1, &barrier);
    return result;
}
std::vector<PolygonTextureUpload> PolygonD3D12::generate_a1r5g5b5(ID3D12GraphicsCommandList &list,
                                                                  std::uint32_t primary, std::uint32_t alternate,
                                                                  std::uint32_t dimension)
{
    require(primary != 0 && alternate != 0 && dimension > 0 && dimension <= D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION,
            "generated polygon texture dimensions/tokens");
    ComPtr<ID3D12Device> command_device;
    check(list.GetDevice(IID_PPV_ARGS(&command_device)), "generated texture command device");
    require(command_device.Get() == impl_->device.Get(), "generated texture command list belongs to another device");
    auto &device = *impl_->device.Get();
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support{DXGI_FORMAT_B5G5R5A1_UNORM};
    check(device.CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)),
          "generated A1R5G5B5 support query");
    constexpr auto required = D3D12_FORMAT_SUPPORT1_TEXTURE2D | D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE;
    require((support.Support1 & required) == required, "device cannot sample generated A1R5G5B5 textures");
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    description.Width = description.Height = dimension;
    description.DepthOrArraySize = description.MipLevels = 1;
    description.Format = DXGI_FORMAT_B5G5R5A1_UNORM;
    description.SampleDesc.Count = 1;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap.CreationNodeMask = heap.VisibleNodeMask = 1;
    ComPtr<ID3D12Resource> staging;
    check(device.CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_COPY_SOURCE,
                                         nullptr, IID_PPV_ARGS(&staging)),
          "generated texture staging allocation");
    // NEW HOST (R28b): the generated resource stays B5G5R5A1 (the 16-bit readback
    // is its recovery payload), so its identity's profile never has high
    // precision; the 430D40 replacements that fill it keep edge fix and mips.
    auto generated_profile = texture_profile();
    generated_profile.high_precision = false;
    // NEW HOST (R32 perf-swap): generated identities are the usual 430D40 destinations; their shape is
    // noted so a race load prewarms the levels its swaps will upload.
    note_replacement_profile(generated_profile);
    std::vector<PolygonTextureUpload> result;
    result.reserve(primary == alternate ? 1 : 2);
    for (unsigned index = 0; index < (primary == alternate ? 1u : 2u); ++index)
    {
        PolygonTextureUpload upload;
        upload.texture.token = index ? alternate : primary;
        upload.texture.format = description.Format;
        upload.texture.profile = generated_profile; // NEW HOST (R28b)
        upload.staging = staging;
        check(device.CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_COPY_DEST,
                                             nullptr, IID_PPV_ARGS(&upload.texture.resource)),
              "generated texture destination allocation");
        result.push_back(std::move(upload));
    }
    // Allocate all owners before recording. The source430730 does not fill
    // its pixel allocation; preserve same-initial-content identity without
    // inventing a color. D3D12 chooses the fresh allocation's initial bits.
    for (auto &upload : result)
    {
        list.CopyResource(upload.texture.resource.Get(), staging.Get());
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition = {upload.texture.resource.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                              D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
        list.ResourceBarrier(1, &barrier);
    }
    return result;
}
ID3D12Device &PolygonD3D12::device() const noexcept
{
    return *impl_->device.Get();
}
void PolygonD3D12::set_precise_vertex_mapping(bool enabled)
{
    if (enabled && !impl_->precise_vs)
        impl_->precise_vs = compile_precise_vertex_shader();
    impl_->precise = enabled;
}
bool PolygonD3D12::precise_vertex_mapping() const noexcept
{
    return impl_->precise;
}
void PolygonD3D12::set_raster_quality(const PolygonRasterQuality &quality)
{
    require(static_cast<std::uint8_t>(quality.filter) <= 2 && quality.anisotropy >= 1 && quality.anisotropy <= 16,
            "polygon raster quality outside filter/anisotropy domain");
    require(quality.texture_sharpness >= -3 && quality.texture_sharpness <= 3,
            "polygon texture sharpness outside -3..3");
    if (quality.alpha_to_coverage && !impl_->coverage_ps)
        impl_->coverage_ps = compile_alpha_to_coverage_pixel_shader();
    // NEW HOST (R28): variants compiled before the quality is published.
    if (quality.smooth_magnification && !impl_->smooth_ps)
        impl_->smooth_ps = compile_smooth_pixel_shader(false);
    if (quality.smooth_magnification && quality.alpha_to_coverage && !impl_->coverage_smooth_ps)
        impl_->coverage_smooth_ps = compile_smooth_pixel_shader(true);
    impl_->quality = quality;
}
const PolygonRasterQuality &PolygonD3D12::raster_quality() const noexcept
{
    return impl_->quality;
}
void PolygonD3D12::prewarm_pipelines(DXGI_SAMPLE_DESC samples, const PolygonRasterQuality &quality)
{
    auto &p = *impl_;
    require(samples.Count != 0 && samples.Count <= D3D12_MAX_MULTISAMPLE_SAMPLE_COUNT,
            "polygon prewarm requires a valid sample count");
    p.adopt_finished_jobs();
    const auto target = prewarm_target(samples, quality);
    if (std::any_of(p.jobs.begin(), p.jobs.end(), [&](const auto &job) { return job.target == target; }))
        return; // already building this target
    auto needs = p.needs(samples, quality);
    if (!needs.any())
        return; // every remembered state and variant already exists
    // Everything the job reads is copied now; the render thread keeps going.
    ComPtr<ID3D12Device> device = p.device;
    auto job = std::async(std::launch::async, [device, target, needs = std::move(needs)]() mutable {
        PrewarmResult result;
        result.target = target;
        auto &in = needs.inputs;
        if (needs.precise)
            in.precise_vs = result.precise_vs = compile_precise_vertex_shader();
        if (needs.coverage)
            in.coverage_ps = result.coverage_ps = compile_alpha_to_coverage_pixel_shader();
        if (needs.smooth)
            in.smooth_ps = result.smooth_ps = compile_smooth_pixel_shader(false);
        if (needs.coverage_smooth)
            in.coverage_smooth_ps = result.coverage_smooth_ps = compile_smooth_pixel_shader(true);
        result.pipelines.reserve(needs.keys.size());
        for (const auto &key : needs.keys)
        {
            D3D12_GRAPHICS_PIPELINE_STATE_DESC d;
            describe_pipeline(in, key, d);
            ComPtr<ID3D12PipelineState> pso;
            check(device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&pso)), "polygon pipeline prewarm");
            result.pipelines.emplace_back(key, std::move(pso));
        }
        return result;
    });
    p.jobs.push_back({target, std::move(job)});
}
bool PolygonD3D12::pipelines_prewarmed(DXGI_SAMPLE_DESC samples, const PolygonRasterQuality &quality)
{
    auto &p = *impl_;
    p.adopt_finished_jobs();
    const auto target = prewarm_target(samples, quality);
    if (std::any_of(p.jobs.begin(), p.jobs.end(), [&](const auto &job) { return job.target == target; }))
        return false;
    if (!p.needs(samples, quality).any())
        return true;
    prewarm_pipelines(samples, quality); // states remembered since the last job: build them too
    return false;
}
std::vector<std::uint64_t> PolygonD3D12::pipeline_states() const
{
    std::vector<std::uint64_t> result;
    result.reserve(impl_->seen.size());
    for (const auto &seen : impl_->seen)
        result.push_back(encode_seen(seen));
    return result;
}
void PolygonD3D12::remember_pipeline_states(std::span<const std::uint64_t> codes)
{
    for (const auto code : codes)
        if (const auto seen = decode_seen(code))
            impl_->seen.insert(*seen);
}
void PolygonD3D12::note_replacement_profile(const PolygonTextureProfile &profile) const
{
    // The same format and MIP support queries as upload_a1r5g5b5; an unsupported format notes nothing
    // (such an upload throws anyway).
    auto &device = *impl_->device.Get();
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support{polygon_profile_format(profile)};
    if (FAILED(device.CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support))))
        return;
    constexpr auto required = D3D12_FORMAT_SUPPORT1_TEXTURE2D | D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE;
    if ((support.Support1 & required) != required)
        return;
    host_texture_processing::note_replacement_shape(
        {profile.edge_fix, profile.high_precision,
         profile.mipmaps && (support.Support1 & D3D12_FORMAT_SUPPORT1_MIP) != 0});
}
PolygonTextureProfile PolygonD3D12::texture_profile() const
{
    PolygonTextureProfile profile;
    profile.edge_fix = impl_->quality.texture_edge_fix;
    profile.mipmaps = impl_->quality.generate_mipmaps;
    if (impl_->quality.high_precision_textures)
    {
        D3D12_FEATURE_DATA_FORMAT_SUPPORT support{DXGI_FORMAT_B8G8R8A8_UNORM};
        check(impl_->device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)),
              "polygon B8G8R8A8 support query");
        constexpr auto required = D3D12_FORMAT_SUPPORT1_TEXTURE2D | D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE;
        profile.high_precision = (support.Support1 & required) == required;
    }
    return profile;
}
bool polygon_texture_consistent(const PolygonGpuTexture &texture) noexcept
{
    if (!texture.resource || !texture.recovery || texture.format != polygon_profile_format(texture.profile))
        return false;
    const std::uint64_t scale = texture.scale;
    if (!(scale == 1 || ((scale == 2 || scale == 4) && scale <= texture.profile.upscale)))
        return false;
    const auto description = texture.resource->GetDesc();
    return description.Format == texture.format && description.Width == texture.recovery->width * scale &&
           std::uint64_t(description.Height) == texture.recovery->height * scale;
}
void PolygonD3D12::set_sample_desc(DXGI_SAMPLE_DESC samples)
{
    require(samples.Count != 0 && samples.Count <= D3D12_MAX_MULTISAMPLE_SAMPLE_COUNT,
            "polygon target requires a valid sample count");
    if (samples.Count == impl_->samples.Count && samples.Quality == impl_->samples.Quality)
        return;
    impl_->samples = samples; // R28b: PSOs are kept per sample count (recordings retain theirs anyway)
}
DXGI_SAMPLE_DESC PolygonD3D12::sample_desc() const noexcept
{
    return impl_->samples;
}
PolygonGpuRecording PolygonD3D12::record(ID3D12GraphicsCommandList &list, const CapturedPolygonFrame &frame,
                                         const PolygonDeviceState &initial,
                                         std::shared_ptr<const PolygonGpuTextures> textures, std::uint32_t width,
                                         std::uint32_t height,
                                         std::optional<PolygonLogicalRasterExtent> retained_canvas)
{
    // Standalone form: a call-local arena retained by the returned recording
    // (the pre-R19 per-call allocation lifetime contract).
    auto local = std::make_shared<PolygonTransientArena>(*impl_->device.Get(), 64u * 1024u);
    auto result = record(list, *local, frame, initial, std::move(textures), width, height, retained_canvas);
    result.retained_arena = std::move(local);
    return result;
}
PolygonGpuRecording PolygonD3D12::record(ID3D12GraphicsCommandList &list, PolygonTransientArena &arena,
                                         const CapturedPolygonFrame &frame, const PolygonDeviceState &initial,
                                         std::shared_ptr<const PolygonGpuTextures> textures, std::uint32_t width,
                                         std::uint32_t height,
                                         std::optional<PolygonLogicalRasterExtent> retained_canvas)
{
    return record_frame(list, arena, frame, initial, std::move(textures), width, height, retained_canvas, nullptr);
}
PolygonGpuRecording PolygonD3D12::record_reprojected(ID3D12GraphicsCommandList &list, PolygonTransientArena &arena,
                                                     const CapturedPolygonFrame &frame,
                                                     const PolygonDeviceState &initial,
                                                     std::shared_ptr<const PolygonGpuTextures> textures,
                                                     std::uint32_t width, std::uint32_t height,
                                                     std::optional<PolygonLogicalRasterExtent> retained_canvas,
                                                     const PolygonReprojection &reprojection, std::size_t segment)
{
    SGN_PROFILE_SCOPE("gpu.polygon_record_reprojected");
    require(reprojection.base && reprojection.base->submission &&
                segment < reprojection.base->submission->segments.size() &&
                reprojection.base->submission->segments[segment].device_frame &&
                reprojection.base->submission->segments[segment].device_frame->polygon_commands.get() == &frame,
            "polygon re-projection frame is not the named segment of its base");
    const Reproject reproject{&reprojection, segment, {}, false};
    return record_frame(list, arena, frame, initial, std::move(textures), width, height, retained_canvas, &reproject);
}
PolygonGpuRecording PolygonD3D12::record_reprojected_copy(
    ID3D12GraphicsCommandList &list, PolygonTransientArena &arena, const CapturedPolygonFrame &frame,
    const PolygonDeviceState &initial, std::shared_ptr<const PolygonGpuTextures> textures, std::uint32_t width,
    std::uint32_t height, std::optional<PolygonLogicalRasterExtent> retained_canvas,
    const PolygonReprojection &reprojection, std::span<const CapturedPolygonDraw *const> sources)
{
    require(reprojection.base != nullptr, "polygon re-projection copy requires its base");
    const Reproject reproject{&reprojection, 0, sources, true};
    return record_frame(list, arena, frame, initial, std::move(textures), width, height, retained_canvas, &reproject);
}
PolygonGpuRecording PolygonD3D12::record_frame(ID3D12GraphicsCommandList &list, PolygonTransientArena &arena,
                                               const CapturedPolygonFrame &frame, const PolygonDeviceState &initial,
                                               std::shared_ptr<const PolygonGpuTextures> textures, std::uint32_t width,
                                               std::uint32_t height,
                                               std::optional<PolygonLogicalRasterExtent> retained_canvas,
                                               const Reproject *reproject)
{
    SGN_PROFILE_SCOPE("gpu.polygon_record");
    require(textures && textures->epoch == frame.resource_epoch, "polygon GPU texture epoch mismatch");
    ComPtr<ID3D12Device> command_device;
    check(list.GetDevice(IID_PPV_ARGS(&command_device)), "polygon command device");
    require(command_device.Get() == impl_->device.Get(), "polygon command list belongs to another device");
    require(width > 0 && height > 0 && width <= 16384 && height <= 16384,
            "polygon GPU target dimensions outside D3D12 bound");
    const auto logical = retained_canvas.value_or(PolygonLogicalRasterExtent{width, height});
    require(logical.width > 0 && logical.height > 0 && logical.width <= 16384 && logical.height <= 16384,
            "polygon retained canvas dimensions outside D3D12 bound");
    // NEW HOST (R20 wide_layout): widescreen canvas for retained (fitted)
    // canvases only. The captured 4:3 extent is the centre of a canvas that
    // fills the whole target; host_wide_layout.hpp. Exact-target admission and
    // the original 4:3 presentation keep canvas==logical, offset 0 (identity).
    const auto wide_layout = host_wide::compute_layout(width, height, host_wide::original_4x3());
    const bool wide_canvas = retained_canvas.has_value() && wide_layout.wide();
    float canvas_width = static_cast<float>(logical.width), canvas_height = static_cast<float>(logical.height);
    float canvas_offset_x = 0.0f, canvas_offset_y = 0.0f;
    if (wide_canvas)
    {
        const double units_x = double(logical.width) / host_wide::source_width,
                     units_y = double(logical.height) / host_wide::source_height;
        canvas_width = static_cast<float>(double(wide_layout.canvas_width) * units_x);
        canvas_height = static_cast<float>(double(wide_layout.canvas_height) * units_y);
        canvas_offset_x = static_cast<float>(double(wide_layout.extend_x) * units_x);
        canvas_offset_y = static_cast<float>(double(wide_layout.extend_y) * units_y);
        require(canvas_width > 0.0f && canvas_height > 0.0f && canvas_width <= 16384.0f && canvas_height <= 16384.0f,
                "polygon widescreen canvas outside D3D12 bound");
    }
    {
        // NEW HOST (R19) insertions: ordered, in range, non-null (never source data).
        std::size_t previous = 0;
        for (const auto &insertion : frame.gpu_insertions)
        {
            require(insertion.command && insertion.before_command >= previous &&
                        insertion.before_command <= frame.commands.size(),
                    "polygon GPU insertion order/position");
            previous = insertion.before_command;
        }
    }
    auto &p = *impl_;
    // NEW HOST (R30 perf_renderer): stamp of this call for the per-record marks below.
    const auto serial = ++p.record_serial;
    if (serial % 64 == 0)
    {
        SGN_PROFILE_SCOPE("gpu.polygon_cache_sweep");
        p.sweep_resources();
    }
    // NEW HOST (R33): the kept plan of this base segment for this exact target configuration.
    if (reproject && !reproject->copy)
        if (const auto *entry = p.find_reprojection(*reproject, frame, textures, width, height, retained_canvas))
            return p.emit_reprojection_entry(list, arena, *entry, *reproject->reprojection, serial);
    // Reused call scratch (capacity persists across frames; contents never do).
    auto &draws = p.scratch_draws;
    draws.clear();
    auto &sampler_pairs = p.scratch_sampler_pairs;
    sampler_pairs.clear();
    auto &sampler_pair_keys = p.scratch_sampler_pair_keys;
    sampler_pair_keys.clear();
    auto &sampler_pair_indices = p.scratch_sampler_indices;
    sampler_pair_indices.clear();
    auto &token_index = p.scratch_tokens;
    token_index.clear();
    bool token_index_built = false;
    auto &insertions = p.scratch_insertions;
    insertions.clear();
    const auto &bank = textures->entries;
    auto state = initial;
    std::size_t bytes = 0, vertices_total = 0, indices_total = 0;
    // Same checks as the per-draw texture() + owner lookup, performed once per
    // token of this bank; a token that passed once passes identically again.
    // NEW HOST (R30 perf_renderer): the token lookup is one flat index of the
    // bank (built on first use) instead of a scan of every entry per token, and
    // the resource facts (description, device) come from the resource record.
    // Same checks, same order, same messages as texture() and the owner check.
    const auto cached_texture = [&](std::uint32_t token) -> const PolygonGpuTexture * {
        if (token == 0)
            return nullptr;
        if (!token_index_built)
        {
            token_index.reserve(bank.size());
            for (std::uint32_t i = 0; i < bank.size(); ++i)
            {
                if (auto *earlier = token_index.find(bank[i].token))
                    *earlier |= Impl::duplicate_token;
                else
                    token_index.insert(bank[i].token, i);
            }
            if (p.scratch_entry_verified.size() < bank.size())
            {
                p.scratch_entry_verified.resize(bank.size());
                p.scratch_entry_resource.resize(bank.size());
            }
            token_index_built = true;
        }
        const auto *slot = token_index.find(token);
        require(!slot || (*slot & Impl::duplicate_token) == 0, "duplicate polygon texture token");
        const auto *found = slot ? &bank[*slot] : nullptr;
        require(found && found->resource, "polygon texture token has no retained GPU resource");
        if (p.scratch_entry_verified[*slot] == serial)
            return found;
        const auto record = p.resource_record(*found->resource.Get());
        const auto &d = p.resources[record].desc;
        require(d.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && d.DepthOrArraySize == 1 &&
                    d.SampleDesc.Count == 1 && d.MipLevels != 0,
                "polygon texture requires a single nonmultisampled 2D resource");
        require(d.Format == found->format, "polygon texture view format differs from retained resource");
        require(found->format == DXGI_FORMAT_R8G8B8A8_UNORM || found->format == DXGI_FORMAT_B8G8R8A8_UNORM ||
                    found->format == DXGI_FORMAT_B5G5R5A1_UNORM || found->format == DXGI_FORMAT_B5G6R5_UNORM ||
                    found->format == DXGI_FORMAT_B4G4R4A4_UNORM,
                "polygon texture format must preserve unorm component arithmetic");
        check(p.resources[record].device_lookup, "polygon texture device lookup");
        require(p.resources[record].same_device, "polygon texture belongs to a different D3D12 device");
        p.resources[record].last_record = serial;
        p.scratch_entry_verified[*slot] = serial;
        p.scratch_entry_resource[*slot] = record;
        return found;
    };
    // ROOT DIRECTIVE / R19: fixed-function planning on state change only. The
    // plan is a pure function of the device state (bank, canvas and precise
    // mode are fixed per call), so it is recomputed only after a command that
    // changes a state value; its checks run in the original order when it is.
    struct StatePlan
    {
        PipelineKey key{};
        std::array<std::uint32_t, 24> constants{};
        std::array<const PolygonGpuTexture *, 2> textures{};
        std::array<D3D12_SAMPLER_DESC, 2> samplers{};
        std::optional<std::size_t> sampler_pair;   // interned lazily, at the original point
        bool smooth_eligible{};                    // NEW HOST (R28): every enabled stage LINEAR min+mag, setting on
        bool smooth_capable{}, coverage_capable{}; // NEW HOST (R28b): the same without the settings
    };
    std::optional<StatePlan> plan_cache;
    const auto plan_state = [&]() -> StatePlan & {
        if (plan_cache)
            return *plan_cache;
        StatePlan plan;
        require(rs(state, 4) == 1 && rs(state, 8) == 3 && rs(state, 9) == 2,
                "polygon GPU requires source perspective/solid/Gouraud state");
        require(rs(state, 28) == 0, "polygon GPU fog-enabled path is not implemented");
        require(rs(state, 41) == 0, "polygon GPU legacy color-key-enabled path is not qualified");
        auto cull = rs(state, 22);
        require(cull >= 1 && cull <= 3, "polygon cull state outside source domain");
        plan.key = {boolean(rs(state, 7)),
                    boolean(rs(state, 14)),
                    comparison(rs(state, 23)),
                    boolean(rs(state, 27)),
                    blend(rs(state, 19)),
                    blend(rs(state, 20)),
                    cull,
                    0u,
                    p.precise ? 1u : 0u};
        auto &k = plan.constants;
        k[0] = std::bit_cast<std::uint32_t>(canvas_width);
        k[1] = std::bit_cast<std::uint32_t>(canvas_height); // NEW HOST (R20): ==logical extent unless wide_canvas
        k[2] = boolean(rs(state, 15));
        k[3] = rs(state, 24);
        require(k[3] <= 255, "polygon alpha reference outside byte domain");
        k[4] = comparison(rs(state, 25));
        k[5] = boolean(rs(state, 29));
        bool enabled = true;
        for (unsigned s = 0; s < 2; ++s)
        {
            if (!enabled)
                break;
            const auto op = ts(state, s, 1);
            if (op == 1)
            {
                enabled = false;
                continue;
            }
            plan.textures[s] = cached_texture(value(state.texture[s]));
            if (!plan.textures[s])
            {
                enabled = false;
                continue;
            }
            k[6 + s] = 1;
            validate_stage(k, plan.samplers, state, s, p.quality, *plan.textures[s]);
        }
        // NEW HOST (R23): alpha-tested opaque draws (ALPHATESTENABLE, blending off,
        // ordered comparison) take the alpha-to-coverage variant under MSAA.
        // Any other combination, 1 sample or the setting off: exact discard path.
        plan.key.alpha_to_coverage = (p.quality.alpha_to_coverage && p.samples.Count > 1 && k[2] == 1 &&
                                      plan.key.blend_enabled == 0 && (k[4] == 2 || k[4] == 4 || k[4] == 5 || k[4] == 7))
                                         ? 1u
                                         : 0u;
        // NEW HOST (R28): smooth magnification only for textured draws whose
        // enabled stages all request LINEAR min and mag (never point sampling).
        plan.smooth_capable = plan.textures[0] != nullptr;
        for (unsigned stage = 0; stage < 2 && plan.smooth_capable; ++stage)
            if (plan.textures[stage])
                plan.smooth_capable = ts(state, stage, 16) == 2 && ts(state, stage, 17) == 2;
        plan.smooth_eligible = p.quality.smooth_magnification && plan.smooth_capable;
        // NEW HOST (R28b): quality-independent A2C eligibility, remembered for prewarm.
        plan.coverage_capable =
            k[2] == 1 && plan.key.blend_enabled == 0 && (k[4] == 2 || k[4] == 4 || k[4] == 5 || k[4] == 7);
        // Preserve the old inactive-stage descriptor exactly (interned later).
        for (unsigned stage = 0; stage < 2; ++stage)
            if (!plan.textures[stage])
            {
                auto &sample = plan.samplers[stage];
                sample.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
                sample.AddressU = sample.AddressV = sample.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                sample.MaxAnisotropy = 1;
                sample.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            }
        plan_cache = plan;
        return *plan_cache;
    };
    // Intern both stages together so their root table remains contiguous.
    const auto sampler_pair = [&](StatePlan &plan) -> std::size_t {
        if (plan.sampler_pair)
            return *plan.sampler_pair;
        const SamplerPairKey key{sampler_key(plan.samplers[0]), sampler_key(plan.samplers[1])};
        const auto hash = sampler_pair_hash(key);
        std::size_t index;
        if (const auto *found =
                sampler_pair_indices.find(hash, [&](std::uint32_t i) { return sampler_pair_keys[i] == key; }))
            index = *found;
        else
        {
            require(sampler_pairs.size() < polygon_gpu_sampler_pair_limit,
                    "polygon GPU frame exceeds unique sampler-pair descriptor limit");
            index = sampler_pairs.size();
            sampler_pair_indices.insert(hash, static_cast<std::uint32_t>(index));
            sampler_pair_keys.push_back(key);
            sampler_pairs.push_back(plan.samplers);
        }
        plan.sampler_pair = index;
        return index;
    };
    const auto flush_insertions = [&](std::size_t &next, std::size_t command_index) {
        while (next < frame.gpu_insertions.size() && frame.gpu_insertions[next].before_command == command_index)
        {
            const auto &insertion = frame.gpu_insertions[next++];
            PendingInsertion pending{draws.size(), &insertion, state, std::nullopt};
            if (insertion.draw)
            {
                // Planned exactly like a draw of this type/flags/viewport; only
                // the vertex payload checks are absent (the payload is GPU-side).
                const auto &r = *insertion.draw;
                require(r.type == 2 || r.type == 4 || r.type == 5 || r.type == 6,
                        "polygon draw FVF/topology outside source subset");
                require(r.type != 2 || r.flags == 0x14, "polygon line list outside source431F10/431830 domain");
                require(r.flags == 0x1c || r.flags == 0x14 || r.flags == 0, "polygon draw flags outside source subset");
                require(r.viewport.full_width == static_cast<std::int32_t>(logical.width) &&
                            r.viewport.full_height == static_cast<std::int32_t>(logical.height),
                        retained_canvas ? "polygon retained canvas differs from captured projection"
                                        : "polygon frame must be projected for the actual target size");
                auto &plan = plan_state();
                PlannedDraw draw{};
                draw.pipeline = plan.key;
                draw.coverage_capable = plan.coverage_capable; // NEW HOST (R28b)
                draw.smooth_capable = plan.smooth_capable;
                draw.pipeline.topology = r.type == 2 ? 2u : 3u;
                draw.pipeline.smooth_magnification = plan.smooth_eligible ? 1u : 0u; // NEW HOST (R28): world
                draw.constants = plan.constants;
                draw.textures = plan.textures;
                draw.sampler_pair = sampler_pair(plan);
                draw.type = r.type;
                pending.plan = draw;
            }
            insertions.push_back(std::move(pending));
        }
    };
    std::size_t next_insertion = 0;
    {
        SGN_PROFILE_SCOPE("gpu.polygon_plan");
        for (std::size_t command_index = 0; command_index < frame.commands.size(); ++command_index)
        {
            if (next_insertion < frame.gpu_insertions.size())
                flush_insertions(next_insertion, command_index);
            const auto &command = frame.commands[command_index];
            if (const auto *render_command = std::get_if<CapturedRenderState>(&command))
            {
                require(render_command->state < state.render.size(), "polygon render state index");
                auto &slot = state.render[render_command->state];
                if (slot != render_command->value)
                {
                    slot = render_command->value;
                    plan_cache.reset();
                }
            }
            else if (const auto *stage_command = std::get_if<CapturedTextureStageState>(&command))
            {
                require(stage_command->stage < 2 && stage_command->state < 32, "polygon texture stage state index");
                auto &slot = state.stage[stage_command->stage][stage_command->state];
                if (slot != stage_command->value)
                {
                    slot = stage_command->value;
                    plan_cache.reset();
                }
            }
            else if (const auto *texture_command = std::get_if<CapturedTexture>(&command))
            {
                require(texture_command->stage < 2, "polygon texture stage index");
                auto &slot = state.texture[texture_command->stage];
                if (slot != texture_command->token)
                {
                    slot = texture_command->token;
                    plan_cache.reset();
                }
            }
            else
            {
                const auto &c = std::get<CapturedPolygonDraw>(command);
                require(c.fvf == 0x2c4 && (c.type == 2 || c.type == 4 || c.type == 5 || c.type == 6),
                        "polygon draw FVF/topology outside source subset");
                require(c.type != 2 || (!c.indexed && (c.vertices.size() == 2 || c.vertices.size() == 8) &&
                                        c.indices.empty() && c.flags == 0x14),
                        "polygon line list outside source431F10/431830 domain");
                require(c.flags == 0x1c || c.flags == 0x14 || c.flags == 0, "polygon draw flags outside source subset");
                require(c.viewport.full_width == static_cast<std::int32_t>(logical.width) &&
                            c.viewport.full_height == static_cast<std::int32_t>(logical.height),
                        retained_canvas ? "polygon retained canvas differs from captured projection"
                                        : "polygon frame must be projected for the actual target size");
                require(c.vertices.size() <= 65536, "polygon draw exceeds WORD vertex domain");
                auto &plan = plan_state();
                PlannedDraw draw{};
                draw.source = &c;
                draw.type = c.type;
                draw.pipeline = plan.key;
                draw.coverage_capable = plan.coverage_capable; // NEW HOST (R28b)
                draw.smooth_capable = plan.smooth_capable;
                draw.pipeline.topology = c.type == 2 ? 2u : 3u;
                draw.constants = plan.constants;
                draw.textures = plan.textures;
                const auto &k = draw.constants;
                const auto used = c.indexed ? c.indices.size() : c.vertices.size();
                // A source screen fan can retain0..2 vertices. Preserve its API
                // command and payload; expansion below yields zero complete
                // triangles and the indexed draw uses zero indices. No source
                // packet is dropped or padded to a fabricated triangle.
                require(c.type == 2 || (c.type == 4 && used % 3 == 0) || (c.type == 5 && (used == 0 || used >= 3)) ||
                            c.type == 6,
                        "polygon primitive element count");
                const auto vertex_count = c.vertices.size();
                for (const auto index : c.indices)
                    require(index < vertex_count, "polygon GPU index outside captured vertices");
                const unsigned uv0 = k[6] ? 6u + 2u * k[11] : 0u, uv1 = k[7] ? 6u + 2u * k[19] : 0u;
                // PORT-FIX-012: only the 432F60 screen fans (type 6, not indexed,
                // flags 14h) can carry a ring point that 41E410 rejected (raw world
                // XYZ, so RHW<=0). D3D7 drew them unclipped (DONOTCLIP); the D3D12
                // pipeline cannot. Their triangles touching such a vertex are not
                // rasterized; every other draw keeps the hard requirement.
                const bool screen_fan = c.type == 6 && !c.indexed && c.flags == 0x14;
                bool screen_flat = true; // NEW HOST (R28): every vertex at the first vertex's RHW bits
                // NEW HOST (R30 perf_renderer): finite_word == std::isfinite(bit_cast<float>) (no CRT call).
                for (std::size_t vertex_index = 0; vertex_index < c.vertices.size(); ++vertex_index)
                {
                    const auto &w = c.vertices[vertex_index].words;
                    screen_flat = screen_flat && w[3] == c.vertices[0].words[3];
                    for (unsigned i = 0; i < 4; ++i)
                        require(finite_word(w[i]), "nonfinite polygon GPU XYZRHW");
                    if (!(std::bit_cast<float>(w[3]) > 0))
                    {
                        if (screen_fan)
                        {
                            draw.drop_nonpositive_rhw = true;
                            continue;
                        }
                        char detail[256];
                        std::snprintf(detail, sizeof detail,
                                      "polygon GPU requires positive reciprocal W (command %zu, type %u, flags %02X, "
                                      "vertex %zu/%zu, XYZRHW %08X %08X %08X %08X)",
                                      command_index, unsigned(c.type), unsigned(c.flags), vertex_index,
                                      c.vertices.size(), unsigned(w[0]), unsigned(w[1]), unsigned(w[2]),
                                      unsigned(w[3]));
                        require(false, detail);
                    }
                    if (uv0)
                        require(finite_word(w[uv0]) && finite_word(w[uv0 + 1]),
                                "polygon GPU active UV pair is nonfinite");
                    if (uv1)
                        require(finite_word(w[uv1]) && finite_word(w[uv1 + 1]),
                                "polygon GPU active UV pair is nonfinite");
                }
                // NEW HOST (R28): perspective (world) draws only; screen-flat 2D keeps the default PS.
                draw.pipeline.smooth_magnification = plan.smooth_eligible && !screen_flat ? 1u : 0u;
                const std::size_t fan_count = c.type == 6 && used > 2 ? (used - 2) * 3 : 0;
                // Pre-R19 upload accounting, kept for the identical host bound below.
                bytes += vertex_count * 40;
                draw.index_count = c.type == 6 ? fan_count : c.indices.size();
                bytes += (draw.index_count * 2 + 3) & ~std::size_t(3);
                require(bytes <= 256 * 1024 * 1024, "polygon GPU frame upload exceeds host safety bound");
                require(draws.size() < polygon_gpu_srv_draw_limit, "polygon GPU frame exceeds SRV descriptor limit");
                draw.first_vertex = vertices_total;
                vertices_total += vertex_count;
                draw.uses_indices = c.indexed || c.type == 6;
                if (draw.uses_indices)
                {
                    draw.first_index = indices_total;
                    indices_total += draw.index_count;
                }
                draw.sampler_pair = sampler_pair(plan);
                draws.push_back(draw);
            }
        }
        if (next_insertion < frame.gpu_insertions.size())
            flush_insertions(next_insertion, frame.commands.size());
    }
    PolygonGpuRecording result;
    result.source_tick = frame.source_tick;
    result.resource_epoch = frame.resource_epoch;
    result.final_state = state;
    result.draws = draws.size();
    if (draws.empty() && insertions.empty())
        return result;
    // All input/state validation precedes allocation and command-list mutation.
    result.retained_root = p.root;
    auto &texture_pairs = p.scratch_texture_pairs;
    texture_pairs.clear();
    auto &texture_pair_indices = p.scratch_texture_indices;
    texture_pair_indices.clear();
    auto &retained_pipelines = p.scratch_retained_pipelines;
    retained_pipelines.clear();
    {
        // NEW HOST (R30 perf_renderer): a pair is keyed by the bank indices of its
        // two textures (every planned texture is an entry of this call's bank).
        // The recording owns them through the bank itself (retained_bank below).
        const auto bank_position = [&](const PolygonGpuTexture *t) -> std::uint64_t {
            return t ? static_cast<std::uint64_t>(t - bank.data()) + 1u : 0u;
        };
        TexturePair last_pair{};
        std::size_t last_pair_index = 0;
        bool have_pair = false;
        const auto intern = [&](PlannedDraw &d) {
            const TexturePair pair{d.textures[0], d.textures[1]};
            if (have_pair && pair == last_pair)
            {
                d.texture_pair = last_pair_index;
                return;
            }
            const auto key = bank_position(pair[0]) | bank_position(pair[1]) << 32;
            if (const auto *found = texture_pair_indices.find(key))
                d.texture_pair = *found;
            else
            {
                d.texture_pair = texture_pairs.size();
                texture_pair_indices.insert(key, static_cast<std::uint32_t>(d.texture_pair));
                texture_pairs.push_back(pair);
            }
            last_pair = pair;
            last_pair_index = d.texture_pair;
            have_pair = true;
        };
        // NEW HOST (R30 perf_renderer): PSO memo by pipeline_key64 (injective over the
        // validated key domain, sample count included). remember() still runs at
        // every key change for a (coverage, smooth) combination not yet remembered:
        // std::set insertion is idempotent, so `seen` receives the same states.
        ID3D12PipelineState *last_pipeline = nullptr;
        PipelineKey last_key{};
        for (auto &d : draws)
        {
            if (!last_pipeline || !(d.pipeline == last_key))
            {
                const auto code = pipeline_key64(d.pipeline, p.samples.Count);
                const auto combination =
                    static_cast<std::uint8_t>(1u << ((d.coverage_capable ? 1u : 0u) | (d.smooth_capable ? 2u : 0u)));
                auto *memo_index = p.pipeline_index.find(code);
                if (!memo_index || (p.pipeline_memo[*memo_index].remembered & combination) == 0)
                    p.remember(d); // NEW HOST (R28b): prewarm input
                if (!memo_index)
                {
                    auto *created = p.pipeline(d.pipeline);
                    p.pipeline_memo.push_back({created, 0, 0});
                    p.pipeline_index.insert(code, static_cast<std::uint32_t>(p.pipeline_memo.size() - 1));
                    memo_index = p.pipeline_index.find(code);
                }
                auto &memo = p.pipeline_memo[*memo_index];
                memo.remembered |= combination;
                last_pipeline = memo.pso;
                last_key = d.pipeline;
                if (memo.retained_record != serial)
                {
                    memo.retained_record = serial;
                    retained_pipelines.push_back(last_pipeline);
                }
            }
            d.pso = last_pipeline;
            intern(d);
        }
        for (auto &insertion : insertions)
            if (insertion.plan)
                intern(*insertion.plan);
    }
    // NEW HOST (R30 perf_renderer): every texture this list references is an entry
    // of `textures`, whose immutable bank owns a reference to each of them, so the
    // recording retains the bank (one reference) instead of one reference per used
    // texture (hundreds of AddRef/Release pairs per presented frame).
    if (!texture_pairs.empty())
        result.retained_bank = textures;
    result.retained_pipelines.reserve(retained_pipelines.size());
    for (auto *pipeline : retained_pipelines)
        result.retained_pipelines.emplace_back(pipeline);
    result.retained_commands.reserve(insertions.size());
    for (const auto &insertion : insertions)
        result.retained_commands.push_back(insertion.insertion->command);
    // NEW HOST (R33): the GPU viewport of this call (the unchanged arithmetic of the
    // former emit section, computed before the upload so a kept plan can store it).
    const auto view = frame_view(width, height, logical, wide_canvas, wide_layout);
    if (reproject && !reproject->copy)
    {
        // NEW HOST (R33) first call for this base segment: keep the plan and its GPU payload.
        auto &entry =
            p.store_reprojection(*reproject, frame, textures, width, height, retained_canvas, view, logical,
                                 canvas_offset_x, canvas_offset_y, vertices_total, indices_total, result.final_state);
        return p.emit_reprojection_entry(list, arena, entry, *reproject->reprojection, serial);
    }
    const auto vertex_bytes = vertices_total * 40, index_bytes = indices_total * sizeof(std::uint32_t);
    PolygonGpuUpload vertex_upload{}, index_upload{}, slot_upload{};
    {
        SGN_PROFILE_SCOPE("gpu.polygon_upload");
        if (vertex_bytes)
        {
            vertex_upload = arena.allocate(vertex_bytes, 256);
            result.upload = arena.page(vertex_upload);
        }
        if (index_bytes)
        {
            index_upload = arena.allocate(index_bytes, 256);
            if (!result.upload)
                result.upload = arena.page(index_upload);
        }
        {
            SGN_PROFILE_SCOPE("gpu.polygon_upload_geometry");
            write_polygon_geometry(draws, vertex_upload.cpu, reinterpret_cast<std::uint32_t *>(index_upload.cpu),
                                   canvas_offset_x, canvas_offset_y, result);
        }
    }
    // NEW HOST (R33) copy mode (the bloom emissive copies of re-projected draws):
    // their slot stream and per-draw mapping, uploaded per call.
    std::vector<ReprojectDraw> *reprojected = nullptr;
    if (reproject && vertices_total)
    {
        slot_upload = arena.allocate(vertices_total * sizeof(std::uint32_t), 256);
        p.prepare_reproject_draws(*reproject, draws, canvas_offset_x, canvas_offset_y,
                                  reinterpret_cast<std::uint32_t *>(slot_upload.cpu), p.scratch_reproject,
                                  p.scratch_reproject_pipelines);
        for (auto *pipeline : p.scratch_reproject_pipelines)
            result.retained_pipelines.emplace_back(pipeline);
        reprojected = &p.scratch_reproject;
    }
    const EmitInput in{frame,
                       *textures,
                       draws,
                       insertions,
                       texture_pairs,
                       sampler_pairs,
                       vertex_upload.gpu,
                       index_upload.gpu,
                       slot_upload.gpu,
                       vertex_bytes,
                       index_bytes,
                       vertices_total * sizeof(std::uint32_t),
                       view.viewport,
                       view.scissor,
                       logical,
                       canvas_offset_x,
                       canvas_offset_y,
                       false,
                       reprojected ? reproject->reprojection : nullptr,
                       reprojected};
    p.emit(list, arena, in, result);
    return result;
}
void PolygonD3D12::Impl::prepare_reproject_draws(const PolygonD3D12::Reproject &rp,
                                                 const std::vector<PlannedDraw> &draws, float canvas_offset_x,
                                                 float canvas_offset_y, std::uint32_t *slot_out,
                                                 std::vector<ReprojectDraw> &out,
                                                 std::vector<ID3D12PipelineState *> &pipelines_out)
{
    const auto &base = *rp.reprojection->base;
    out.clear();
    out.reserve(draws.size());
    pipelines_out.clear();
    std::size_t first_draw = 0;
    if (!rp.copy)
    {
        require(rp.segment < base.segment_first_draw.size(), "polygon re-projection segment outside its base");
        first_draw = base.segment_first_draw[rp.segment];
    }
    for (std::size_t k = 0; k < draws.size(); ++k)
    {
        const auto &d = draws[k];
        const auto n = d.source->vertices.size();
        const PolygonReprojectionDraw *info = nullptr;
        if (!rp.copy)
        {
            require(first_draw + k < base.draws.size(), "polygon re-projection base has fewer draws than its segment");
            info = &base.draws[first_draw + k];
        }
        else
        {
            require(k < rp.sources.size() && rp.sources[k] && rp.sources[k]->vertices.size() == n,
                    "polygon re-projection copy differs from its source draw");
            if (const auto found = base.draw_index.find(rp.sources[k]); found != base.draw_index.end())
                info = &base.draws[found->second];
        }
        ReprojectDraw r;
        auto *slots = slot_out + d.first_vertex;
        if (info && info->mapping.valid)
        {
            require(info->first_slot <= base.slots.size() && n <= base.slots.size() - info->first_slot,
                    "polygon re-projection slots outside their base");
            const auto *from = base.slots.data() + info->first_slot;
            for (std::size_t i = 0; i < n; ++i)
            {
                slots[i] = from[i];
                if (from[i] != polygon_reprojection_passthrough)
                {
                    r.mask |= polygon_reprojection_bit(from[i]);
                    r.max_slot = (std::max)(r.max_slot, std::uint32_t{from[i]});
                }
            }
            const auto &m = info->mapping;
            r.constants = {std::bit_cast<std::uint32_t>(m.kx),
                           std::bit_cast<std::uint32_t>(m.ky),
                           std::bit_cast<std::uint32_t>(m.cx + canvas_offset_x),
                           std::bit_cast<std::uint32_t>(m.cy + canvas_offset_y),
                           std::bit_cast<std::uint32_t>(base.w_near),
                           1u};
        }
        else
            std::fill(slots, slots + n, std::uint32_t{polygon_reprojection_passthrough});
        if (r.mask)
        {
            auto key = d.pipeline;
            key.reproject = 1;
            r.pso = pipeline(key);
            if (std::find(pipelines_out.begin(), pipelines_out.end(), r.pso) == pipelines_out.end())
                pipelines_out.push_back(r.pso);
        }
        out.push_back(r);
    }
}
PolygonD3D12::Impl::ReprojectionEntry &PolygonD3D12::Impl::store_reprojection(
    const PolygonD3D12::Reproject &rp, const CapturedPolygonFrame &frame,
    const std::shared_ptr<const PolygonGpuTextures> &textures, std::uint32_t width, std::uint32_t height,
    const std::optional<PolygonLogicalRasterExtent> &canvas, const FrameView &view,
    const PolygonLogicalRasterExtent &logical, float canvas_offset_x, float canvas_offset_y, std::size_t vertices_total,
    std::size_t indices_total, const PolygonDeviceState &final_state)
{
    SGN_PROFILE_SCOPE("gpu.polygon_reprojection_keep");
    std::erase_if(reprojection_entries, [](const ReprojectionEntry &entry) { return entry.base.expired(); });
    if (reprojection_entries.size() >= reprojection_entry_limit)
        reprojection_entries.erase(reprojection_entries.begin());
    ReprojectionEntry entry;
    entry.base = rp.reprojection->base;
    entry.segment = rp.segment;
    entry.frame = &frame;
    entry.textures = textures;
    entry.width = width;
    entry.height = height;
    entry.canvas = canvas;
    entry.original_4x3 = host_wide::original_4x3();
    entry.precise = precise;
    entry.samples = samples;
    entry.quality = quality;
    entry.draws = scratch_draws;
    entry.insertions = scratch_insertions;
    entry.texture_pairs = scratch_texture_pairs;
    entry.sampler_pairs = scratch_sampler_pairs;
    entry.pipelines.reserve(scratch_retained_pipelines.size());
    for (auto *pipeline : scratch_retained_pipelines)
        entry.pipelines.emplace_back(pipeline);
    entry.final_state = final_state;
    entry.source_tick = frame.source_tick;
    entry.resource_epoch = frame.resource_epoch;
    entry.view = view;
    entry.logical = logical;
    entry.canvas_offset_x = canvas_offset_x;
    entry.canvas_offset_y = canvas_offset_y;
    entry.vertex_bytes = vertices_total * 40;
    entry.index_bytes = indices_total * sizeof(std::uint32_t);
    entry.slot_bytes = vertices_total * sizeof(std::uint32_t);
    const auto index_at = align_up(entry.vertex_bytes, 256), slot_at = align_up(index_at + entry.index_bytes, 256);
    const auto buffer = take_buffer((std::max)(slot_at + entry.slot_bytes, std::size_t{256}));
    entry.buffer = buffer.resource;
    const auto gpu = buffer.resource->GetGPUVirtualAddress();
    entry.vertex_gpu = entry.vertex_bytes ? gpu : 0;
    entry.index_gpu = entry.index_bytes ? gpu + index_at : 0;
    entry.slot_gpu = entry.slot_bytes ? gpu + slot_at : 0;
    PolygonGpuRecording stats;
    write_polygon_geometry(entry.draws, buffer.cpu, reinterpret_cast<std::uint32_t *>(buffer.cpu + index_at),
                           canvas_offset_x, canvas_offset_y, stats);
    entry.stat_vertex_bytes = stats.vertex_bytes;
    entry.stat_index_bytes = stats.index_bytes;
    std::vector<ID3D12PipelineState *> moving_pipelines;
    prepare_reproject_draws(rp, entry.draws, canvas_offset_x, canvas_offset_y,
                            reinterpret_cast<std::uint32_t *>(buffer.cpu + slot_at), entry.reprojected,
                            moving_pipelines);
    for (auto *pipeline : moving_pipelines)
        entry.pipelines.emplace_back(pipeline);
    sgn::host_profile::add("gpu.polygon_reprojection_kept_plan", 0);
    reprojection_entries.push_back(std::move(entry));
    return reprojection_entries.back();
}
PolygonGpuRecording PolygonD3D12::Impl::emit_reprojection_entry(ID3D12GraphicsCommandList &list,
                                                                PolygonTransientArena &arena,
                                                                const ReprojectionEntry &entry,
                                                                const PolygonReprojection &reprojection, std::uint64_t)
{
    PolygonGpuRecording result;
    result.source_tick = entry.source_tick;
    result.resource_epoch = entry.resource_epoch;
    result.final_state = entry.final_state;
    result.draws = entry.draws.size();
    if (entry.draws.empty() && entry.insertions.empty())
        return result;
    result.retained_root = root;
    if (!entry.texture_pairs.empty())
        result.retained_bank = entry.textures;
    result.retained_pipelines = entry.pipelines;
    result.retained_commands.reserve(entry.insertions.size());
    for (const auto &insertion : entry.insertions)
        result.retained_commands.push_back(insertion.insertion->command);
    result.upload = entry.buffer;
    result.vertex_bytes = entry.stat_vertex_bytes;
    result.index_bytes = entry.stat_index_bytes;
    const EmitInput in{*entry.frame,          *entry.textures,       entry.draws,
                       entry.insertions,      entry.texture_pairs,   entry.sampler_pairs,
                       entry.vertex_gpu,      entry.index_gpu,       entry.slot_gpu,
                       entry.vertex_bytes,    entry.index_bytes,     entry.slot_bytes,
                       entry.view.viewport,   entry.view.scissor,    entry.logical,
                       entry.canvas_offset_x, entry.canvas_offset_y, true,
                       &reprojection,         &entry.reprojected};
    emit(list, arena, in, result);
    return result;
}
// The pre-R33 descriptor copy and emit sections of record(), moved; the R33
// additions only act with in.reprojection (a draw whose slots move takes its
// re-projection PSO and b1 words; nothing else about the stream changes).
void PolygonD3D12::Impl::emit(ID3D12GraphicsCommandList &list, PolygonTransientArena &arena, const EmitInput &in,
                              PolygonGpuRecording &result)
{
    auto &device_ref = *device.Get();
    const auto &draws = in.draws;
    const auto &insertions = in.insertions;
    const auto &texture_pairs = in.texture_pairs;
    const auto &sampler_pairs = in.sampler_pairs;
    const auto &bank = in.textures.entries;
    PolygonGpuDescriptors srv_range{}, sampler_range{};
    const bool descriptors = !texture_pairs.empty();
    if (descriptors)
    {
        srv_range = arena.srvs(texture_pairs.size() * 2);
        sampler_range = arena.samplers(sampler_pairs.size() * 2);
        result.descriptors = arena.heap(srv_range);
        result.samplers = arena.heap(sampler_range);
        // NEW HOST (R30 perf_renderer): every texture view and sampler is written
        // once into a CPU-only cache (per resource / per exact sampler bits) and
        // this frame's shader-visible tables are filled by ONE CopyDescriptors per
        // heap type. A copied descriptor is the identical view; the table layout
        // (two SRVs per pair, two samplers per pair, in interning order) is unchanged.
        SGN_PROFILE_SCOPE("gpu.polygon_descriptors");
        auto &sources = scratch_sources;
        auto &ones = scratch_ones;
        const auto count = (std::max)(texture_pairs.size(), sampler_pairs.size()) * 2;
        if (ones.size() < count)
            ones.resize(count, 1u);
        sources.clear();
        for (const auto &pair : texture_pairs)
            for (const auto *t : pair)
            {
                if (!t)
                {
                    sources.push_back(empty_srv());
                    continue;
                }
                std::uint32_t record;
                if (in.kept_plan)
                {
                    // NEW HOST (R33): a kept plan's textures were checked when it was planned;
                    // their records are looked up again (the call scratch holds other banks).
                    record = resource_record(*t->resource.Get());
                    resources[record].last_record = record_serial;
                }
                else
                    record = scratch_entry_resource[static_cast<std::size_t>(t - bank.data())];
                sources.push_back(texture_srv(record, t->format));
            }
        const auto srv_count = static_cast<UINT>(sources.size());
        device_ref.CopyDescriptors(1, &srv_range.cpu, &srv_count, srv_count, sources.data(), ones.data(),
                                   D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        sources.clear();
        bound_sampler_cache(sampler_pairs.size() * 2);
        for (const auto &pair : sampler_pairs)
            for (const auto &sample : pair)
                sources.push_back(sampler_descriptor(sample));
        const auto sampler_count = static_cast<UINT>(sources.size());
        device_ref.CopyDescriptors(1, &sampler_range.cpu, &sampler_count, sampler_count, sources.data(), ones.data(),
                                   D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }
    // NEW HOST (R33): the slot motions of this present (root SRV t2 of the re-projection VS).
    const auto *reprojection = in.reprojection;
    D3D12_GPU_VIRTUAL_ADDRESS motions_gpu = 0;
    if (reprojection && !reprojection->motions.empty())
    {
        const auto bytes = reprojection->motions.size_bytes();
        const auto upload = arena.allocate(bytes, 256);
        std::memcpy(upload.cpu, reprojection->motions.data(), bytes);
        motions_gpu = upload.gpu;
    }
    const auto moving = [&](std::size_t i) {
        if (!reprojection || !in.reprojected || !motions_gpu)
            return false;
        const auto &r = (*in.reprojected)[i];
        return r.pso && (r.mask & reprojection->moving) != 0 && r.max_slot < reprojection->motions.size();
    };
    SGN_PROFILE_SCOPE("gpu.polygon_emit");
    const auto &viewport = in.viewport;
    const auto &scissor = in.scissor;
    // Redundant-state filter. Every cached value is invalidated whenever a
    // host insertion may have rebound the pipeline.
    ID3D12PipelineState *bound_pipeline = nullptr;
    const std::array<std::uint32_t, 24> *bound_constants = nullptr;
    const std::array<std::uint32_t, 6> *bound_mapping = nullptr; // NEW HOST (R33) b1 words 0..5
    std::size_t bound_textures = ~std::size_t(0), bound_samplers = ~std::size_t(0);
    D3D12_PRIMITIVE_TOPOLOGY bound_topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    const auto bind = [&] {
        bound_pipeline = nullptr;
        bound_constants = nullptr;
        bound_mapping = nullptr;
        bound_textures = bound_samplers = ~std::size_t(0);
        bound_topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
        if (!descriptors)
            return;
        list.SetGraphicsRootSignature(root.Get());
        ID3D12DescriptorHeap *heaps[]{srv_range.heap, sampler_range.heap};
        list.SetDescriptorHeaps(2, heaps);
        list.RSSetViewports(1, &viewport);
        list.RSSetScissorRects(1, &scissor);
        if (in.vertex_bytes)
        {
            const D3D12_VERTEX_BUFFER_VIEW whole[2]{
                {in.vertex_gpu, static_cast<UINT>(in.vertex_bytes), 40},
                {in.slot_gpu, static_cast<UINT>(in.slot_bytes), static_cast<UINT>(sizeof(std::uint32_t))}};
            list.IASetVertexBuffers(0, motions_gpu && in.slot_gpu ? 2u : 1u, whole);
        }
        if (in.index_bytes)
        {
            const D3D12_INDEX_BUFFER_VIEW whole{in.index_gpu, static_cast<UINT>(in.index_bytes), DXGI_FORMAT_R32_UINT};
            list.IASetIndexBuffer(&whole);
        }
        if (motions_gpu)
        {
            // NEW HOST (R33): the camera->world columns (b1 words 8..19) and the motions.
            list.SetGraphicsRoot32BitConstants(3, 12, reprojection->to_world.data(), 8);
            list.SetGraphicsRootShaderResourceView(4, motions_gpu);
        }
    };
    bind();
    // Consecutive draws merge only when every GPU-visible input is identical:
    // PSO, all 24 root constants, both SRV and sampler tables, topology, the
    // index/no-index form and a contiguous element range. D3D12 rasterizes,
    // depth-tests and blends the primitives of one draw in submission order,
    // so a merged range produces the same pixels as the separate draws. Strips
    // are never merged (their primitive assembly restarts per draw).
    // NEW HOST (R33): a moving draw merges only with a moving draw of the same b1 words.
    struct Batch
    {
        const PlannedDraw *first{};
        std::size_t start{}, count{};
        bool indexed{}, mergeable{};
        std::size_t draw{};
        bool moves{};
    };
    std::optional<Batch> pending;
    const auto table = [](const PolygonGpuDescriptors &range, std::size_t pair) {
        auto handle = range.gpu;
        handle.ptr += static_cast<UINT64>(pair) * 2u * range.step;
        return handle;
    };
    std::size_t moved_draws = 0;
    const auto flush = [&] {
        if (!pending)
            return;
        const auto batch = *pending;
        pending.reset();
        if (!batch.count)
            return; // zero-element draw rasterizes nothing
        const auto &d = *batch.first;
        auto *pso = d.pso;
        if (batch.moves)
        {
            const auto &r = (*in.reprojected)[batch.draw];
            pso = r.pso;
            if (!bound_mapping || *bound_mapping != r.constants)
            {
                list.SetGraphicsRoot32BitConstants(3, 6, r.constants.data(), 0);
                bound_mapping = &r.constants;
            }
            ++moved_draws;
        }
        if (bound_pipeline != pso)
        {
            list.SetPipelineState(pso);
            bound_pipeline = pso;
        }
        if (!bound_constants || std::memcmp(bound_constants->data(), d.constants.data(), sizeof(d.constants)) != 0)
        {
            list.SetGraphicsRoot32BitConstants(0, 24, d.constants.data(), 0);
            bound_constants = &d.constants;
        }
        if (bound_textures != d.texture_pair)
        {
            list.SetGraphicsRootDescriptorTable(1, table(srv_range, d.texture_pair));
            bound_textures = d.texture_pair;
        }
        if (bound_samplers != d.sampler_pair)
        {
            list.SetGraphicsRootDescriptorTable(2, table(sampler_range, d.sampler_pair));
            bound_samplers = d.sampler_pair;
        }
        const auto wanted = topology(d.type);
        if (bound_topology != wanted)
        {
            list.IASetPrimitiveTopology(wanted);
            bound_topology = wanted;
        }
        if (batch.indexed)
            list.DrawIndexedInstanced(static_cast<UINT>(batch.count), 1, static_cast<UINT>(batch.start), 0, 0);
        else
            list.DrawInstanced(static_cast<UINT>(batch.count), 1, static_cast<UINT>(batch.start), 0);
        ++result.gpu_draw_calls;
    };
    const auto run_insertion = [&](const PendingInsertion &pending_insertion) {
        flush();
        std::optional<PolygonGpuDrawState> draw_state;
        if (const auto &plan = pending_insertion.plan)
        {
            draw_state.emplace();
            describe(plan->pipeline, draw_state->pipeline);
            draw_state->pipeline_key = pipeline_key64(plan->pipeline, samples.Count);
            draw_state->constants = plan->constants;
            draw_state->textures = table(srv_range, plan->texture_pair);
            draw_state->samplers = table(sampler_range, plan->sampler_pair);
            draw_state->topology = topology(plan->type);
        }
        const PolygonGpuRecordContext context{list,
                                              device_ref,
                                              arena,
                                              color_format,
                                              depth_format,
                                              samples,
                                              viewport,
                                              scissor,
                                              in.logical,
                                              in.frame.source_tick,
                                              in.frame.resource_epoch,
                                              pending_insertion.state,
                                              in.textures,
                                              draw_state ? &*draw_state : nullptr,
                                              in.canvas_offset_x,
                                              in.canvas_offset_y};
        pending_insertion.insertion->command->record(context);
        bind();
    };
    std::size_t insertion_index = 0;
    for (std::size_t i = 0; i < draws.size(); ++i)
    {
        while (insertion_index < insertions.size() && insertions[insertion_index].draw_index == i)
            run_insertion(insertions[insertion_index++]);
        const auto &d = draws[i];
        const auto &c = *d.source;
        const bool moves = moving(i);
        const Batch next{&d,
                         d.uses_indices ? d.first_index : d.first_vertex,
                         d.uses_indices ? d.index_count : c.vertices.size(),
                         d.uses_indices,
                         c.type != 5,
                         i,
                         moves};
        if (pending && pending->mergeable && next.mergeable && pending->indexed == next.indexed &&
            pending->start + pending->count == next.start && pending->moves == next.moves &&
            (!moves || (*in.reprojected)[pending->draw].constants == (*in.reprojected)[i].constants))
        {
            const auto &a = *pending->first;
            if (a.pso == d.pso && a.texture_pair == d.texture_pair && a.sampler_pair == d.sampler_pair &&
                topology(a.type) == topology(d.type) && a.constants == d.constants)
            {
                pending->count += next.count;
                continue;
            }
        }
        flush();
        pending = next;
    }
    flush();
    while (insertion_index < insertions.size())
        run_insertion(insertions[insertion_index++]);
    if (reprojection)
        sgn::host_profile::sample("gpu.reprojected_draw_batches", static_cast<std::int64_t>(moved_draws));
}
} // namespace sgn
