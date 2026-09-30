#include "sgn/retail_championship_transition.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
std::uint32_t race_index(CampaignLive live)
{
    std::uint32_t result = 0;
    for (unsigned i = 0; i < 4; ++i)
        result |= std::to_integer<std::uint32_t>(live[8 + i]) << (i * 8);
    return result;
}
using sgn::bit_reinterpret::as_signed;
} // namespace
void advance_campaign_between_races_44c0a0(CampaignBetweenRaceViews view)
{
    switch (view.progression)
    {
    case 12: {
        // 44C729..44C744, INC wraps before the next tick's signed comparisons.
        const auto next = race_index(view.live) + 1u;
        for (unsigned i = 0; i < 4; ++i)
            view.live[8 + i] = static_cast<std::byte>((next >> (i * 8)) & 255u);
        view.progression = 13;
        return;
    }
    case 13: {
        // 44C745..44C7A4, including full 44CC90 at 44C770.
        const auto race = race_index(view.live);
        if (as_signed(race) > as_signed(view.last_normal_race))
        {
            view.progression = as_signed(race) <= as_signed(view.last_normal_race + 1u) ? 19u : 17u;
        }
        else if (campaign_checkpoint_44cc90(view.checkpoints, race) != 0)
        {
            view.progression = 14;
        }
        else
        {
            view.menu_action = 1;
            view.progression = 7;
        }
        return;
    }
    case 17:
        view.progression = 21;
        return;
    case 20:
        view.progression = campaign_player_rank_44ccf0(view.standings) == 1 ? 18u : 21u;
        return;
    default:
        throw std::invalid_argument("campaign phase needs a different recovered handler");
    }
}
} // namespace sgn::retail
