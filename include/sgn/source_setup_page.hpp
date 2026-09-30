#pragma once
#include "sgn/source_menu_page_selection.hpp"

namespace sgn::retail
{
// Factory selector5 creates internal page-kind23 (PageSetup), not kind5.
std::uint32_t create_frontend_setup_page_43e000(FrontendState &, std::uint32_t music_key);
// Complete45A630. Existing PageSave27 and PageLanguage29 children are invoked
// through the actual factory/navigation receiver; no successful stand-in.
std::uint32_t dispatch_frontend_setup_page_45a630(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                  SourcePageFactoryServices &, SourceNavigationPageServices &,
                                                  CampaignScoreMusic &);
} // namespace sgn::retail
