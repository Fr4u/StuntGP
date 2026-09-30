// NEW HOST rendering (gpu_world R19): renderer side of the hybrid track path,
// built only on perf_gpu's R19 API. Static per-level buffers come from
// Renderer::create_static_buffer (once per level); each replacement draw is a
// PLANNED PolygonGpuCommand inserted with PolygonDeviceContext::insert_gpu_command
// at the exact 433F40 stream position; per-draw data uses the recorder's
// transient arena. The pixel shader and fixed-function plan are the polygon
// recorder's (PolygonGpuDrawState); only the VS, input layout and root differ.
#include "sgn/detail/gpu_world_ticket.hpp"
#include "sgn/host_shader_cache.hpp"
#include "sgn/assets.hpp"
#include "sgn/format_require.hpp"
#include "sgn/renderer.hpp"
#include "sgn/polygon_device_context.hpp"
#include <d3dcompiler.h>
#include <bit>
#include <climits>
#include <cstring>
#include <map>
#include <sstream>
#include <stdexcept>

namespace sgn::gpu_world
{
using Microsoft::WRL::ComPtr;
namespace
{
using W = std::uint32_t;
void check(HRESULT value, const char *action)
{
    if (FAILED(value))
    {
        std::ostringstream s;
        s << action << " HRESULT 0x" << std::hex << static_cast<std::uint32_t>(value);
        throw std::runtime_error(s.str());
    }
}
using format_check::require;

// b0 is the polygon recorder's 24-word block (the PS reads the same words).
// The final mapping is the polygon VS's opt-in precise mapping, textually
// (Renderer::set_polygon_precise_vertex_mapping(true) in hybrid mode), so a
// vertex with identical XYZRHW gets an identical clip position in both.
constexpr char shader_source[] = R"(
cbuffer State:register(b0) {
    float2 size; uint alpha_enable,alpha_ref;
    uint alpha_func,specular_enable,enabled0,enabled1;
    uint4 color0,alpha0,color1,alpha1;
};
cbuffer View:register(b1) {
    uint4 m0,m1,m2,m3;      // 61C428 words [0..15]
    uint4 projection;       // px,py,pz,offset (61C3E8/3FC/410/420)
    uint4 viewport;         // scale_x,scale_y,center_x,center_y (429A46..5A)
    uint4 canvas;           // NEW R20 host canvas offset x,y (binary32), same add as the recorder's CPU path
};
cbuffer Draw:register(b2) {uint motion_u,motion_v,color_and,color_or;};
StructuredBuffer<float4> positions:register(t2);   // part+30 XYZ (static)
StructuredBuffer<uint4> coordinates:register(t3);  // part+2C u,v,vertex,colour (static)
StructuredBuffer<uint4> locals:register(t4);       // reference local vertices of this draw
struct Output {float4 p:SV_POSITION;noperspective float4 diffuse:COLOR0;noperspective float4 specular:COLOR1;float4 uv:TEXCOORD0;};
float4 unpack(uint x){return float4((x>>16)&255,(x>>8)&255,x&255,x>>24)/255.0;}
double f(uint bits){return (double)asfloat(bits);}
Output vs(uint local:SV_VertexID){
    Output o;
    const uint4 e=locals[local];
    const uint4 c=coordinates[e.x&0x7fffffff];
    float sx,sy,rhw;
    [branch] if((e.x>>31)!=0){sx=asfloat(e.y);sy=asfloat(e.z);rhw=asfloat(e.w);} // CPU words (429880/429A40)
    else{
        const float4 p=positions[c.z];
        const double x=(double)p.x,y=(double)p.y,z=(double)p.z;
        // 42979A..42983C (== world_track_vertex_inside_42979a): binary32
        // products exact in binary64, sums round to 53 bits; X/Z unspilled,
        // Y spilled to binary32, Z in YZX order.
        precise double X=((f(m0.x)*x+f(m1.x)*y)+f(m2.x)*z)+f(m3.x);
        precise double Yd=((f(m0.y)*x+f(m1.y)*y)+f(m2.y)*z)+f(m3.y);
        precise float Y=(float)Yd;
        precise double Z=((f(m1.z)*y+f(m2.z)*z)+f(m0.z)*x)+f(m3.z);
        precise float px=(float)(f(projection.x)*X);
        precise float py=(float)(f(projection.y)*(double)Y);
        precise float w=(float)Z;
        // 429A40: inverse=1/(w+1); x'=(inverse*x)*scale+center; rhw=inverse*32.
        precise double inverse=1.0/((double)w+1.0);
        precise double dx=(inverse*(double)px)*f(viewport.x)+f(viewport.z);
        precise double dy=(inverse*(double)py)*f(viewport.y)+f(viewport.w);
        precise double dr=inverse*32.0;
        sx=(float)dx;sy=(float)dy;rhw=(float)dr;
    }
    // NEW R20 host canvas offset (PolygonD3D12::record adds the same binary32
    // offset to captured XY on the CPU). Zero in the original 4:3 presentation.
    [branch] if((canvas.x|canvas.y)!=0){precise float ox=asfloat(canvas.x);precise float oy=asfloat(canvas.y);
        precise float cx=sx+ox;precise float cy=sy+oy;sx=cx;sy=cy;}
    // Polygon VS precise mapping with v.p=(sx,sy,rhw,rhw) (4335EA..4335FD).
    precise float w=rcp(rhw);
    precise float4 position=float4(((float2(sx,sy)+0.5)/size*2.0-1.0)*float2(1,-1)*w,rhw*w,w);
    o.p=position;
    o.diffuse=unpack((c.w&color_and)|color_or);
    o.specular=unpack(0);
    // 4335E8/433603 and 433609/43360C: FLD u, FADD motion u, FSTP binary32.
    precise double u=f(c.x)+f(motion_u);
    precise double v=f(c.y)+f(motion_v);
    o.uv=float4((float)u,(float)v,0,0);
    return o;
}
float4 probe(Output i):SV_TARGET{return i.diffuse;}
)";
ComPtr<ID3DBlob> compile(const char *entry, const char *profile)
{
    ComPtr<ID3DBlob> result, errors;
    const auto status = host_shader_cache::compile(
        shader_source, sizeof(shader_source), "gpu-world", nullptr, entry, profile,
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &result,
        &errors);
    if (FAILED(status))
        throw std::runtime_error(errors ? static_cast<const char *>(errors->GetBufferPointer())
                                        : "gpu_world shader compilation failed");
    return result;
}

// Root signature: params 0..2 are the layout the recorder requires (b0 = 24
// constants ALL, t0-t1 and s0-s1 tables PIXEL); 3..7 are gpu_world's.
class Backend
{
  public:
    ComPtr<ID3D12Device> device;
    ComPtr<ID3DBlob> vs;
    ComPtr<ID3D12RootSignature> root;
    std::map<std::uint64_t, ComPtr<ID3D12PipelineState>> pipelines;
    explicit Backend(ID3D12Device &d, const RendererPolygonTargetFormat &format) : device(&d)
    {
        D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
        check(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof options),
              "gpu_world feature query");
        require(options.DoublePrecisionFloatShaderOps != FALSE, "gpu_world requires DoublePrecisionFloatShaderOps");
        vs = compile("vs", "vs_5_0");
        D3D12_DESCRIPTOR_RANGE ranges[2]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, 0},
                                         {D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 2, 0, 0, 0}};
        D3D12_ROOT_PARAMETER p[8]{};
        p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        p[0].Constants = {0, 0, 24};
        for (unsigned i = 0; i < 2; ++i)
        {
            p[i + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            p[i + 1].DescriptorTable = {1, &ranges[i]};
            p[i + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        }
        p[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        p[3].Descriptor = {1, 0};
        p[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        p[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        p[4].Constants = {2, 0, 4};
        p[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        for (unsigned i = 0; i < 3; ++i)
        {
            p[5 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
            p[5 + i].Descriptor = {2 + i, 0};
            p[5 + i].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        }
        D3D12_ROOT_SIGNATURE_DESC description{8, p, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> blob, errors;
        check(D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors),
              "gpu_world root serialization");
        check(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root)),
              "gpu_world root creation");
        // Probe PSO: a device without the D3D11.1 double extensions (ddiv)
        // fails here, at attach, instead of during a frame.
        const auto ps = compile("probe", "ps_5_0");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC probe{};
        probe.pRootSignature = root.Get();
        probe.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
        probe.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        probe.SampleMask = UINT_MAX;
        probe.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        probe.NumRenderTargets = 1;
        probe.RTVFormats[0] = format.color;
        probe.DSVFormat = format.depth;
        probe.SampleDesc = format.samples;
        probe.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        probe.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        probe.RasterizerState.DepthClipEnable = TRUE;
        probe.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        ComPtr<ID3D12PipelineState> test;
        check(device->CreateGraphicsPipelineState(&probe, IID_PPV_ARGS(&test)), "gpu_world probe pipeline creation");
    }
    // The recorder's complete polygon PSO for this state with VS, input layout
    // and root signature replaced (PS/blend/depth/raster/formats unchanged).
    ID3D12PipelineState *pipeline(const PolygonGpuDrawState &state)
    {
        if (auto i = pipelines.find(state.pipeline_key); i != pipelines.end())
            return i->second.Get();
        auto d = state.pipeline;
        d.pRootSignature = root.Get();
        d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
        d.InputLayout = {nullptr, 0};
        ComPtr<ID3D12PipelineState> result;
        check(device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&result)), "gpu_world pipeline creation");
        return pipelines.emplace(state.pipeline_key, std::move(result)).first->second.Get();
    }
};
struct GpuMesh
{
    std::shared_ptr<const GpuStaticBuffer> positions, coordinates;
};
// One inserted draw. Immutable; recordings retain it (and through it the
// backend, the static buffers) until their fence retires.
class WorldInsertion final : public PolygonGpuCommand
{
    std::shared_ptr<Backend> backend_;
    std::shared_ptr<const GpuMesh> mesh_;
    detail::WorldDraw draw_;

  public:
    WorldInsertion(std::shared_ptr<Backend> backend, std::shared_ptr<const GpuMesh> mesh, detail::WorldDraw &&draw)
        : backend_(std::move(backend)), mesh_(std::move(mesh)), draw_(std::move(draw))
    {
    }
    void record(const PolygonGpuRecordContext &ctx) const override
    {
        require(ctx.draw != nullptr, "gpu_world insertion requires the recorder's planned draw state");
        require(&ctx.device == backend_->device.Get(), "gpu_world insertion recorded on another D3D12 device");
        const auto &plan = *ctx.draw;
        // UV1 (FVF words 8/9) is never produced for these corners; the
        // reference would sample stale scratch words there.
        for (unsigned s = 0; s < 2; ++s)
            require(plan.constants[6 + s] == 0 || plan.constants[11 + s * 8] == 0,
                    "gpu_world draw requires texture coordinate set 0");
        auto *pipeline = backend_->pipeline(plan);
        auto view = ctx.transient.allocate(256, 256);
        std::memcpy(view.cpu, draw_.view.data(), sizeof draw_.view);
        // NEW HOST (R20) canvas offset (View.canvas, bytes 96..111 of this 256-byte block).
        static_assert(sizeof draw_.view == 96, "gpu_world View cbuffer layout");
        const std::uint32_t canvas[4]{std::bit_cast<std::uint32_t>(ctx.canvas_offset_x),
                                      std::bit_cast<std::uint32_t>(ctx.canvas_offset_y), 0u, 0u};
        std::memcpy(view.cpu + sizeof draw_.view, canvas, sizeof canvas);
        auto locals = ctx.transient.allocate(draw_.locals.size() * 16, 256);
        std::memcpy(locals.cpu, draw_.locals.data(), draw_.locals.size() * 16);
        auto &list = ctx.list;
        list.SetGraphicsRootSignature(backend_->root.Get());
        list.SetPipelineState(pipeline);
        list.SetGraphicsRoot32BitConstants(0, 24, plan.constants.data(), 0);
        list.SetGraphicsRootDescriptorTable(1, plan.textures);
        list.SetGraphicsRootDescriptorTable(2, plan.samplers);
        list.SetGraphicsRootConstantBufferView(3, view.gpu);
        const std::uint32_t constants[4]{draw_.motion_u_bits, draw_.motion_v_bits, draw_.color_and, draw_.color_or};
        list.SetGraphicsRoot32BitConstants(4, 4, constants, 0);
        list.SetGraphicsRootShaderResourceView(5, mesh_->positions->address);
        list.SetGraphicsRootShaderResourceView(6, mesh_->coordinates->address);
        list.SetGraphicsRootShaderResourceView(7, locals.gpu);
        list.IASetPrimitiveTopology(plan.topology);
        if (draw_.type == 4)
        {
            auto indices = ctx.transient.allocate(draw_.indices.size() * 2, 256);
            std::memcpy(indices.cpu, draw_.indices.data(), draw_.indices.size() * 2);
            // The reference's WORD local indices, unchanged (4335xx fan).
            const D3D12_INDEX_BUFFER_VIEW ib{indices.gpu, static_cast<UINT>(draw_.indices.size() * 2),
                                             DXGI_FORMAT_R16_UINT};
            list.IASetIndexBuffer(&ib);
            list.DrawIndexedInstanced(static_cast<UINT>(draw_.indices.size()), 1, 0, 0, 0);
        }
        else
        {
            list.DrawInstanced(static_cast<UINT>(draw_.locals.size()), 1, 0, 0); // strip, 477CD0 order
        }
    }
};

struct Attachment
{
    Renderer *renderer{};
    PolygonDeviceContext *device{};
    const retail::PolygonSubmissionSink *sink{};
    std::shared_ptr<Backend> backend;
    std::uint64_t mesh_serial{};
    std::shared_ptr<const GpuMesh> mesh;
};
Attachment &attachment() noexcept
{
    static Attachment value;
    return value;
}
void apply_mode(Mode mode)
{
    auto &a = attachment();
    if (a.renderer && a.backend)
        a.renderer->set_polygon_precise_vertex_mapping(mode == Mode::hybrid_exact);
}
template <class T> std::span<const std::byte> bytes_of(const std::vector<T> &values)
{
    return std::as_bytes(std::span<const T>(values));
}
std::shared_ptr<const GpuMesh> gpu_mesh(const detail::StaticMesh &mesh)
{
    auto &a = attachment();
    if (a.mesh && a.mesh_serial == mesh.serial)
        return a.mesh;
    // Level start only (one upload + queue flush each); the previous level's
    // buffers stay alive while in-flight recordings retain their insertions.
    auto result = std::make_shared<GpuMesh>();
    result->positions =
        a.renderer->create_static_buffer(bytes_of(mesh.positions), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    result->coordinates =
        a.renderer->create_static_buffer(bytes_of(mesh.coordinates), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    a.mesh = std::move(result);
    a.mesh_serial = mesh.serial;
    return a.mesh;
}
} // namespace

void attach(Renderer &renderer, PolygonDeviceContext &device, const retail::PolygonSubmissionSink &forwarding_sink)
{
    detach();
    auto &a = attachment();
    a.renderer = &renderer;
    a.device = &device;
    a.sink = &forwarding_sink;
    try
    {
        a.backend = std::make_shared<Backend>(renderer.native_device(), renderer.polygon_target_format());
    }
    catch (const std::runtime_error &)
    {                      // includes FormatError
        a.backend.reset(); // capability probe only: mode 1 stays unavailable, mode 0 unaffected
    }
    detail::set_backend_available(a.backend != nullptr);
    detail::set_mode_listener(&apply_mode);
    apply_mode(requested_mode());
}
void detach() noexcept
{
    auto &a = attachment();
    detail::set_mode_listener(nullptr);
    detail::set_backend_available(false);
    detail::ticket() = detail::Ticket{};
    a = Attachment{};
}
namespace detail
{
bool attached_to(const retail::PolygonSubmissionSink &sink) noexcept
{
    const auto &a = attachment();
    return a.backend && a.sink == &sink;
}
void insert_world_draw(const std::shared_ptr<const StaticMesh> &mesh, WorldDraw &&draw)
{
    auto &a = attachment();
    require(a.backend && a.device && a.renderer, "gpu_world draw without an attached renderer");
    auto gpu = gpu_mesh(*mesh);
    const PolygonGpuDrawRequest request{draw.type, draw.flags, draw.viewport};
    a.device->insert_gpu_command(std::make_shared<WorldInsertion>(a.backend, std::move(gpu), std::move(draw)), request);
}
} // namespace detail
} // namespace sgn::gpu_world
