#pragma once
#include "sgn/source_main_page_navigation.hpp"
#include "sgn/source_title_draw.hpp"
#include "sgn/source_menu_page_draw.hpp"
namespace sgn::retail
{
// Selector7, PageModify, object88/type5. Uses the existing campaign and page pool.
std::uint32_t create_frontend_modify_page_43e000(FrontendState &, std::uint32_t, SourceNavigationPageServices &,
                                                 SourceMainPageServices &);
void destroy_frontend_modify_page_4536f0(FrontendState &, std::uint32_t);
std::uint32_t dispatch_frontend_modify_page_453710(FrontendState &, std::uint32_t, std::uint32_t,
                                                   SourceNavigationPageServices &, CampaignScoreMusic &,
                                                   SourceMainPageServices &, SourceTitlePageServices &,
                                                   SourceMenuPageDrawServices &, SourceFrontendDrawServices &);
} // namespace sgn::retail
