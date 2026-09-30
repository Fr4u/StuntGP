#pragma once
#include "sgn/source_main_page_navigation.hpp"
#include "sgn/source_title_page.hpp"
namespace sgn::retail
{
// Factory37 creates source type32. Same page, MenuText, language and draw owners.
std::uint32_t create_frontend_bmp_splash_page_43e000(FrontendState &, std::uint32_t music_key,
                                                     SourceNavigationPageServices &);
void destroy_frontend_bmp_splash_page_45b5b0(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_bmp_splash_page_45b5d0(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                       SourceNavigationPageServices &, CampaignScoreMusic &,
                                                       SourceMainPageServices &, SourceTitlePageServices &);
} // namespace sgn::retail
