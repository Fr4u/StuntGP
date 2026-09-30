#pragma once
#include "sgn/retail_frontend_page_pool.hpp"

namespace sgn::retail
{
struct CampaignScorePageViews
{
    FrontendPagePool pool;
    std::span<std::byte, 4> action; // 5FCF08, borrowed from frontend cold suffix
};
// 43E000 with actual EDX=0x20, including full45A360 and real43C860/900/930,
// 43C980 events0x18 and0x16 for pinned descriptor488E38. Returns source identity.
// The factory epilogue sets page+8 to music_key. Navigation43DFA0/43EE40 and
// enabled45F8F0 are separate operations and are not bypassed by this constructor.
std::uint32_t create_campaign_score_page_43e000(CampaignScorePageViews, std::uint32_t music_key);
void initialize_campaign_score_page_45a360(CampaignScorePageViews, std::uint32_t page);
// Full45A3F0 releases only the child MenuText, then zeros page+0x14. Its caller
// owns release of the PageCampaignScore object itself.
void destroy_campaign_score_children_45a3f0(FrontendPagePool, std::uint32_t page);
} // namespace sgn::retail
