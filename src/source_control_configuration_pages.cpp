#include "sgn/source_control_configuration_pages.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_text_splash_page.hpp"
#include "sgn/source_menu_text.hpp"
#include "sgn/source_menu_apply_options.hpp"
#include "sgn/source_frontend_initialization.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/retail_frontend_initialization.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using sgn::retail::frontend_page_words::page_pool;
using sgn::retail::frontend_page_words::page_word;
using sgn::retail::frontend_page_words::set_page_word;
void clear(FrontendState &f, W object, W size)
{
    for (W offset = 0; offset < size; offset += 4u)
        set_page_word(f, object, offset, 0);
}
using sgn::retail::frontend_page_words::source_word;
W setting(FrontendState &f, W address)
{
    return persistence_word(f.source.persistence.bytes, address - 0x5df560u);
}
void setting(FrontendState &f, W address, W value)
{
    persistence_word(f.source.persistence.bytes, address - 0x5df560u, value);
}
W float_bits(double value)
{
    return std::bit_cast<W>(vehicle_f32(value));
}
using sgn::bit_reinterpret::as_float;
void appearance(FrontendState &f)
{
    source_word(f, 0x5ff46c) = 1; // 4400A0
    set_frontend_packed_word_43dd10(f.source, 0x114311);
    source_word(f, 0x5ff448) = 99;
    source_word(f, 0x5ff44c) = 0x3e99999a;
    source_word(f, 0x5ff450) = 0xbf000000;
}
W configuration_child(FrontendState &f, bool confirmation, SourceNavigationPageServices &pages)
{
    const auto child = allocate_frontend_page_object_43d470(page_pool(f), 0xc8);
    W close = 0;
    if (confirmation)
        close = source_word(f, 0x4794e8); // 455024, before clear of SECOND child
    clear(f, child, 0xc8);
    if (!confirmation)
        close = source_word(f, 0x4794e8); // 454FBC, after first clear
    const auto event = source_word(f, 0x4794ec);
    set_page_word(f, child, 12, close);
    if (!confirmation)
        set_page_word(f, child, 16, event);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, confirmation ? 0x4884d4u : 0x4884c8u);
    set_page_word(f, child, 8, 0);
    if (confirmation)
        set_page_word(f, child, 16, event);
    if (initialize_frontend_menu_text_43c860(f, child, confirmation ? 0x488420u : 0x488490u, pages) != 0)
        return child;
    release_frontend_page_object_43d4c0(page_pool(f), child);
    return 0;
}
W initialize_configuration_454fa0(FrontendState &f, W page, SourceNavigationPageServices &pages,
                                  SourceMainPageServices &calls)
{
    set_page_word(f, page, 0x14, configuration_child(f, false, pages));
    const auto first = page_word(f, page, 0x14); // same ESI across both actual callbacks
    (void)calls.menu_text_event(f, first, 24, 0);
    (void)calls.menu_text_event(f, first, 22, 0);
    set_page_word(f, page, 0x1c, 0);
    set_page_word(f, page, 0x18, configuration_child(f, true, pages));
    (void)calls.menu_text_event(f, page_word(f, page, 0x18), 24, 0);
    return 1;
}
void close_child(FrontendState &f, W child)
{
    if (page_word(f, child, 12) != 0x43c970u)
        throw FormatError("455090 reached an unreconstructed MenuText close identity");
    // The complete 43C970 child is RET; the caller owns the following free.
    release_frontend_page_object_43d4c0(page_pool(f), child);
}
void open_confirmation_4552f0(FrontendState &f, W page, SourceMainPageServices &calls)
{
    const auto child = page_word(f, page, 0x18);
    (void)calls.menu_text_event(f, child, 19, 0);
    (void)calls.menu_text_event(f, child, 28, 2);
    set_page_word(f, page, 0x1c, 1);
}
void close_confirmation_455320(FrontendState &f, W page, SourceMainPageServices &calls)
{
    (void)calls.menu_text_event(f, page_word(f, page, 0x18), 20, 0);
    set_page_word(f, page, 0x1c, 0);
}
W input_kind_437090(FrontendState &f, W player)
{
    if (player < 1u || player > 4u)
        throw FormatError("control page reached input outside its four actual physical records");
    const auto slot = player - 1u;
    if (f.world().menu_input.raw_words[slot].word_0 == 0)
        return 0;
    const auto kind = f.world().source_input.physical[slot].activity_kind;
    return kind - 1u <= 4u ? kind : 0u;
}
W profile_437050(FrontendState &f, W player)
{
    (void)input_kind_437090(f, player);
    return 0; // complete pinned 437050, not a default profile
}
W feedback_kind_437010(FrontendState &f, W player)
{
    if (input_kind_437090(f, player) == 0)
        return 0;
    const auto mode = f.world().source_input.physical[player - 1u].feedback_mode; // 45ED60
    return mode == 1u || mode == 2u ? mode : 0u;
}
bool edge_436fd0(FrontendState &f, W player, W button)
{
    const auto address = 0x5da2e8u + player * 24u + button;
    if (f.source.source_bytes(address, 1)[0] == 0)
        return false;
    return f.source.source_bytes(address + 8u, 1)[0] == 0;
}
W set_input_kind_437130(FrontendState &f, W player, W selection)
{
    if (input_kind_437090(f, player) == 0)
        return 0;
    // 4371A8 selects 424340 for 1..4; selection5 takes the zero-return arm.
    // The page reaches only1..4, but retain the complete selector return map.
    if (selection == 5u)
        return 0;
    if (selection >= 1u && selection <= 4u)
    {
        const auto slot = player - 1u;
        // 424353 re-reads actual physical +0, independently of 437090.
        if (f.world().menu_input.raw_words[slot].word_0 != 0)
        {
            constexpr W source_kind[]{3, 2, 6, 5};
            // R25: clang-format off keeps the address comment on one line with its statement (no reflow).
            // clang-format off
            f.world().source_input.acquisition.upstream[slot].words[1] = source_kind[selection - 1u]; // 57661C + slot*88
            // clang-format on
        }
    }
    return 1;
}
void change_kind_455c20(FrontendState &f, W player, W direction, SourceNavigationPageServices &pages)
{
    const auto old = input_kind_437090(f, player);
    if (old == 0 || old == 5)
        return;
    auto next = old + direction;
    if (std::bit_cast<std::int32_t>(next) <= 0)
        next = 4;
    else if (std::bit_cast<std::int32_t>(next) >= 5)
        next = 1;
    if (set_input_kind_437130(f, player, next) != 0)
        pages.feedback_440880(f, 1);
}
void update_controls_455af0(FrontendState &f, SourceNavigationPageServices &pages)
{
    for (W player = 1; player < 5; ++player)
    {
        // 455B90 short-circuits these four source reads in this exact order.
        if (!(edge_436fd0(f, player, 3) || edge_436fd0(f, player, 4) || edge_436fd0(f, player, 1) ||
              edge_436fd0(f, player, 2)))
            continue;
        const auto blocked = source_word(f, 0x5ff464);
        source_word(f, 0x5ff460) = 0; // 43DD70 read BEFORE clear
        if (blocked != 0)
            continue;
        // 455BE0 calls full 437050, whose EAX is always zero. Its change and
        // sound branches are provably unreachable; retain the reached read.
        if (edge_436fd0(f, player, 3))
            (void)profile_437050(f, player);
        if (edge_436fd0(f, player, 4))
            (void)profile_437050(f, player);
        if (edge_436fd0(f, player, 1))
            change_kind_455c20(f, player, 0xffffffffu, pages);
        if (edge_436fd0(f, player, 2))
            change_kind_455c20(f, player, 1, pages);
    }
}
void persist_controls_4558c0(FrontendState &f)
{
    for (W at = 0x5df608; at < 0x5df638; at += 12u)
    {
        setting(f, at - 4u, 0);
        setting(f, at, 0);
        setting(f, at + 4u, 0);
    }
    configure_frontend_inputs_43dc40(f); // source tail, after all twelve stores
}
void single_text_438be0(FrontendState &f, W x, W y, W id, W color)
{
    const auto identity = frontend_language_text_441170(f.source.language_strings(), id);
    const auto text = resolve_frontend_language_text(f.source.language_strings(), identity);
    queue_frontend_text_438990(f.source, 12, 1, x, y, 4, text, 0x3fa00000, color, 3, 0);
}
void draw_controls_4558e0(FrontendState &f, SourceTitlePageServices &draw, SourceMenuPageDrawServices &panels)
{
    // 4558EE calls signed min(4,4); the source draws all four physical slots.
    for (W player = 1; player < 5; ++player)
    {
        const auto kind = input_kind_437090(f, player);
        const auto profile = profile_437050(f, player); // independent second 437090 call
        const auto x = float_bits((double(player - 1u) - 1.5) * 150.0 + 320.0);
        const auto variant = profile != 0 ? profile - 1u : 0u;
        const auto sprite = kind - 1u <= 4u ? 0x171u + (kind - 1u) * 3u + variant : 0u; // 43DCC0
        const SourceMenuRect rect{float_bits(double(as_float(x)) - 64.0), 0x42c00000,
                                  float_bits(double(as_float(x)) + 64.0), 0x43740000};
        panels.frame_439450(f, 10, 111, rect, 0x40);
        panels.fill_439380(f, 13, 0, rect, 0, 0);
        draw.sprite_438710(f, 12, sprite != 0 ? sprite : 0x170u, {x, 0x432a0000, 0x3f4ccccd, 3});
        const auto color =
            sprite != 0 ? ((source_word(f, 0x5ff45c) & 0x20u) != 0 ? 0xffffffffu : 0xffff8080u) : 0xff808080u;
        single_text_438be0(f, x, 0x42dc0000, player + 0xc1u, color);
        const auto feedback = feedback_kind_437010(f, player); // called even for absent sprite
        if (sprite != 0 && feedback != 0)
            single_text_438be0(f, x, 0x43660000, feedback + 0x2c7u, color);
    }
    // The source local array also contains 26F, but the actual pointer starts
    // at its second DWORD: the three reached language IDs are 270,273,272.
    std::array<W, 6> rows;
    rows[0] = 0xffffffff;
    rows[1] = frontend_language_text_441170(f.source.language_strings(), 0x270).value;
    rows[2] = 0xffffffff;
    rows[3] = frontend_language_text_441170(f.source.language_strings(), 0x273).value;
    rows[4] = 0xffffffff;
    rows[5] = frontend_language_text_441170(f.source.language_strings(), 0x272).value;
    draw_frontend_text_panel_45be20(f, {rows, 0}, 3, {0x43a00000, 0x43a28000, 0x44070000, 0}, panels);
}
} // namespace
W create_frontend_configuration_page_43e000(FrontendState &f, W key, SourceNavigationPageServices &pages,
                                            SourceMainPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 0x20);
    const auto event = source_word(f, 0x4884c4);
    clear(f, page, 0x20);
    const auto close = source_word(f, 0x4884c0);
    set_page_word(f, page, 4, 14);
    set_page_word(f, page, 12, close);
    set_page_word(f, page, 16, event);
    set_page_word(f, page, 0, 0x47978c);
    set_page_word(f, page, 8, 0);
    if (initialize_configuration_454fa0(f, page, pages, calls) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), page);
        return 0;
    }
    set_page_word(f, page, 8, key);
    return page;
}
void destroy_frontend_configuration_page_455090(FrontendState &f, W page)
{
    f.shared_platform()->require_owner_thread();
    close_child(f, page_word(f, page, 0x18));
    const auto first = page_word(f, page, 0x14);
    set_page_word(f, page, 0x18, 0); // 4550A3 BEFORE 4550A6
    close_child(f, first);
    set_page_word(f, page, 0x14, 0);
}
W dispatch_frontend_configuration_page_4550d0(FrontendState &f, W page, W event, SourcePageFactoryServices &factory,
                                              SourceNavigationPageServices &pages, CampaignScoreMusic &music,
                                              SourceMainPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 19:
        appearance(f);
        (void)calls.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        break;
    case 16: {
        if (page_word(f, page, 0x1c) != 0)
        {
            const auto child = page_word(f, page, 0x18);
            (void)calls.menu_text_event(f, child, event, 0);
            const auto selected = calls.menu_text_event(f, child, 26, 0);
            if (selected == 1)
            {
                close_confirmation_455320(f, page, calls);
                break;
            }
            const auto language = setting(f, 0x5df5f0); // 45515F, only in confirmed branch
            initialize_frontend_defaults_43c060(f.source, f.world().frontend_launch);
            initialize_source_unlocks_43c370(f);
            initialize_source_records_43c130(f);
            apply_source_menu_options_43c000(f);
            setting(f, 0x5df5f0, language);
            close_confirmation_455320(f, page, calls);
            (void)pop_frontend_page_43eef0(f, pages, music);
            break;
        }
        const auto child = page_word(f, page, 0x14);
        (void)calls.menu_text_event(f, child, event, 0);
        const auto selected = calls.menu_text_event(f, child, 26, 0);
        if (selected == 0)
            setting(f, 0x5df5d8, setting(f, 0x5df5d8) ^ 1u);
        else if (selected == 1 || selected == 2)
        {
            source_word(f, 0x5e3c28) = selected == 1 ? 2u : 3u;
            source_word(f, 0x5e3c1c) = 0;
            source_word(f, 0x5e3c20) = 0;
            source_word(f, 0x5e3c24) = 0;
            (void)push_frontend_page_43dfa0(f, 0, 27, factory, pages, music);
        }
        else if (selected == 3)
        {
            open_confirmation_4552f0(f, page, calls);
            // 455225 forwards original EDI (16), not a fabricated event20.
            (void)calls.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        }
        break;
    }
    case 17:
        if (page_word(f, page, 0x1c) != 0)
        {
            close_confirmation_455320(f, page, calls);
            calls.feedback_440890(f, event);
        }
        else if (pop_frontend_page_43eef0(f, pages, music) != 0)
            calls.feedback_440890(f, event);
        break;
    case 3:
        calls.background_4392b0(f, 74, 0);
        [[fallthrough]];
    case 2:
        (void)calls.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        (void)calls.menu_text_event(f, page_word(f, page, 0x18), event, 0);
        break;
    case 11:
    case 12:
        (void)calls.menu_text_event(f, page_word(f, page, page_word(f, page, 0x1c) == 0 ? 0x14u : 0x18u), event, 0);
        break;
    case 20:
        (void)calls.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        break;
    default:
        break;
    }
    return 0;
}
W create_frontend_control_page_43e000(FrontendState &f, W key)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 0x18);
    const auto event = source_word(f, 0x488614);
    clear(f, page, 0x18);
    const auto close = source_word(f, 0x488610);
    set_page_word(f, page, 4, 17);
    set_page_word(f, page, 12, close);
    set_page_word(f, page, 16, event);
    set_page_word(f, page, 0, 0x4797bc);
    set_page_word(f, page, 8, 0);
    configure_frontend_inputs_43dc40(f); // full4557E0 returns1 after this child
    set_page_word(f, page, 8, key);
    return page;
}
void destroy_frontend_control_page_4557f0(FrontendState &, W) {} // complete source RET
W dispatch_frontend_control_page_455800(FrontendState &f, W, W event, SourceNavigationPageServices &pages,
                                        CampaignScoreMusic &music, SourceMainPageServices &calls,
                                        SourceTitlePageServices &draw, SourceMenuPageDrawServices &panels)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 1:
        update_controls_455af0(f, pages);
        break;
    case 3:
        calls.background_4392b0(f, 83, 0);
        draw.text_438ff0(f, 2, 4,
                         {0x43a00000, 0x43d20000, 85, 0x3fa00000, 0xffffffff, 4, 0, 0x3f800000, 0x41, 0xfffffe70, 0});
        draw_controls_4558e0(f, draw, panels);
        break;
    case 16:
    case 17:
        calls.feedback_440890(f, event);
        (void)pop_frontend_page_43eef0(f, pages, music);
        persist_controls_4558c0(f);
        break;
    case 19:
        appearance(f);
        break;
    default:
        break;
    }
    return 0;
}
} // namespace sgn::retail
