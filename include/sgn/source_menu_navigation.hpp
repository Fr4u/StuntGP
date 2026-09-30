#pragma once
#include "sgn/source_menu_input.hpp"

namespace sgn::retail
{
// Page identities are original 32-bit identities, never native function pointers.
// Their existing owner/dispatcher must resolve the two source vtable methods.
struct SourceNavigationPageServices
{
    virtual ~SourceNavigationPageServices() = default;
    virtual void feedback_440880(FrontendState &, std::uint32_t) = 0;
    virtual std::uint32_t select_page_43ef10(FrontendState &, std::uint32_t, std::uint32_t) = 0;
    virtual void update_cursor_43f610(FrontendState &) = 0;
    virtual void dispatch_navigation_43f310(FrontendState &) = 0;
    virtual void close_page(FrontendState &, std::uint32_t object) = 0; // object+0C
    virtual void page_event(FrontendState &, std::uint32_t object, std::uint32_t event,
                            std::uint32_t argument) = 0; // object+10, ECX/EDX/stack
};
void update_frontend_words_436ed0(FrontendState &);
void update_frontend_navigation_43eff0(FrontendState &, SourceNavigationPageServices &);
// PC53 arithmetic with explicit binary32 stores, source round-nearest domain.
// This value API does not emulate the x87 exception/status register.
float approach_frontend_value_43d810(float current, float target, float minimum_step) noexcept;

class SourceMenuNavigationChildren : public SourceMenuInputChildren, public SourceNavigationPageServices
{
  public:
    using SourceMenuInputChildren::SourceMenuInputChildren;
    void update_words_436ed0(FrontendState &) final;
    void update_navigation_43eff0(FrontendState &) final;
};
} // namespace sgn::retail
