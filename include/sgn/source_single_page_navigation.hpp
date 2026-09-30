#pragma once
#include "sgn/source_menu_text_navigation.hpp"
namespace sgn::retail
{
struct SourceGameLoopModeServices
{
    virtual ~SourceGameLoopModeServices() = default;
    // Required full selected state machine, retaining the actual PageGameLoop.
    virtual void run_mode(FrontendState &, std::uint32_t page, std::uint32_t source_entry) = 0;
};
std::uint32_t dispatch_frontend_single_page_45a810(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                   std::uint32_t argument, SourcePageFactoryServices &,
                                                   SourceNavigationPageServices &, CampaignScoreMusic &,
                                                   SourceMainPageServices &);
std::uint32_t dispatch_frontend_game_loop_452fe0(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                 SourceGameLoopModeServices &);
} // namespace sgn::retail
