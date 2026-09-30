#pragma once
#include "sgn/source_main_page_navigation.hpp"
#include "sgn/source_title_page.hpp"
namespace sgn::retail
{
// Selector40 is PageUnlock (internal type35), as named by PE479864.
// TimeTrial phases6/8 push it with music key43; no new caller payload.
std::uint32_t create_frontend_unlock_page_43e000(FrontendState &, std::uint32_t key, SourceNavigationPageServices &,
                                                 SourceMainPageServices &);
void destroy_frontend_unlock_page_45dde0(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_unlock_page_45de00(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                   SourceNavigationPageServices &, CampaignScoreMusic &,
                                                   SourceMainPageServices &, SourceTitlePageServices &);
} // namespace sgn::retail
