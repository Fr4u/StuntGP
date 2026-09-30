#pragma once
#include "sgn/source_main_page_navigation.hpp"
namespace sgn::retail
{
std::uint32_t create_frontend_audio_options_43e000(FrontendState &, std::uint32_t music_key,
                                                   SourceNavigationPageServices &, SourceMainPageServices &);
void destroy_frontend_audio_options_455410(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_audio_options_455430(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                     SourceNavigationPageServices &, CampaignScoreMusic &,
                                                     SourceMainPageServices &);
// Only the audio branch of454DA0, after its existing child16/26 calls.
// False means another Options selection, which remains with its own handler.
bool route_frontend_audio_option_454e2a(FrontendState &, std::uint32_t selection, SourcePageFactoryServices &,
                                        SourceNavigationPageServices &, CampaignScoreMusic &);
} // namespace sgn::retail
