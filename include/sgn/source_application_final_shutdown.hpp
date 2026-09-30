#pragma once
#include "sgn/source_application_shutdown.hpp"
#include "sgn/window_input_owner.hpp"
#include <Windows.h>
#include <memory>
namespace sgn
{
class Renderer;
}
namespace sgn::retail
{
class SourceD3D12DeviceProduction;
class SourceLateBoundApplicationSurfaces;
struct SourceNavigationPageServices;
// Concrete composition for the one NEW HOST D3D12 application. No owner is
// created/copied here. Construct before HWND/GPU; bind actual input after attach.
// The app must stop external source/draw entries before ordinary442180; reached
// page destructors still run. This object never replaces the CRT exit boundary.
class SourceApplicationFinalShutdown final : public SourceApplicationShutdownServices
{
    FrontendState &frontend_;
    SourceNavigationPageServices *pages_{}; // borrowed actual navigation, attached once after menu construction
    SourceD3D12DeviceProduction &graphics_;
    SourceLateBoundApplicationSurfaces &surfaces_;
    HWND &window_;
    std::unique_ptr<Renderer> &renderer_;
    WindowInputOwner *input_{};          // borrow of platform's actual owner, never owned here
    InputWindowHandles input_handles_{}; // original native identities, survive HWND destruction
    void require_owner() const;
    SourceNavigationPageServices &require_pages() const;

  public:
    SourceApplicationFinalShutdown(FrontendState &, SourceNavigationPageServices &, SourceD3D12DeviceProduction &,
                                   SourceLateBoundApplicationSurfaces &, HWND &app_window,
                                   std::unique_ptr<Renderer> &app_renderer);
    // NEW HOST pre-menu composition; no proxy or page creation in this constructor.
    SourceApplicationFinalShutdown(FrontendState &, SourceD3D12DeviceProduction &, SourceLateBoundApplicationSurfaces &,
                                   HWND &app_window, std::unique_ptr<Renderer> &app_renderer);
    // Actual menu navigation from the same Core; one owner-thread publication.
    void attach_pages(SourceNavigationPageServices &);
    ~SourceApplicationFinalShutdown() override;
    SourceApplicationFinalShutdown(const SourceApplicationFinalShutdown &) = delete;
    SourceApplicationFinalShutdown &operator=(const SourceApplicationFinalShutdown &) = delete;
    // After real HWND/platform input attachment, before shutdown. No4249B0 here.
    void bind_input(InputWindowHandles original_handles);
    void close_menu_43d180() override;
    void release_graphics_resources_431800() override;
    // NEW HOST display teardown at this source call boundary. It does NOT claim
    // DirectDraw61C378/61C380/registry COM releases or change their source words.
    void release_display_422a60() override;
    void shutdown_input_424930() override;
};
} // namespace sgn::retail
