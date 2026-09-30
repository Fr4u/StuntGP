#pragma once
#include "sgn/source_title_page.hpp"
namespace sgn::retail
{
// Full44C0A0, phases0..25. Pages use the actual factory/navigation and Start25
// performs the existing source scene transition; missing pages remain explicit.
void advance_frontend_campaign_mode_44c0a0(FrontendState &, SourcePageFactoryServices &, SourceNavigationPageServices &,
                                           CampaignScoreMusic &);
} // namespace sgn::retail
