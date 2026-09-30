#pragma once
#include "sgn/retail_championship.hpp"

namespace sgn::retail
{
struct CampaignBetweenRaceViews
{
    CampaignLive live;
    std::span<const std::byte, 0xC0> standings;
    std::span<const std::byte, 0x10> checkpoints;
    std::uint32_t last_normal_race; // borrowed value 5F943C
    std::uint32_t &progression;     // sole frontend 5E3794
    std::uint32_t &menu_action;     // sole frontend 5F9A9C
};
// Complete original 44C0A0 entry-to-return paths for states 12,13,17,20.
// State12 advances race then13 selects the checkpoint/finale route. State17
// enters21; state20 chooses18 only for rank1, otherwise21. Every reached source
// callee is recovered (44CC90 and44CCF0). No page/save/launch call is elided.
// Other states require different source producers and are rejected before any
// mutation. The host must dispatch them to their own recovered handlers.
void advance_campaign_between_races_44c0a0(CampaignBetweenRaceViews);
} // namespace sgn::retail
