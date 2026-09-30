#pragma once
#include "sgn/source_main_page_navigation.hpp"
#include "sgn/source_menu_page_draw.hpp"
#include "sgn/source_title_page.hpp"
namespace sgn::retail
{
// Selector12, source type10:60-byte PageSelectTree and two200-byte MenuText
// children. Uses the live championship history, persistence and draw owners.
std::uint32_t create_frontend_select_tree_page_43e000(FrontendState &, std::uint32_t key,
                                                      SourceNavigationPageServices &, SourceMainPageServices &);
void destroy_frontend_select_tree_page_45d2e0(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_select_tree_page_45d320(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                        SourceNavigationPageServices &, CampaignScoreMusic &,
                                                        SourceMainPageServices &, SourceMenuPageDrawServices &,
                                                        SourceTitlePageServices &);
} // namespace sgn::retail
