#pragma once
#include "sgn/source_main_page_navigation.hpp"
#include "sgn/source_menu_page_draw.hpp"
#include "sgn/source_title_page.hpp"
namespace sgn::retail
{
// Shared actual standings child, also called by PageCampaignScore32.
void draw_frontend_campaign_standings_450e50(FrontendState &, std::uint32_t race, std::uint32_t maximum,
                                             std::uint32_t center_x, std::uint32_t center_y,
                                             SourceMenuPageDrawServices &);
// Selector14:44-byte source type12 PageCampaignMenu; two200-byte MenuText
// children and live campaign presentation. Selector7 remains a separate page.
std::uint32_t create_frontend_campaign_menu_page_43e000(FrontendState &, std::uint32_t key,
                                                        SourceNavigationPageServices &, SourceMainPageServices &);
void destroy_frontend_campaign_menu_page_450e10(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_campaign_menu_page_4512b0(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                          SourcePageFactoryServices &, SourceNavigationPageServices &,
                                                          CampaignScoreMusic &, SourceMainPageServices &,
                                                          SourceMenuPageDrawServices &);
} // namespace sgn::retail
