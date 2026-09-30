#include "sgn/source_application_final_shutdown.hpp"
#include "sgn/source_menu_shutdown.hpp"
#include "sgn/source_d3d12_device_production.hpp"
#include "sgn/source_application_surfaces_late_bound.hpp"
#include "sgn/race_session.hpp"
#include "sgn/renderer.hpp"
namespace sgn::retail
{
SourceApplicationFinalShutdown::SourceApplicationFinalShutdown(FrontendState &f, SourceNavigationPageServices &pages,
                                                               SourceD3D12DeviceProduction &graphics,
                                                               SourceLateBoundApplicationSurfaces &surfaces,
                                                               HWND &window, std::unique_ptr<Renderer> &renderer)
    : frontend_(f), pages_(&pages), graphics_(graphics), surfaces_(surfaces), window_(window), renderer_(renderer)
{
}
SourceApplicationFinalShutdown::SourceApplicationFinalShutdown(FrontendState &f, SourceD3D12DeviceProduction &graphics,
                                                               SourceLateBoundApplicationSurfaces &surfaces,
                                                               HWND &window, std::unique_ptr<Renderer> &renderer)
    : frontend_(f), graphics_(graphics), surfaces_(surfaces), window_(window), renderer_(renderer)
{
}
void SourceApplicationFinalShutdown::attach_pages(SourceNavigationPageServices &pages)
{
    require_owner();
    if (pages_)
        throw FormatError("application shutdown pages are already attached");
    pages_ = &pages; // Borrow only; Main passes the actual Core menu, never a proxy.
}
SourceNavigationPageServices &SourceApplicationFinalShutdown::require_pages() const
{
    require_owner();
    if (!pages_)
        throw FormatError("43D180 lacks the actual menu navigation attachment");
    return *pages_;
}
SourceApplicationFinalShutdown::~SourceApplicationFinalShutdown() = default; // No implicit source shutdown.
void SourceApplicationFinalShutdown::require_owner() const
{
    const auto platform = frontend_.shared_platform();
    platform->require_owner_thread();
    if (&platform->state() != &frontend_.world())
        throw FormatError("application shutdown belongs to another world");
}
void SourceApplicationFinalShutdown::bind_input(InputWindowHandles handles)
{
    require_owner();
    if (input_)
        throw FormatError("application shutdown input is already bound");
    if (!window_ || handles.window != reinterpret_cast<std::uintptr_t>(window_))
        throw FormatError("application shutdown requires its actual original HWND");
    // The getter performs the live HWND/raw/HINSTANCE check now, before destruction.
    auto &owner = frontend_.shared_platform()->window_input(handles.instance, handles.window);
    input_handles_ = handles;
    input_ = &owner;
}
void SourceApplicationFinalShutdown::close_menu_43d180()
{
    close_frontend_menu_43d180(frontend_, require_pages());
}
void SourceApplicationFinalShutdown::release_graphics_resources_431800()
{
    require_owner();
    graphics_.clear_gamma_publication_4320a0(); // Same Main-owned gamma slot, NEW CPU-owner translation.
    graphics_.release_materials_431790(); // Existing real MaterialD3D12 resource bank, no reset of physical table.
}
void SourceApplicationFinalShutdown::release_display_422a60()
{
    require_owner();
    // NEW HOST terminal lifetime, distinct from original DirectDraw422A60.
    // External frame/source entry must already be retired by the App. The
    // surface bridge can no longer invoke a lifecycle with destroyed borrowers.
    surfaces_.require_frontend(frontend_);
    surfaces_.detach();
    graphics_.new_host_release_gpu_bundle_for_shutdown(); // Retire work, detach borrowers, release GPU owners.
    renderer_.reset();                                    // Only after all Production GPU borrowers have detached.
    const auto window = window_;
    if (window)
    {
        // Original callback plumbing remains App-owned. WM_DESTROY must still
        // receive its source/default processing, with GPU entry already retired.
        if (!::DestroyWindow(window))
            throw FormatError("NEW HOST final DestroyWindow failed");
        window_ = nullptr;
    }
}
void SourceApplicationFinalShutdown::shutdown_input_424930()
{
    require_owner();
    if (!input_)
        throw FormatError("424930 lacks the original platform input binding");
    // Do NOT use platform.window_input() or SessionWindowInput here: their
    // lookup correctly admits only live HWNDs, whereas422A60 already destroyed it.
    input_->shutdown_424930(frontend_.world().source_input.acquisition, input_handles_);
}
} // namespace sgn::retail
