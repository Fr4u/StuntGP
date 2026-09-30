#pragma once
#include "sgn/source_menu_page_selection.hpp"

namespace sgn::retail
{
struct SourceMainPageServices
{
    virtual ~SourceMainPageServices() = default;
    // Actual child+10 callback including returned EAX, unlike page_event(void).
    virtual std::uint32_t menu_text_event(FrontendState &, std::uint32_t object, std::uint32_t event,
                                          std::uint32_t argument) = 0;
    virtual void background_4392b0(FrontendState &, std::uint32_t ecx, std::uint32_t edx) = 0;
    virtual void feedback_440890(FrontendState &, std::uint32_t) = 0;
    // Required nonreturning source CRT exit path. No direct exit() substitute.
    virtual void exit_462dfa(FrontendState &, std::uint32_t status) = 0;
};
void open_frontend_main_exit_453430(FrontendState &, std::uint32_t page, SourceMainPageServices &);
void close_frontend_main_exit_453460(FrontendState &, std::uint32_t page, SourceMainPageServices &);
std::uint32_t dispatch_frontend_main_page_453210(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                 std::uint32_t argument, SourcePageFactoryServices &,
                                                 SourceNavigationPageServices &, CampaignScoreMusic &,
                                                 SourceMainPageServices &);
} // namespace sgn::retail
