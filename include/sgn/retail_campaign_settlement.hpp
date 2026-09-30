#pragma once
#include "sgn/retail_championship.hpp"
#include "sgn/retail_campaign_presentation.hpp"
#include "sgn/retail_campaign_score_navigation.hpp"
#include "sgn/retail_frontend_language.hpp"

namespace sgn::retail
{
struct CampaignSettlementPageViews
{
    CampaignSettlementViews settlement;
    CampaignPresentationViews presentation;
    std::span<std::byte, 0xB0> menu;       // 5FCF0C..5FCFBB; action5FCF08 is navigation.page.action
    std::span<std::byte, 260> points_text; // 61B9C8
    std::span<std::byte, 260> reward_text; // 61BDCC
    FrontendLanguageStrings language;
    CampaignScoreNavigationViews navigation;
};
// Full44C0A0 for input progression10 only. Actual abort publishes24. Successful
// settlement completes44BD00,441170,46280C and43DFA0 (including real enabled
// music) before44C6F8 publishes11. Other campaign phases are not this entry.
// Views borrow the existing owners. Language pointer slots use Audio's typed
// arena-offset+1 identities; fixed points/reward buffer identities remain
// source61B9C8/61BDCC and must be resolved by the frontend, never dereferenced.
void settle_campaign_race_44c0a0(CampaignSettlementPageViews, CampaignScoreMusic &);
} // namespace sgn::retail
