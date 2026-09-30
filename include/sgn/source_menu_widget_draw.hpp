#pragma once
#include "sgn/source_menu_text_navigation.hpp"
namespace sgn::retail
{
void draw_frontend_menu_text_event3(FrontendState &, std::uint32_t child, std::uint32_t captured_count,
                                    std::uint32_t captured_descriptor, SourceFrontendDrawServices &);
// Full main-page call shape4392B0(62,0); nonzero formatting route remains explicit.
void draw_frontend_main_background_4392b0(FrontendState &, std::uint32_t ecx, std::uint32_t edx,
                                          SourceFrontendDrawServices &);
class SourceMenuWidgetDrawChildren : public SourceMenuMainNavigationChildren
{
  public:
    using SourceMenuMainNavigationChildren::SourceMenuMainNavigationChildren;
    void draw_menu_text_event3(FrontendState &, std::uint32_t, std::uint32_t, std::uint32_t) final;
    void background_4392b0(FrontendState &, std::uint32_t, std::uint32_t) final;
};
} // namespace sgn::retail
