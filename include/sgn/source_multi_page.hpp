#pragma once
#include "sgn/source_main_page_navigation.hpp"

namespace sgn::retail
{
// Factory selector6 stores source PageMulti type4; both use the existing page pool.
std::uint32_t create_frontend_multi_page_43e000(FrontendState &, std::uint32_t music_key,
                                                SourceNavigationPageServices &, SourceMainPageServices &);
void destroy_frontend_multi_page_4542a0(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_multi_page_4542c0(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                  SourcePageFactoryServices &, SourceNavigationPageServices &,
                                                  CampaignScoreMusic &, SourceMainPageServices &);
} // namespace sgn::retail
