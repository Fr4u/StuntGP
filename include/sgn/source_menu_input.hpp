#pragma once
#include "sgn/source_menu_frame.hpp"
#include "sgn/menu_directinput_startup.hpp"

namespace sgn::retail
{
// Full 45F210 only. Borrow initialized input and the existing platform audio
// owner; no reset, device creation, vehicle dispatcher or feedback execution.
void acquire_frontend_input_45f210(FrontendState &, MenuKeyboardAcquisition &, Win32MenuDirectInputStartup &);
// Full 40D860 binding, called once for each due menu tick by 441F25.
void produce_frontend_menu_input_40d860(FrontendState &);

// Concrete input children for the existing menu parent. The application derives
// its remaining menu/audio/draw children here. These two cannot be overridden
// with callbacks which accidentally append 40DAD0 or fabricate another bank.
class SourceMenuInputChildren : public SourceMenuFrameChildren
{
    MenuKeyboardAcquisition &keyboard_;
    Win32MenuDirectInputStartup &direct_input_;

  public:
    SourceMenuInputChildren(MenuKeyboardAcquisition &keyboard, Win32MenuDirectInputStartup &direct_input) noexcept
        : keyboard_(keyboard), direct_input_(direct_input)
    {
    }
    SourceMenuInputChildren(const SourceMenuInputChildren &) = delete;
    SourceMenuInputChildren &operator=(const SourceMenuInputChildren &) = delete;
    void acquire_input_45f210(FrontendState &) final;
    void update_controls_40d860(FrontendState &) final;
    void apply_options_43c000(FrontendState &) final;
};
} // namespace sgn::retail
