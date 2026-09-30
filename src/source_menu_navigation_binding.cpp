#include "sgn/source_menu_navigation_binding.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_frontend_page_pool.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <bit>
#include <algorithm>
namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_signed;
using sgn::retail::frontend_page_words::source_word;
std::uint8_t key(FrontendState &f, std::uint32_t player, std::uint32_t button, std::uint32_t previous = 0)
{
    // 436FC0/436FD0: current+0, previous+8 within the same24-byte row.
    if (player > 4 || button > 7)
        throw FormatError("frontend key outside source input rows");
    return f.source.source_bytes(0x5da2e8u + 24u * player + button + previous, 1)[0];
}
bool any_43f5e0(FrontendState &f, std::uint32_t player)
{
    for (std::uint32_t i = 1; i < 8; ++i)
        if (key(f, player, i))
            return true;
    return false;
}
bool admitted_437090(FrontendState &f, std::uint32_t player)
{
    if (player < 1 || player > 4)
        throw FormatError("frontend player outside physical source slots");
    const auto active = f.world().menu_input.raw_words[player - 1].word_0;
    if (active == 0)
        return false;
    return f.world().source_input.physical[player - 1].activity_kind - 1u <= 4u;
}
} // namespace
void dispatch_frontend_player_43f3e0(FrontendState &f, std::uint32_t page, std::uint32_t player,
                                     std::uint8_t &character, SourceNavigationPageServices &calls)
{
    const auto saved_character = character;
    character = 0;
    std::uint32_t pressed = 0;
    constexpr std::uint32_t buttons[]{7, 6, 5, 1, 2, 3, 4};
    constexpr std::uint32_t events[]{17, 16, 15, 11, 12, 13, 14};
    for (unsigned i = 0; i < 7; ++i)
        if (key(f, player, buttons[i]) && !key(f, player, buttons[i], 8))
        {
            pressed = events[i];
            break;
        }
    if (saved_character)
    {
        source_word(f, 0x5ff460) = 0;
        if (source_word(f, 0x5ff464))
            return;
        calls.page_event(
            f, page, 18,
            static_cast<std::uint32_t>(static_cast<std::int32_t>(std::bit_cast<std::int8_t>(saved_character))));
    }
    std::uint32_t held = 0;
    constexpr std::uint32_t held_buttons[]{7, 6, 1, 2, 3, 4};
    constexpr std::uint32_t held_events[]{10, 9, 4, 5, 6, 7};
    for (unsigned i = 0; i < 6; ++i)
        if (key(f, player, held_buttons[i]))
        {
            held = held_events[i];
            calls.page_event(f, page, held, player);
        }
    if (key(f, player, 5))
    {
        calls.page_event(f, page, 8, player);
        source_word(f, 0x5ff460) = 0;
    }
    else if (held || pressed)
        source_word(f, 0x5ff460) = 0;
    if (!source_word(f, 0x5ff464) && pressed)
        calls.page_event(f, page, pressed, player);
}
void dispatch_frontend_navigation_43f310(FrontendState &f, std::uint32_t page, std::uint8_t &character,
                                         SourceNavigationPageServices &calls)
{
    if (source_word(f, 0x5ff4b0))
    {
        const auto selected = source_word(f, 0x5ff4a0);
        if (selected)
        {
            if (any_43f5e0(f, selected))
                return;
        }
        else
            for (std::uint32_t i = 1; i < 5; ++i)
                if (any_43f5e0(f, i))
                    return;
        source_word(f, 0x5ff4b0) = 0;
    }
    const auto selected = source_word(f, 0x5ff4a0);
    if (selected)
    {
        if (admitted_437090(f, selected))
            dispatch_frontend_player_43f3e0(f, page, source_word(f, 0x5ff4a0), character, calls);
        else
            source_word(f, 0x5ff4a4) = 1;
    }
    else
    {
        unsigned admitted = 0;
        for (std::uint32_t i = 1; i < 5; ++i)
            if (admitted_437090(f, i))
            {
                dispatch_frontend_player_43f3e0(f, page, i, character, calls);
                ++admitted;
            }
        if (admitted == 0)
            source_word(f, 0x5ff4a4) = 1;
    }
    if (source_word(f, 0x5ff4a4) != source_word(f, 0x5ff4a8))
        calls.feedback_440880(f, source_word(f, 0x5ff4a4) ? 7u : 8u);
    source_word(f, 0x5ff4a8) = source_word(f, 0x5ff4a4);
}
void update_frontend_idle_43f610(FrontendState &f, SourceNavigationPageServices &calls)
{
    const auto page = source_word(f, 0x5ff3f0);
    if (page)
    {
        const auto object = frontend_page_object_bytes(std::as_writable_bytes(std::span{f.source.page_pool}), page, 8);
        std::uint32_t kind = 0;
        for (unsigned i = 0; i < 4; ++i)
            kind |= std::uint32_t(std::to_integer<std::uint8_t>(object[4 + i])) << (i * 8);
        // Pinned dispatch bytes43F690..43F6AB;1 selects no timeout.
        constexpr std::uint8_t skip[]{0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0,
                                      0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0};
        if (kind - 2u < 28u && !skip[kind - 2u] && as_signed(source_word(f, 0x5ff460)) > 3600)
        {
            calls.feedback_440880(f, 2);
            (void)calls.select_page_43ef10(f, 1, 1);
            [[maybe_unused]] const auto prior = source_word(f, 0x5ff464);
            source_word(f, 0x5ff460) = 0;
        }
    }
    const auto next = as_signed(source_word(f, 0x5ff460)) > 18000 ? source_word(f, 0x5ff464) + 1u : 0u;
    source_word(f, 0x5ff464) = next;
    source_word(f, 0x5ff464) = static_cast<std::uint32_t>(std::clamp(as_signed(next), 0, 70));
}
SourceMenuNavigationBindingChildren::SourceMenuNavigationBindingChildren(
    MenuKeyboardAcquisition &keys, Win32MenuDirectInputStartup &input, CampaignScoreMusic &music,
    SessionOuterFrameServices &frames, PolygonSubmissionSink &sink, RaceTextureReplacement &textures,
    std::uint8_t &character)
    : SourceMenuTextChildren(keys, input, music, frames, sink, textures), character_(character)
{
}
void SourceMenuNavigationBindingChildren::update_cursor_43f610(FrontendState &f)
{
    f.shared_platform()->require_owner_thread();
    update_frontend_idle_43f610(f, *this);
}
void SourceMenuNavigationBindingChildren::dispatch_navigation_43f310(FrontendState &f)
{
    f.shared_platform()->require_owner_thread();
    auto page = source_word(f, 0x5ff3f8);
    if (!page)
        page = source_word(f, 0x5ff3f0);
    dispatch_frontend_navigation_43f310(f, page, character_, *this);
}
} // namespace sgn::retail
