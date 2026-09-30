#include "sgn/new_host_d3d12_application_surfaces.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session.hpp"
#include "sgn/material_d3d12.hpp"
#include "sgn/gpu_world.hpp" //NEW host rendering (gpu_world R19)
#include <system_error>

namespace sgn
{
namespace
{
Renderer &required_renderer(const std::unique_ptr<Renderer> &owner)
{
    if (!owner)
        throw FormatError("D3D12 lifecycle requires the existing renderer owner slot");
    return *owner;
}
struct LifecycleScope
{
    bool &busy;
    explicit LifecycleScope(bool &value) : busy(value)
    {
        busy = true;
    }
    ~LifecycleScope()
    {
        busy = false;
    }
};
} // namespace
NewHostD3D12ApplicationSurfaces::NewHostD3D12ApplicationSurfaces(
    retail::FrontendState &frontend, retail::SourceWindowMessageWords words, std::uint32_t &mode,
    retail::NewHostNativeWindowBorrow native, std::unique_ptr<Renderer> &renderer,
    SourceMenuPresentationBackend &presentation, LightGammaD3D12 &gamma,
    retail::LightGammaDevice *const &source_gamma_5da1dc, const std::exception_ptr &native_failure)
    : frontend_(frontend), words_(words), mode_(mode), native_(native), renderer_owner_(renderer),
      presentation_(presentation), gamma_(gamma), source_gamma_5da1dc_(source_gamma_5da1dc),
      native_failure_(native_failure)
{
    require_owner();
    if (&mode_ != &words_.mode_61c364)
        throw FormatError("D3D12 mode writer and source dispatcher must borrow the same word");
    if (!native_.instance)
        throw FormatError("early lifecycle needs the actual application HINSTANCE");
    (void)gamma_.snapshot(); // same CPU gamma owner; no GPU, source write or reset
}
void NewHostD3D12ApplicationSurfaces::detach_gpu_for_shutdown()
{
    // Cleanup must remain available after a recorded native callback failure.
    frontend_.shared_platform()->require_owner_thread();
    if (lifecycle_busy_)
        throw FormatError("cannot detach lifecycle during an active GPU operation");
    gpu_.reset();
    restore_pending_ = false;
    shutdown_detached_ = true;
    // No source words, gamma publication, recovery, GPU calls or owner deletion.
}
void NewHostD3D12ApplicationSurfaces::require_owner() const
{
    frontend_.shared_platform()->require_owner_thread();
    if (shutdown_detached_)
        throw FormatError("D3D12 lifecycle was detached for shutdown");
    if (native_failure_)
        std::rethrow_exception(native_failure_);
    if (&frontend_.world() != &frontend_.shared_platform()->state())
        throw FormatError("D3D12 lifecycle frontend has another platform world");
}
HWND NewHostD3D12ApplicationSurfaces::require_native_window() const
{
    require_owner();
    const auto current = native_.current();
    const auto window = reinterpret_cast<HWND>(current.window);
    DWORD process{};
    if (!window || GetWindowThreadProcessId(window, &process) != GetCurrentThreadId() ||
        process != GetCurrentProcessId() || GetAncestor(window, GA_ROOT) != window ||
        static_cast<std::uintptr_t>(GetWindowLongPtrW(window, GWLP_HINSTANCE)) != current.instance)
        throw FormatError("native lifecycle operation requires the actual live local HWND/HINSTANCE");
    if (gpu_ && gpu_->window != window)
        throw FormatError("D3D12 lifecycle HWND changed after GPU admission");
    return window;
}
void NewHostD3D12ApplicationSurfaces::require_window_binding(retail::FrontendState &frontend,
                                                             retail::InputWindowHandles handles,
                                                             retail::SourceWindowMessageWords words) const
{
    require_owner();
    const auto current = native_.current();
    if (&frontend != &frontend_ || handles.window != current.window || handles.instance != current.instance ||
        &words.active_61c374 != &words_.active_61c374 || &words.mode_61c364 != &mode_ ||
        &words.character_62ddfc != &words_.character_62ddfc)
        throw FormatError("early window/dispatcher must share frontend, native handles and all source words");
    (void)require_native_window(); // no Renderer/material/surface checks
}
void NewHostD3D12ApplicationSurfaces::attach_gpu(Renderer &renderer, PolygonDeviceContext &context,
                                                 MaterialD3D12 &materials, RendererLoadingSurfaces &surfaces)
{
    require_owner();
    if (gpu_ || lifecycle_busy_ || presentation_.source_frame_active())
        throw FormatError("GPU admission requires one idle initial attachment");
    const auto window = require_native_window();
    if (renderer_owner_.get() != &renderer || renderer.native_window() != window)
        throw FormatError("GPU admission must use the existing App Renderer slot and live HWND");
    presentation_.require_binding(frontend_, renderer, context, materials, surfaces, gamma_);
    const auto actual = renderer.host_state();
    if (FAILED(actual.device_status))
        throw GraphicsError(actual.device_status, "GPU admission reached a removed device");
    if (!actual.targets_ready || actual.unfenced_submission)
        throw FormatError("GPU admission requires actual ready targets and fenced work");
    const auto options = renderer.creation_options();
    surfaces.enable_source_canvas_fit();
    gpu_.emplace(GpuBinding{context, materials, surfaces, window, options});
    // No source writes, gamma reset, restore, Present or replay on attachment.
    // An earlier restore request stays queued until a later real lifecycle call.
}
void NewHostD3D12ApplicationSurfaces::require_live_renderer() const
{
    require_owner();
    if (!gpu_)
        throw FormatError("GPU operation reached before actual42FD80 admission");
    auto &renderer = required_renderer(renderer_owner_);
    if (renderer.native_window() != require_native_window())
        throw FormatError("D3D12 lifecycle renderer changed HWND");
    presentation_.require_binding(frontend_, renderer, gpu_->context, gpu_->materials, gpu_->surfaces, gamma_);
}
void NewHostD3D12ApplicationSurfaces::require_binding(retail::FrontendState &frontend, Renderer &renderer,
                                                      PolygonDeviceContext &context, RendererLoadingSurfaces &surfaces,
                                                      retail::SourceWindowMessageWords words) const
{
    require_window_binding(frontend, native_.current(), words);
    if (!gpu_ || renderer_owner_.get() != &renderer || &context != &gpu_->context || &surfaces != &gpu_->surfaces)
        throw FormatError("D3D12 admission must share actual renderer, context and surfaces");
    require_live_renderer();
}
NewHostSurfaceStatus NewHostD3D12ApplicationSurfaces::status() const
{
    require_owner();
    if (!native_.current().window)
    {
        if (gpu_)
            throw FormatError("admitted GPU outlived its native HWND");
        return {NewHostSurfaceCondition::awaiting_window, std::nullopt, 0, 0};
    }
    const auto window = require_native_window();
    RECT client{};
    if (!GetClientRect(window, &client))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "D3D12 client extent");
    const auto width = client.right - client.left, height = client.bottom - client.top;
    if (width < 0 || height < 0)
        throw FormatError("D3D12 client extent is negative");
    const auto w = static_cast<unsigned>(width), h = static_cast<unsigned>(height);
    if (!gpu_)
        return {NewHostSurfaceCondition::awaiting_gpu, std::nullopt, w, h};
    if (!renderer_owner_)
    {
        if (!presentation_.renderer_suspended())
            throw FormatError("D3D12 renderer owner disappeared without CPU handoff");
        return {NewHostSurfaceCondition::renderer_suspended, std::nullopt, w, h};
    }
    require_live_renderer();
    const auto actual = renderer_owner_->host_state(); // real device, target and queue state
    if (actual.unfenced_submission)
        return {NewHostSurfaceCondition::unfenced_work, actual.device_status, w, h};
    if (FAILED(actual.device_status))
        return {NewHostSurfaceCondition::device_removed, actual.device_status, w, h};
    if (IsIconic(window) || w == 0 || h == 0)
        return {NewHostSurfaceCondition::minimized, actual.device_status, w, h};
    if (presentation_.source_frame_active())
        return {NewHostSurfaceCondition::frame_busy, actual.device_status, w, h};
    const auto &dimensions = renderer_owner_->stats();
    if (!actual.targets_ready || dimensions.width != w || dimensions.height != h ||
        gpu_->surfaces.needs_restore(*renderer_owner_))
        return {NewHostSurfaceCondition::resize_required, actual.device_status, w, h};
    return {NewHostSurfaceCondition::ready, actual.device_status, w, h};
}
std::optional<HRESULT> NewHostD3D12ApplicationSurfaces::last_restore_failure() const
{
    require_owner();
    return last_restore_failure_;
}
void NewHostD3D12ApplicationSurfaces::recreate_removed_renderer()
{
    if (renderer_owner_)
    {
        require_live_renderer();
        if (SUCCEEDED(renderer_owner_->host_state().device_status))
            throw FormatError("device recreation requires an actual removal result");
        gpu_->options = renderer_owner_->creation_options(); // includes actual selected adapter options
        presentation_.suspend_renderer_for_recreation();
        gpu_world::detach(); // NEW host rendering (gpu_world R19): before the old device goes away
        renderer_owner_.reset();
    }
    if (!gpu_ || !presentation_.renderer_suspended())
        throw FormatError("D3D12 device replacement lost its CPU handoff");
    auto candidate = std::make_unique<Renderer>(require_native_window(), gpu_->options);
    require_owner(); // native construction can synchronously dispatch messages
    if (candidate->native_window() != require_native_window())
        throw FormatError("native HWND changed while constructing replacement GPU");
    presentation_.bind_recreated_renderer(*candidate);
    gpu_world::attach(*candidate, gpu_->context, presentation_.sink()); // NEW gpu_world R19
    renderer_owner_ = std::move(candidate);                             // nonthrowing final owner publication
}
void NewHostD3D12ApplicationSurfaces::restore()
{
    require_owner();
    restore_pending_ = true;
    if (!gpu_ || lifecycle_busy_ || presentation_.source_frame_active())
        return;
    const auto before = status();
    if (IsIconic(require_native_window()) || before.client_width == 0 || before.client_height == 0)
    {
        window_mode(1);
        return;
    }
    LifecycleScope scope(lifecycle_busy_);
    try
    {
        if (before.condition == NewHostSurfaceCondition::unfenced_work)
            throw FormatError("D3D12 unfenced work cannot be reported as a restored source frame");
        if (before.condition == NewHostSurfaceCondition::device_removed ||
            before.condition == NewHostSurfaceCondition::renderer_suspended)
            recreate_removed_renderer();
        presentation_.resize_render_targets(before.client_width, before.client_height);
        (void)require_native_window(); // reread live HWND and native failure after reentry
        restore_pending_ = false;
        last_restore_failure_.reset();
    }
    catch (const GraphicsError &error)
    {
        last_restore_failure_ = error.result;
        window_mode(1); // source fallback; caller must not overwrite it later
    }
}
void NewHostD3D12ApplicationSurfaces::window_mode(std::uint32_t requested)
{
    require_owner();
    if (requested == 1)
    {
        if (mode_ != 0)
            return; // 44E4BF..44E4C6
        mode_ = 1;  // 44E4D4 before native operations, also before GPU exists
        restore_pending_ = true;
        (void)DrawMenuBar(require_native_window());
        // Reload App's HWND after each native child; a nested callback may
        // destroy it or record its exception. Never continue with an old HWND.
        (void)RedrawWindow(require_native_window(), nullptr, nullptr, 0x400u);
        (void)require_native_window();
        frontend_.world().outer.source_word_612994 = 0; // 44E538 ->44E490
        return;
    }
    mode_ = 0; // 44E541, all requested values other than1
    restore(); // before GPU: only defer host work, never manufacture readiness
    retail::initialize_frontend_lookup_tables(frontend_.source); // 44E550
    auto &fade = frontend_.world().light_fade;
    fade.last_gamma = 0.0f; // 44E555
    // Read the real publication AFTER restore/tables/cache-clear. CPU gamma
    // construction and GPU admission are not publication of source5DA1DC.
    auto *const published_gamma = source_gamma_5da1dc_;
    if (published_gamma && published_gamma != &gamma_)
        throw FormatError("source gamma publication belongs to another CPU gamma owner");
    retail::set_light_gamma(fade, 1.0f, published_gamma); // 44E564;4320C4..CC null guard precedes cache/ramp writes
    // If restore fell back to mode1, do not write mode0 again.
}
void NewHostD3D12ApplicationSurfaces::idle()
{
    require_owner();
    if (!gpu_)
    {
        restore_pending_ = true;
        return;
    } // explicit host gate; source words unchanged
    if (lifecycle_busy_)
        return;
    const auto current = status();
    if (current.condition == NewHostSurfaceCondition::frame_busy)
    {
        restore_pending_ = true;
        return;
    }
    if (current.condition == NewHostSurfaceCondition::minimized)
    {
        window_mode(1);
        return;
    }
    if (current.condition == NewHostSurfaceCondition::unfenced_work)
        throw FormatError("D3D12 idle reached unfenced submission work");
    if (current.condition == NewHostSurfaceCondition::device_removed ||
        current.condition == NewHostSurfaceCondition::renderer_suspended)
    {
        restore_pending_ = true;
        if (mode_ == 0)
        {
            window_mode(1);
            return;
        }
    }
    if (mode_ == 1)
    {
        if (words_.active_61c374 != 0)
            window_mode(0);
        return;
    }
    if (restore_pending_ || current.condition == NewHostSurfaceCondition::resize_required)
        restore();
}
} // namespace sgn
