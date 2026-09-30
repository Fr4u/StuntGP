#pragma once
#include "sgn/source_main_page_navigation.hpp"
namespace sgn::retail
{
std::uint32_t create_frontend_credits_page_43e000(FrontendState &, std::uint32_t music_key,
                                                  SourceNavigationPageServices &);
void destroy_frontend_credits_page_4529c0(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_credits_page_4529e0(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                    SourceNavigationPageServices &, CampaignScoreMusic &,
                                                    SourceMainPageServices &);
} // namespace sgn::retail
