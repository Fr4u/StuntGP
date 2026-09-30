#pragma once
#include "sgn/source_menu_page_selection.hpp"
namespace sgn::retail
{
struct SourceMainPageServices;
struct SourceTitlePageServices;
struct SourceMenuPageDrawServices;
std::uint32_t create_frontend_enter_name_page_43e000(FrontendState &, std::uint32_t key);
void destroy_frontend_enter_name_page_454450(FrontendState &, std::uint32_t page);
// Full454460 and all mandatory4547E0..454CD1 children, including drawing.
// Argument is the original event argument; event18 consumes only its low byte.
std::uint32_t dispatch_frontend_enter_name_page_454460(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                       std::uint32_t argument, SourceNavigationPageServices &,
                                                       CampaignScoreMusic &, SourceMainPageServices &,
                                                       SourceTitlePageServices &, SourceMenuPageDrawServices &);
} // namespace sgn::retail
