#include "sgn/retail_campaign_settlement.hpp"
#include "sgn/retail_campaign_award_text.hpp"
#include "sgn/little_endian_words.hpp"
#include <array>
#include <bit>
#include <stdexcept>
#include <string_view>

namespace sgn::retail
{
namespace
{
using sgn::little_endian::read_le32;
using sgn::little_endian::write_le32;
std::string_view format(FrontendLanguageStrings language, std::uint32_t id)
{
    const auto text = resolve_frontend_language_text(language, frontend_language_text_441170(language, id));
    if (text.empty())
        throw std::invalid_argument("campaign award format has a null source identity");
    return {reinterpret_cast<const char *>(text.data()), text.size() - 1};
}
} // namespace
void settle_campaign_race_44c0a0(CampaignSettlementPageViews view, CampaignScoreMusic &music)
{
    const auto &settlement = view.settlement;
    if (settlement.progression != 10)
        throw std::invalid_argument("campaign settlement requires source phase10");
    if (read_le32(settlement.results, 0) != 0)
    {
        (void)settle_campaign_race_prefix_44c0a0(settlement);
        return;
    }
    // New host preflight is read-only. Reuse the same validation as the actual
    // callees so a bad menu/language/configuration cannot partially award a race.
    if (settlement.live.data() != view.presentation.live.data() ||
        settlement.awards.data() != view.presentation.catalog.rules.data() + 0x5C ||
        settlement.last_normal_race != read_le32(view.presentation.catalog.rules, 0))
        throw std::invalid_argument("campaign settlement views do not borrow the same source owners");
    for (std::size_t row = 0; row < 6; ++row)
    {
        const auto place = read_le32(settlement.results, 4 + row * 20);
        if (place < 1 || place > 6)
            throw std::invalid_argument("campaign finish place outside source producer range");
    }
    validate_campaign_presentation(view.presentation);
    validate_campaign_score_navigation(view.navigation);
    const auto player_place = read_le32(settlement.results, 4);
    const auto title = frontend_language_text_441170(view.language, player_place + 0x27C);
    const auto continuation = frontend_language_text_441170(view.language, 0x26C);
    (void)resolve_frontend_language_text(view.language, title);
    (void)resolve_frontend_language_text(view.language, continuation);
    const auto points_format = format(view.language, 0x283), reward_format = format(view.language, 0x284);
    const bool bonus = std::bit_cast<std::int32_t>(read_le32(settlement.live, 8)) >
                       std::bit_cast<std::int32_t>(settlement.last_normal_race);
    const auto points = read_le32(settlement.awards, player_place * 16) * (bonus ? 3u : 1u);
    const auto reward =
        read_le32(settlement.awards, player_place * 16 + (bonus ? 12u : 4u)) + read_le32(settlement.results, 20);
    std::array<std::byte, 260> scratch{};
    (void)format_campaign_award_46280c(scratch, points_format, points);
    (void)format_campaign_award_46280c(scratch, reward_format, reward);

    (void)settle_campaign_race_prefix_44c0a0(settlement);
    build_campaign_presentation_44bd00(view.presentation);
    // 44C621..44C6EE. Literal words below are recovered source fields/identities,
    // not host pointers or new menu defaults. Preserve all unmentioned bytes.
    write_le32(view.menu, 0, 0x5F);
    write_le32(view.menu, 4, 0xFFFFFFFFu);
    write_le32(view.menu, 8, title.value);
    (void)format_campaign_award_46280c(view.points_text, points_format, points);
    write_le32(view.menu, 0xC, 0xFFFF8080u);
    write_le32(view.menu, 0x10, 0x61B9C8);
    (void)format_campaign_award_46280c(view.reward_text, reward_format, reward);
    write_le32(view.menu, 0x14, 0xFFFF8080u);
    write_le32(view.menu, 0x18, 0x61BDCC);
    write_le32(view.menu, 0x1C, 0xFFC8C8FFu);
    write_le32(view.menu, 0x20, continuation.value);
    write_le32(view.menu, 0xAC, settlement.last_normal_race);
    write_le32(view.navigation.page.action, 0, 0);
    write_le32(view.menu, 0xA4, 4);
    write_le32(view.menu, 0xA8, read_le32(settlement.live, 8));
    (void)navigate_to_campaign_score_43dfa0(view.navigation, music);
    // 44C6F8 is unconditional after the actual43DFA0 return, including its
    // invalid-navigation-phase return0. It is never an early prefix placeholder.
    settlement.progression = 11;
}
} // namespace sgn::retail
