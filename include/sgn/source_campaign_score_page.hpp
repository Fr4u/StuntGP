#pragma once
#include "sgn/source_main_page_navigation.hpp"
#include "sgn/source_menu_page_draw.hpp"
namespace sgn::retail
{
// Selector32,24-byte/type37 PageCampaignScore, source45A360/45A3F0/45A410.
std::uint32_t create_frontend_campaign_score_page_43e000(FrontendState &, std::uint32_t key,
                                                         SourceNavigationPageServices &, SourceMainPageServices &);
void destroy_frontend_campaign_score_page_45a3f0(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_campaign_score_page_45a410(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                           SourceNavigationPageServices &, CampaignScoreMusic &,
                                                           SourceMainPageServices &, SourceMenuPageDrawServices &);
} // namespace sgn::retail
