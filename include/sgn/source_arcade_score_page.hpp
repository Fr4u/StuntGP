#pragma once
#include "sgn/source_main_page_navigation.hpp"
#include "sgn/source_title_draw.hpp"
#include "sgn/source_menu_feedback.hpp"
namespace sgn::retail
{
// Factory selector31 is PageArcadeScore, object type36. The retained mode1
// producer supplies5FA5D8/5FA5DC/5FA69C in the existing frontend cold suffix.
std::uint32_t create_frontend_arcade_score_page_43e000(FrontendState &, std::uint32_t key,
                                                       SourceNavigationPageServices &, SourceMainPageServices &);
void destroy_frontend_arcade_score_page_459a20(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_arcade_score_page_459a40(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                         SourceNavigationPageServices &, CampaignScoreMusic &,
                                                         SourceMainPageServices &, SourceTitlePageServices &,
                                                         SourceFrontendDrawServices &, SourceMenuSampleReceiving &);
} // namespace sgn::retail
