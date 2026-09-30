#pragma once
#include "sgn/source_menu_text.hpp"
#include "sgn/source_application_window_messages.hpp"
namespace sgn::retail
{
void update_frontend_idle_43f610(FrontendState &, SourceNavigationPageServices &);
void dispatch_frontend_navigation_43f310(FrontendState &, std::uint32_t page, std::uint8_t &character,
                                         SourceNavigationPageServices &);
void dispatch_frontend_player_43f3e0(FrontendState &, std::uint32_t page, std::uint32_t player, std::uint8_t &character,
                                     SourceNavigationPageServices &);
class SourceMenuNavigationBindingChildren : public SourceMenuTextChildren
{
    std::uint8_t &character_; // same retained slot passed to SourceWindowMessageWords
  public:
    SourceMenuNavigationBindingChildren(MenuKeyboardAcquisition &, Win32MenuDirectInputStartup &, CampaignScoreMusic &,
                                        SessionOuterFrameServices &, PolygonSubmissionSink &, RaceTextureReplacement &,
                                        std::uint8_t &character);
    // Borrow the SAME character byte consumed by43F310; no second queue.
    SourceWindowMessageWords window_message_words(std::uint32_t &active, const std::uint32_t &mode) const noexcept
    {
        return {active, mode, character_};
    }
    void update_cursor_43f610(FrontendState &) final;
    void dispatch_navigation_43f310(FrontendState &) final;
};
} // namespace sgn::retail
