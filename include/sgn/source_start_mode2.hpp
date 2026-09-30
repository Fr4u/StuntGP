#pragma once
#include "sgn/source_menu_page_selection.hpp"
#include "sgn/retail_frontend_clock.hpp"
namespace sgn::retail
{
struct SelectionRecord;
void prepare_frontend_stock_configuration_43a430(FrontendState &, SelectionRecord &, std::uint32_t model,
                                                 std::uint32_t livery);
void prepare_frontend_mode2_race_43a780(FrontendState &, std::uint32_t source_roster);
void request_frontend_save_page_43d500(FrontendState &, std::uint32_t key, SourcePageFactoryServices &,
                                       SourceNavigationPageServices &, CampaignScoreMusic &);
std::uint32_t dispatch_frontend_start_page_452b60(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                  SourceNavigationPageServices &, CampaignScoreMusic &,
                                                  FrontendPerformanceCounter &);
} // namespace sgn::retail
