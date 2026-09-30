#pragma once
#include "sgn/source_main_page_navigation.hpp"
namespace sgn::retail
{
// Factory kind18 is PageOptionsVID; actual page field+4 is16, not factory kind16 CFG.
std::uint32_t create_frontend_video_options_page_43e000(FrontendState &, std::uint32_t music_key,
                                                        SourceNavigationPageServices &);
void destroy_frontend_video_options_page_455690(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_video_options_page_4556b0(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                          SourcePageFactoryServices &, SourceNavigationPageServices &,
                                                          CampaignScoreMusic &, SourceMainPageServices &);
} // namespace sgn::retail
