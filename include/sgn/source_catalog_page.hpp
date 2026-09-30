#pragma once
#include "sgn/source_main_page_navigation.hpp"
#include "sgn/source_title_page.hpp"

namespace sgn::retail
{
std::uint32_t create_frontend_catalog_page_43e000(FrontendState &, std::uint32_t music_key,
                                                  SourceNavigationPageServices &, SourceMainPageServices &);
void destroy_frontend_catalog_page_452530(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_catalog_page_452550(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                    SourceNavigationPageServices &, CampaignScoreMusic &,
                                                    SourceMainPageServices &, SourceTitlePageServices &);
} // namespace sgn::retail
