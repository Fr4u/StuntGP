#pragma once
#include "sgn/source_main_page_navigation.hpp"
#include "sgn/source_menu_page_draw.hpp"
#include "sgn/source_title_page.hpp"
namespace sgn::retail
{
// Complete factory selector28 / source type27,60-byte PageSlot and two200-byte
// MenuText children. Borrow the existing frontend bank and actual page services.
std::uint32_t create_frontend_campaign_slot_page_43e000(FrontendState &, std::uint32_t key,
                                                        SourceNavigationPageServices &, SourceMainPageServices &);
void destroy_frontend_campaign_slot_page_45ab70(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_campaign_slot_page_45abb0(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                          SourceNavigationPageServices &, CampaignScoreMusic &,
                                                          SourceMainPageServices &, SourceMenuPageDrawServices &,
                                                          SourceTitlePageServices &);
} // namespace sgn::retail
