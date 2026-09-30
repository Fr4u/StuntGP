#include "sgn/source_d3d12_device_production.hpp"
#include "sgn/source_application_window_start_d3d12.hpp"
#include "sgn/new_host_d3d12_application_surfaces.hpp"
#include "sgn/polygon_surface_recipes.hpp"
#include "sgn/race_session.hpp"
#include "sgn/gpu_world.hpp" //NEW host rendering (gpu_world R19)
#include "sgn/format_require.hpp"
#include <d3d.h>
#include <algorithm>
#include <bit>
#include <limits>
namespace sgn::retail
{
namespace
{
// Offsets verified against42F420's descriptor copy at+68 and Windows SDK.
static_assert(0x68 + offsetof(D3DDEVICEDESC, dpcTriCaps) + offsetof(D3DPRIMCAPS, dwAlphaCmpCaps) == 0xe4);
static_assert(0x68 + offsetof(D3DDEVICEDESC, dpcTriCaps) + offsetof(D3DPRIMCAPS, dwShadeCaps) == 0xe8);
static_assert(0x68 + offsetof(D3DDEVICEDESC, dpcTriCaps) + offsetof(D3DPRIMCAPS, dwTextureFilterCaps) == 0xf0);
using sgn::format_check::require;
PolygonDeviceState new_host_stage_policy()
{
    // NEW native sampler/unused-stage policy. Source42FA80 subsequently supplies
    // all recovered render/light writes. This is not a recovered driver default.
    PolygonDeviceState state;
    for (unsigned stage = 0; stage < 2; ++stage)
    {
        state.stage[stage][D3DTSS_COLOROP] = D3DTOP_DISABLE;
        state.stage[stage][D3DTSS_ALPHAOP] = D3DTOP_DISABLE;
        state.stage[stage][D3DTSS_COLORARG1] = D3DTA_TEXTURE;
        state.stage[stage][D3DTSS_COLORARG2] = D3DTA_CURRENT;
        state.stage[stage][D3DTSS_ALPHAARG1] = D3DTA_TEXTURE;
        state.stage[stage][D3DTSS_ALPHAARG2] = D3DTA_CURRENT;
        state.stage[stage][D3DTSS_TEXCOORDINDEX] = stage;
        state.stage[stage][D3DTSS_ADDRESSU] = D3DTADDRESS_WRAP;
        state.stage[stage][D3DTSS_ADDRESSV] = D3DTADDRESS_WRAP;
        state.stage[stage][D3DTSS_MAGFILTER] = D3DTFG_LINEAR;
        state.stage[stage][D3DTSS_MINFILTER] = D3DTFN_LINEAR;
        state.stage[stage][D3DTSS_MIPFILTER] = D3DTFP_NONE;
        state.texture[stage] = 0;
    }
    return state;
}
} // namespace
struct SourceD3D12DeviceProduction::Impl
{
    FrontendState &f;
    SessionWorldMaterials &materials;
    SessionOuterFrameServices &frames;
    SourceMenuPresentationBackend &presentation;
    SourceApplicationLiteralData &application;
    PolygonSubmissionState &scratch;
    const HWND &window;
    std::unique_ptr<Renderer> &renderer;
    RenderOptions options;
    PolygonSubmissionCaptureLimits limits;
    LightGammaD3D12 &gamma;
    LightGammaDevice *&source_gamma;
    NewHostD3D12ApplicationSurfaces &lifecycle;
    NewHostD3D12WindowStartup &startup;
    SourceDrawStartupFailure &failure;
    std::vector<RendererAdapterInfo> adapters;
    std::optional<std::uint32_t> selected;
    RendererDeviceInfo hardware;
    // This is a NEW representation of only consumed descriptor fields. Values
    // come from queried hardware intersected with this pretransformed shader.
    std::uint32_t alpha_comparisons{}, shade{}, filter{}, depth_kind{};
    std::uint16_t texture_units{}, blend_stages{};
    DeviceSetupDevice *selected_device{};
    DeviceSetupDevice *global_device{};
    std::unique_ptr<PolygonDeviceContext> context;
    std::unique_ptr<PolygonDeviceSurface> surface;
    DeviceSetupSurface *surface_receiver{};
    MaterialTransparencyStorage transparency;
    std::unique_ptr<MaterialD3D12> backend;
    PolygonSurfaceRecipes recipes;
    std::unique_ptr<RendererLoadingSurfaces> loading;
    // Represented viewport and white material are CPU state for transformed FVF.
    // There is no invented IDirect3DViewport/Material COM pointer or GPU T&L claim.
    std::optional<D3DVIEWPORT2> viewport;
    std::optional<D3DMATERIAL> white_material;
    bool gpu_released_{}; // terminal NEW host lifetime, never a source readiness word
    bool enumerated{}, surfaces_created{}, depth_created{}, depth_attached{}, device_created{}, driver_checked{};
    // NEW non-owning creation identities, retained after WM_NCDESTROY clears App.window.
    // Never use this HWND for an operation. Initial Renderer identity is needed
    // only until the actual loading/material borrowers exist (they follow recovery).
    HWND creation_window{};
    Renderer *creation_renderer{};
    void check(FrontendState &value) const
    {
        require(&value == &f, "device producer received a foreign frontend");
        f.shared_platform()->require_owner_thread();
        require(!gpu_released_, "device producer GPU bundle was retired for shutdown");
    }
    void actual_renderer() const
    {
        require(bool(renderer), "device producer has no current App Renderer");
        require(renderer->native_window() == window, "device producer Renderer has a foreign HWND");
        renderer->check_health();
    }
    void selected_pair(std::uint32_t a, std::uint32_t d) const
    {
        require(selected && *selected == a, "device producer adapter differs from retained source selection");
        require(d == 0, "NEW D3D12 catalog exposes one driver per adapter; legacy driver ordinal is unavailable");
    }
};
SourceD3D12DeviceProduction::SourceD3D12DeviceProduction(
    FrontendState &f, SessionWorldMaterials &m, SessionOuterFrameServices &frames, SourceMenuPresentationBackend &p,
    SourceApplicationLiteralData &app, PolygonSubmissionState &scratch, const HWND &window,
    std::unique_ptr<Renderer> &renderer, RenderOptions options, PolygonSubmissionCaptureLimits limits,
    LightGammaD3D12 &gamma, LightGammaDevice *&source_gamma, NewHostD3D12ApplicationSurfaces &lifecycle,
    NewHostD3D12WindowStartup &startup, SourceDrawStartupFailure &failure)
    : impl_(std::make_unique<Impl>(Impl{f, m, frames, p, app, scratch, window, renderer, options, limits, gamma,
                                        source_gamma, lifecycle, startup, failure}))
{
    require(source_gamma == nullptr, "cold source gamma publication must start at PE null");
}
SourceD3D12DeviceProduction::~SourceD3D12DeviceProduction() = default;
void SourceD3D12DeviceProduction::enumerate_adapters_42f6a0(FrontendState &f)
{
    auto &s = *impl_;
    s.check(f);
    require(!s.surfaces_created, "cannot enumerate over live source device resources");
    s.adapters.clear();
    s.selected.reset();
    s.enumerated = false;
    // NEW DXGI counterpart of source catalog clear + callback enumeration.
    // Keeps real native identities, never writes DXGI pointers into retail bytes.
    s.adapters = Renderer::enumerate_d3d12_adapters(s.options.warp);
    require(!s.adapters.empty(), "no adapter supports the requested D3D12 transport");
    s.enumerated = true;
}
void SourceD3D12DeviceProduction::create_surfaces_42fd80(FrontendState &f, std::uint32_t adapter)
{
    auto &s = *impl_;
    s.check(f);
    require(s.enumerated && !s.surfaces_created, "42FD80 requires fresh enumeration");
    require(adapter < s.adapters.size(), "source adapter ordinal has no enumerated D3D12 counterpart");
    require(s.window != nullptr, "42FD80 requires the existing App window");
    require(!s.renderer, "42FD80 refuses a prematurely created or second Renderer");
    s.selected = adapter;
    s.options.adapter_luid = s.adapters[adapter].luid;
    s.creation_window = s.window; // Capture the actual constructor argument before native reentry.
    s.renderer = std::make_unique<Renderer>(s.creation_window, s.options); // ONE App slot, ONE existing window
    s.creation_renderer = s.renderer.get(); // Identity only, never an additional Renderer owner.
    s.actual_renderer();
    s.hardware = s.renderer->device_info();
    require(s.hardware.adapter.luid == s.adapters[adapter].luid, "created adapter differs from enumeration");
    constexpr auto sample = D3D12_FORMAT_SUPPORT1_TEXTURE2D | D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE;
    require((s.hardware.a1rgb5_support & sample) == sample, "selected GPU cannot sample native A1R5G5B5");
    require((s.hardware.rgba8_support & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) != 0 && s.hardware.color_target_present,
            "selected GPU has no actual RGBA8 color target");
    require((s.hardware.depth32_support & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL) != 0 && s.hardware.depth_target_present,
            "selected GPU has no actual D32 depth target");
    // alpha comparisons1..8 are implemented by polygon_d3d12 HLSL compare_alpha.
    s.alpha_comparisons = D3DPCMPCAPS_NEVER | D3DPCMPCAPS_LESS | D3DPCMPCAPS_EQUAL | D3DPCMPCAPS_LESSEQUAL |
                          D3DPCMPCAPS_GREATER | D3DPCMPCAPS_NOTEQUAL | D3DPCMPCAPS_GREATEREQUAL | D3DPCMPCAPS_ALWAYS;
    s.shade = D3DPSHADECAPS_COLORGOURAUDRGB;
    if (s.hardware.rgba8_support & D3D12_FORMAT_SUPPORT1_BLENDABLE)
        s.shade |= D3DPSHADECAPS_ALPHAGOURAUDBLEND;
    s.filter = D3DPTFILTERCAPS_NEAREST | D3DPTFILTERCAPS_LINEAR;
    // The translated shader has two texture/sampler registers. FL11+ is queried,
    // not a guessed original texture-unit count; admission caps it at implemented2.
    require(s.hardware.feature_level >= D3D_FEATURE_LEVEL_11_0,
            "native device is below the shader feature-level contract");
    s.texture_units = 2;
    s.blend_stages = 2;
    s.depth_kind = 1; // real D32 Z buffer, no W-buffer emulation
    s.context = std::make_unique<PolygonDeviceContext>(new_host_stage_policy());
    s.surface = std::make_unique<PolygonDeviceSurface>(std::nullopt);
    s.surface_receiver = s.surface.get();
    s.backend = std::make_unique<MaterialD3D12>(*s.renderer, s.transparency, true, 1);
    s.loading = std::make_unique<RendererLoadingSurfaces>(*s.renderer, *s.context, s.recipes);
    // 42FD80's memory result: actual selected DXGI adapter memory, narrowed only
    // for the original32-bit budget word; no static fake high-memory profile.
    s.application.mutable_model_memory_budget_477c30() = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(s.hardware.adapter.dedicated_video_bytes, std::numeric_limits<std::uint32_t>::max()));
    f.world().viewport.depth_bits = std::bit_cast<std::int32_t>(f.world().boot_config.display_depth);
    initialize_viewport(f.world().viewport, f.world().split_setting); // actual424AA0 child
    s.presentation.bind(*s.renderer, *s.context, *s.backend, *s.loading, s.gamma, s.limits);
    gpu_world::attach(*s.renderer, *s.context, s.presentation.sink()); // NEW host rendering (gpu_world R19)
    s.lifecycle.attach_gpu(*s.renderer, *s.context, *s.backend, *s.loading);
    s.startup.admit_gpu(*s.renderer, *s.context, *s.loading);
    s.surfaces_created = true;
}
void SourceD3D12DeviceProduction::create_depth_42ff20(FrontendState &f, std::uint32_t a, std::uint32_t d)
{
    auto &s = *impl_;
    s.check(f);
    s.selected_pair(a, d);
    require(s.surfaces_created, "depth production before surfaces");
    s.actual_renderer();
    auto current = s.renderer->device_info();
    require(current.depth_target_present, "actual Renderer depth target is absent");
    s.depth_created = true; // NEW binding of existing D32 resource, no duplicate allocation
}
void SourceD3D12DeviceProduction::attach_depth_42fff0(FrontendState &f)
{
    auto &s = *impl_;
    s.check(f);
    require(s.depth_created && bool(s.loading), "depth attachment lacks actual surfaces");
    s.loading->require_binding(*s.renderer, *s.context);
    s.depth_attached = true;
}
std::uint32_t SourceD3D12DeviceProduction::create_device_430020(FrontendState &f, std::uint32_t a, std::uint32_t d)
{
    auto &s = *impl_;
    s.check(f);
    s.selected_pair(a, d);
    require(s.depth_attached, "device selection before depth attachment");
    s.actual_renderer();
    s.selected_device = s.context.get();
    s.frames.bind_source_depth(SelectedDeviceDepthBorrow(s.depth_kind));
    s.device_created = true;
    return 1;
}
void SourceD3D12DeviceProduction::publish_selected_device_4313d5(FrontendState &f)
{
    auto &s = *impl_;
    s.check(f);
    require(s.device_created && s.selected_device, "selected device is not produced");
    s.global_device = s.selected_device;
}
std::uint32_t SourceD3D12DeviceProduction::create_viewport_42f840(FrontendState &f)
{
    auto &s = *impl_;
    s.check(f);
    require(s.global_device, "viewport production before global-device publication");
    const auto &source = f.world().viewport;
    require(source.width > 0 && source.height > 0, "source viewport has invalid extent");
    D3DVIEWPORT2 view{};
    view.dwSize = sizeof(view);
    view.dwWidth = static_cast<DWORD>(source.width);
    view.dwHeight = static_cast<DWORD>(source.height);
    view.dvClipX = -1.0f;
    view.dvClipY = 1.0f;
    view.dvClipWidth = 2.0f;
    view.dvClipHeight = 2.0f;
    view.dvMinZ = 0.0f;
    view.dvMaxZ = 1.0f;
    s.viewport = view;
    return 1; // retained CPU source viewport; Renderer consumes f.world().viewport per draw
}
void SourceD3D12DeviceProduction::create_white_material_430ff0(FrontendState &f)
{
    auto &s = *impl_;
    s.check(f);
    require(s.viewport.has_value(), "white material production before viewport");
    D3DMATERIAL material{};
    material.dwSize = sizeof(material);
    material.diffuse = {1, 1, 1, 1};
    material.ambient = {1, 1, 1, 1};
    material.dwRampSize = 16;
    s.white_material = material; // NEW represented material; transformed FVF carries final diffuse/specular
}
DeviceSetupWorld SourceD3D12DeviceProduction::device_setup_world(FrontendState &f)
{
    auto &s = *impl_;
    s.check(f);
    require(s.white_material.has_value() && s.global_device, "device setup has unfinished production");
    return {{s.alpha_comparisons, s.shade, s.filter, s.texture_units, s.blend_stages, s.selected_device},
            session_outer_device_setup_globals(f.world()),
            s.global_device,
            s.surface_receiver};
}
void SourceD3D12DeviceProduction::query_gamma_432070(FrontendState &f)
{
    auto &s = *impl_;
    s.check(f);
    require(s.global_device && s.white_material.has_value(), "gamma publication before device setup");
    s.actual_renderer();
    (void)s.gamma.snapshot();
    require(s.source_gamma == nullptr || s.source_gamma == &s.gamma, "foreign source gamma publication");
    s.source_gamma = &s.gamma; // same real window-local SDR gamma receiver
}
void SourceD3D12DeviceProduction::driver_restrictions_42f220(FrontendState &f)
{
    auto &s = *impl_;
    s.check(f);
    require(s.source_gamma == &s.gamma, "driver identification before gamma publication");
    s.actual_renderer();
    auto description = s.renderer->device_info().adapter.description;
    for (char &ch : description)
        if (ch >= 'A' && ch <= 'Z')
            ch = static_cast<char>(ch + ('a' - 'A')); // 42F2E0 ASCII lower
    const auto vendor = s.hardware.adapter.vendor_id;
    const bool restricted = vendor == 0x1002
                                ? description.find("radeon") != std::string::npos
                                : vendor == 0x8086 && (description.find("intel(r) 82740") != std::string::npos ||
                                                       description.find("intel(r) 82810") != std::string::npos ||
                                                       description.find("intel(r) 82815") != std::string::npos);
    if (restricted)
    {
        f.world().view_quality.legacy_driver_restriction = 1;
        s.scratch.specular_enabled = 0;
    }
    s.driver_checked = true;
}
void SourceD3D12DeviceProduction::fatal_431330(FrontendState &f)
{
    auto &s = *impl_;
    s.check(f);
    s.failure.exit_44e4a0("D3D12 source device initialization failed");
}
SourceRendererResetContext SourceD3D12DeviceProduction::reset_context(FrontendState &f)
{
    auto &s = *impl_;
    s.check(f);
    require(s.driver_checked && bool(s.backend), "reset borrow before completed device production");
    return {f, s.frames, *s.backend, s.presentation}; // pure borrow, never constructs/resets
}
PolygonDeviceContext &SourceD3D12DeviceProduction::device_context()
{
    auto &s = *impl_;
    s.check(s.f);
    require(s.global_device == s.context.get() && s.global_device, "source global device not published");
    return *s.context;
}
void SourceD3D12DeviceProduction::release_materials_431790()
{
    auto &s = *impl_;
    s.check(s.f);
    require(bool(s.backend), "431790 requires the produced material resource bank");
    s.backend->release_all(); // does not reset transparency, physical materials or source flags
}
void SourceD3D12DeviceProduction::new_host_release_gpu_bundle_for_shutdown()
{
    auto &s = *impl_;
    s.f.shared_platform()->require_owner_thread();
    if (s.gpu_released_)
        return;
    require(s.source_gamma == nullptr, "source gamma must be unpublished before GPU bundle shutdown");
    // Caller has stopped all callbacks/source borrowers. No restore/recovery here.
    // Keep every owned resource on a healthy-device timeout or other failure.
    if (s.renderer)
    {
        // Only terminal shutdown admits a cleared live App slot. The producer
        // must still match its original window AND actual current GPU borrowers;
        // HWND equality alone cannot identify a Renderer or prevent handle reuse.
        require(s.creation_window && s.renderer->native_window() == s.creation_window,
                "shutdown Renderer belongs to a foreign creation HWND");
        require(!s.window || s.window == s.creation_window, "shutdown App HWND was replaced");
        if (s.loading)
        {
            require(bool(s.context), "shutdown loading surfaces lost their polygon context");
            s.loading->require_binding(*s.renderer, *s.context); // Identity only, no live-HWND/health gate.
        }
        else if (s.backend)
        {
            // Loading follows backend construction, before any source material upload.
            require(s.backend->bound_to_renderer(*s.renderer), "shutdown material bank belongs to another Renderer");
        }
        else
        {
            require(s.renderer.get() == s.creation_renderer, "shutdown initial Renderer owner was replaced");
        }
        try
        {
            s.renderer->finish();
        }
        catch (...)
        {
            if (SUCCEEDED(s.renderer->host_state().device_status))
                throw;
        }
    }
    else
    {
        require(!s.loading && !s.backend && !s.context, "App Renderer was destroyed before its GPU bundle");
    }
    // Only queue retirement or confirmed removal permits releasing captured GPU owners.
    gpu_world::detach(); // NEW host rendering (gpu_world R19): before the GPU bundle is released
    s.lifecycle.detach_gpu_for_shutdown();
    s.presentation.detach_gpu_for_shutdown();
    s.loading.reset();
    for (auto &submission : s.recipes.submissions)
        submission.reset();
    s.backend.reset();
    s.surface_receiver = nullptr;
    s.surface.reset();
    s.selected_device = nullptr;
    s.global_device = nullptr;
    s.context.reset();
    s.viewport.reset();
    s.white_material.reset();
    s.gpu_released_ = true;
    // App still owns Renderer/HWND. Source words and CPU gamma/ramp remain untouched.
}
void SourceD3D12DeviceProduction::clear_gamma_publication_4320a0()
{
    auto &s = *impl_;
    s.check(s.f);
    require(s.source_gamma == nullptr || s.source_gamma == &s.gamma, "foreign source gamma during detach");
    s.source_gamma = nullptr; // sourceRelease translated to unpublication; CPU lifetime belongs to Main
}
LightGammaD3D12 &SourceD3D12DeviceProduction::published_gamma()
{
    auto &s = *impl_;
    s.check(s.f);
    require(s.source_gamma == &s.gamma, "source gamma not published");
    return s.gamma;
}
} // namespace sgn::retail
