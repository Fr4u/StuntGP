#pragma once
#include "sgn/source_main_page_navigation.hpp"
#include "sgn/source_title_page.hpp"
#include "sgn/source_menu_page_draw.hpp"
namespace sgn::retail
{
// Selector13, object32 bytes/type11. Factory43E5D3, init45B7A0.
std::uint32_t create_frontend_select_team_page_43e000(FrontendState &, std::uint32_t key,
                                                      SourceNavigationPageServices &, SourceMainPageServices &);
void destroy_frontend_select_team_page_45b850(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_select_team_page_45b870(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                        SourceNavigationPageServices &, CampaignScoreMusic &,
                                                        SourceMainPageServices &, SourceTitlePageServices &,
                                                        SourceMenuPageDrawServices &);
} // namespace sgn::retail
