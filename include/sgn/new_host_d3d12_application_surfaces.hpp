#pragma once
#include "sgn/source_application_window_d3d12_operations.hpp"
#include "sgn/source_application_window_start_d3d12.hpp"
#include "sgn/source_menu_presentation_backend.hpp"
#include <exception>
#include <memory>
#include <optional>

namespace sgn
{
enum class NewHostSurfaceCondition
{
    awaiting_window,
    awaiting_gpu,
    ready,
    minimized,
    frame_busy,
    resize_required,
    device_removed,
    renderer_suspended,
    unfenced_work
};
struct NewHostSurfaceStatus
{
    NewHostSurfaceCondition condition;
    std::optional<HRESULT> device_status; // absent before GPU creation or during suspension
    unsigned client_width, client_height; // no client exists in awaiting_window
};
// Stable NEW HOST endpoint before CreateWindow. Source words/gamma have their
// existing CPU owners; GPU borrowers attach only at actual42FD80 production.
// No DirectDraw COM slots, fabricated resources/caps, or source reload/replay.
class NewHostD3D12ApplicationSurfaces final : public retail::NewHostD3D12WindowOperations
{
    retail::FrontendState &frontend_;
    retail::SourceWindowMessageWords words_;
    std::uint32_t &mode_;
    retail::NewHostNativeWindowBorrow native_; // aliases the actual App HWND slot
    std::unique_ptr<Renderer> &renderer_owner_;
    SourceMenuPresentationBackend &presentation_;
    LightGammaD3D12 &gamma_;
    retail::LightGammaDevice *const &source_gamma_5da1dc_; // sole live publication slot, borrowed read-only
    const std::exception_ptr &native_failure_;
    struct GpuBinding
    {
        PolygonDeviceContext &context;
        MaterialD3D12 &materials;
        RendererLoadingSurfaces &surfaces;
        HWND window;
        RenderOptions options;
    };
    std::optional<GpuBinding> gpu_;             // actual borrowed owners, not source readiness
    bool shutdown_detached_{};                  // terminal host lifetime, never a source word
    bool lifecycle_busy_{}, restore_pending_{}; // host scheduling only
    std::optional<HRESULT> last_restore_failure_;
    void require_owner() const;
    HWND require_native_window() const;
    void require_live_renderer() const;
    void recreate_removed_renderer();

  public:
    NewHostD3D12ApplicationSurfaces(retail::FrontendState &, retail::SourceWindowMessageWords,
                                    std::uint32_t &mutable_mode, retail::NewHostNativeWindowBorrow,
                                    std::unique_ptr<Renderer> &, SourceMenuPresentationBackend &, LightGammaD3D12 &,
                                    retail::LightGammaDevice *const &source_gamma_5da1dc,
                                    const std::exception_ptr &native_failure);
    NewHostD3D12ApplicationSurfaces(const NewHostD3D12ApplicationSurfaces &) = delete;
    NewHostD3D12ApplicationSurfaces &operator=(const NewHostD3D12ApplicationSurfaces &) = delete;
    void require_window_binding(retail::FrontendState &, retail::InputWindowHandles,
                                retail::SourceWindowMessageWords) const override;
    void attach_gpu(Renderer &, PolygonDeviceContext &, MaterialD3D12 &, RendererLoadingSurfaces &);
    void detach_gpu_for_shutdown(); // after GPU barrier/removal and callback detachment
    void require_binding(retail::FrontendState &, Renderer &, PolygonDeviceContext &, RendererLoadingSurfaces &,
                         retail::SourceWindowMessageWords) const override;
    NewHostSurfaceStatus status() const;
    std::optional<HRESULT> last_restore_failure() const;
    void idle() override;
    void restore() override;
    void window_mode(std::uint32_t) override;
};
} // namespace sgn
