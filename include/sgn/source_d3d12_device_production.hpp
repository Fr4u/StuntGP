#pragma once
#include "sgn/source_renderer_bootstrap.hpp"
#include "sgn/source_application_draw_start_win32.hpp"
#include <memory>
namespace sgn
{
class NewHostD3D12ApplicationSurfaces;
}
namespace sgn::retail
{
class NewHostD3D12WindowStartup;
// NEW host transport for the recovered431360 sequence. Owns one GPU binding
// bundle; borrows the existing App HWND/Renderer slot and the single CPU gamma.
// No DirectDraw window, synthetic COM address or second source world exists.
class SourceD3D12DeviceProduction final : public SourceRendererBootstrapServices
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    SourceD3D12DeviceProduction(FrontendState &, SessionWorldMaterials &, SessionOuterFrameServices &,
                                SourceMenuPresentationBackend &, SourceApplicationLiteralData &,
                                PolygonSubmissionState &, const HWND &app_window,
                                std::unique_ptr<Renderer> &app_renderer, RenderOptions, PolygonSubmissionCaptureLimits,
                                LightGammaD3D12 &, LightGammaDevice *&source_gamma_5da1dc,
                                NewHostD3D12ApplicationSurfaces &, NewHostD3D12WindowStartup &,
                                SourceDrawStartupFailure &);
    ~SourceD3D12DeviceProduction() override;
    SourceD3D12DeviceProduction(const SourceD3D12DeviceProduction &) = delete;
    SourceD3D12DeviceProduction &operator=(const SourceD3D12DeviceProduction &) = delete;
    PolygonDeviceContext &device_context();
    void release_materials_431790();                 // existing source resource-bank release only
    void new_host_release_gpu_bundle_for_shutdown(); // barrier then terminal detach; App retains Renderer/HWND
    void clear_gamma_publication_4320a0();           // NEW detach of source slot; retains CPU owner/ramp
    LightGammaD3D12 &published_gamma();              // only after432070, SAME early CPU owner
    void enumerate_adapters_42f6a0(FrontendState &) override;
    void create_surfaces_42fd80(FrontendState &, std::uint32_t) override;
    void create_depth_42ff20(FrontendState &, std::uint32_t, std::uint32_t) override;
    void attach_depth_42fff0(FrontendState &) override;
    std::uint32_t create_device_430020(FrontendState &, std::uint32_t, std::uint32_t) override;
    void publish_selected_device_4313d5(FrontendState &) override;
    std::uint32_t create_viewport_42f840(FrontendState &) override;
    void create_white_material_430ff0(FrontendState &) override;
    DeviceSetupWorld device_setup_world(FrontendState &) override;
    void query_gamma_432070(FrontendState &) override;
    void driver_restrictions_42f220(FrontendState &) override;
    void fatal_431330(FrontendState &) override;
    SourceRendererResetContext reset_context(FrontendState &) override;
};
} // namespace sgn::retail
