#pragma once
#include "sgn/source_main_page_navigation.hpp"
#include "sgn/source_title_page.hpp"
namespace sgn::retail
{
// Factory selectors21,34,35,39; internal object types19,39,40,34 differ.
std::uint32_t create_frontend_records_page_43e000(FrontendState &, std::uint32_t key, std::uint32_t selector,
                                                  SourceNavigationPageServices &, SourceMainPageServices &);
void destroy_frontend_records_page_child(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_records_page(FrontendState &, std::uint32_t page, std::uint32_t callback,
                                             std::uint32_t event, SourcePageFactoryServices &,
                                             SourceNavigationPageServices &, CampaignScoreMusic &,
                                             SourceMainPageServices &, SourceTitlePageServices &);
} // namespace sgn::retail
