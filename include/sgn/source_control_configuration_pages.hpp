#pragma once
#include "sgn/source_main_page_navigation.hpp"
#include "sgn/source_menu_page_draw.hpp"
#include "sgn/source_title_page.hpp"

namespace sgn::retail
{
// Factory selector 16 stores source page type 14. Both MenuText children use
// the retained page pool and the literal PE rows in FrontendSourceStorage.
std::uint32_t create_frontend_configuration_page_43e000(FrontendState &, std::uint32_t music_key,
                                                        SourceNavigationPageServices &, SourceMainPageServices &);
void destroy_frontend_configuration_page_455090(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_configuration_page_4550d0(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                          SourcePageFactoryServices &, SourceNavigationPageServices &,
                                                          CampaignScoreMusic &, SourceMainPageServices &);

// Factory selector 19 stores source page type 17. Its close 4557F0 is RET.
// No secondary input bank: 424340 writes the existing upstream record +4.
std::uint32_t create_frontend_control_page_43e000(FrontendState &, std::uint32_t music_key);
void destroy_frontend_control_page_4557f0(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_control_page_455800(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                    SourceNavigationPageServices &, CampaignScoreMusic &,
                                                    SourceMainPageServices &, SourceTitlePageServices &,
                                                    SourceMenuPageDrawServices &);
} // namespace sgn::retail
