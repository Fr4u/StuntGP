#pragma once
#include "sgn/retail_campaign_score_page.hpp"

namespace sgn::retail
{
// Mandatory synchronous source45F8F0 boundary. The production implementation
// must call start_audio_music on the existing shared audio owners, requested,1.
// An empty callback or a second MusicTrack owner is not an implementation.
struct CampaignScoreMusic
{
    virtual ~CampaignScoreMusic() = default;
    virtual void start(std::uint32_t requested, std::uint32_t loop) = 0;
};
struct CampaignScoreNavigationViews
{
    CampaignScorePageViews page;
    std::span<std::byte, 0x5C> navigation; // 5FF3E8..5FF443, sole frontend owner
    std::uint32_t requested_music;         // 43EEE0(44), source5FCB60
    std::uint32_t &last_music;             // source5FF4AC, updated after45F8F0 returns
};
// New read-only host preflight shared with the full campaign settlement.
void validate_campaign_score_navigation(CampaignScoreNavigationViews);
// Full43DFA0(ECX44,EDX32)->43E000/43EE40 in the actual campaign
// context (current page absent or PageGameLoop). Includes a mandatory real
// music call when its source selector changes. Qualified independently and
// composed with enabled45F8F0; seeCAMPAIGN-SCORE-PAGE-RECOVERY.md.
std::uint32_t navigate_to_campaign_score_43dfa0(CampaignScoreNavigationViews, CampaignScoreMusic &);
} // namespace sgn::retail
