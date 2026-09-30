#pragma once
#include "sgn/source_main_page_navigation.hpp"
namespace sgn::retail
{
// Factorycase15, source page field+4 is13. Same24B page and200B MenuText pool.
std::uint32_t create_frontend_options_page_43e000(FrontendState &, std::uint32_t music_key,
                                                  SourceNavigationPageServices &);
void destroy_frontend_options_page_454d80(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_options_page_454da0(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                    SourcePageFactoryServices &, SourceNavigationPageServices &,
                                                    CampaignScoreMusic &, SourceMainPageServices &);
} // namespace sgn::retail
