#pragma once
#include "sgn/source_main_page_navigation.hpp"
#include "sgn/source_menu_draw_children.hpp"
namespace sgn::retail
{
struct SourceMenuTextDrawServices
{
    virtual ~SourceMenuTextDrawServices() = default;
    // Full43C980 event3, with EDI count and EBX descriptor captured on entry.
    virtual void draw_menu_text_event3(FrontendState &, std::uint32_t child, std::uint32_t captured_count,
                                       std::uint32_t captured_descriptor) = 0;
};
// Control/state branches, real widget owner and returned EAX. Draw remains a
// mandatory full branch binding; no blank/default rendering implementation.
std::uint32_t dispatch_frontend_menu_text_43c980(FrontendState &, std::uint32_t child, std::uint32_t event,
                                                 std::uint32_t argument, SourceNavigationPageServices &,
                                                 SourceMainPageServices &, SourceMenuTextDrawServices &);

class SourceMenuMainNavigationChildren : public SourceMenuPageDrawingChildren,
                                         public SourceMainPageServices,
                                         public SourceMenuTextDrawServices
{
    CampaignScoreMusic &music_;

  protected:
    virtual void remaining_page_event(FrontendState &, std::uint32_t page, std::uint32_t callback, std::uint32_t event,
                                      std::uint32_t argument) = 0;
    void other_page_event(FrontendState &, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) final;

  public:
    SourceMenuMainNavigationChildren(MenuKeyboardAcquisition &, Win32MenuDirectInputStartup &, CampaignScoreMusic &,
                                     SessionOuterFrameServices &, PolygonSubmissionSink &, RaceTextureReplacement &,
                                     std::uint8_t &);
    std::uint32_t menu_text_event(FrontendState &, std::uint32_t, std::uint32_t, std::uint32_t) final;
};
} // namespace sgn::retail
