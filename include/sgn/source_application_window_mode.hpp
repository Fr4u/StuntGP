#pragma once
#include "sgn/source_application_surfaces.hpp"
#include "sgn/source_application_display_mode.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_lighting.hpp"

namespace sgn::retail
{
// Implements the mode child on the same services used by idle/restore. Lower
// device operations remain mandatory, and must borrow the live window owner.
class SourceApplicationWindowModeServices : public SourceApplicationSurfaceServices
{
    FrontendState &frontend_;
    std::uint32_t &mode_61c364_;

  public:
    SourceApplicationWindowModeServices(FrontendState &frontend, std::uint32_t &mode)
        : frontend_(frontend), mode_61c364_(mode)
    {
    }
    void window_mode_44e4b0(std::uint32_t mode) final;

  protected:
    // Read61C380 at entry. Bind EDX=&61C368, stack=&61C3A0,640,480,8;
    // source destination slots are live references, not copied COM handles.
    void recreate_surface_422f30();
    virtual SourceDisplayModeWorld display_mode_world() = 0;
    virtual SourceDisplayModeInvocation &display_mode_invocation() = 0;
    // Read61C380 again after the preceding child; slot28 takes only this.
    virtual void device_slot28_61c380() = 0;
    virtual std::uintptr_t window_handle_61c398() = 0;
    // nullptr is only the actual absent5DA1DC receiver, not an unbound adapter.
    virtual LightGammaDevice *gamma_device_5da1dc() = 0;
};
} // namespace sgn::retail
