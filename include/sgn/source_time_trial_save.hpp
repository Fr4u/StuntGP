#pragma once
#include "sgn/source_menu_page_selection.hpp"

namespace sgn::retail
{
// Complete phase11 of44D740:44DC2A..44DCC6, entered by the original phase
// dispatcher at44D755. Caller must dispatch this leaf only for phase11.
// Captures the actual61C1C0 gate before setting phase12. A rejected gate or
// failed compression returns with phase12 retained and without opening Save.
// Uses the existing frontend buffers and the world's actual ReplayBank.
// This leaf does not activate the incomplete44D740 dispatcher or race loop.
void advance_time_trial_save_phase11_44dc2a(FrontendState &, SourcePageFactoryServices &,
                                            SourceNavigationPageServices &, CampaignScoreMusic &);
} // namespace sgn::retail
