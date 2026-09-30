#pragma once
#include "sgn/source_menu_page_selection.hpp"

namespace sgn::retail
{
// Actual44D740 phase4 and phase5 only. Phase4 may publish2 (track cancelled)
// or5 and push Start25; phase5 consumes actual retained61C1F0 then publishes6.
// Every other phase is an explicit missing-body error, not a successful no-op.
// Not connected to SourceMainMenuChildren::run_mode by this packet.
void advance_frontend_time_trial_phases4_5_44d740(FrontendState &, SourcePageFactoryServices &,
                                                  SourceNavigationPageServices &, CampaignScoreMusic &);
} // namespace sgn::retail
