#include "sgn/source_select_team_page.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_menu_text.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/retail_campaign_presentation.hpp"
#include "sgn/retail_frontend_clip.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <bit>

namespace sgn::retail
{
extern "C" void sgn_select_team_index_45b831(const std::uint32_t *, std::uint32_t *);
extern "C" void sgn_select_team_strip_45ba34(const std::uint32_t *, std::uint32_t *);
extern "C" void sgn_select_team_next_45ba89(const std::uint32_t *, std::uint32_t *);
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_signed;
using sgn::retail::frontend_page_words::page_pool;
using sgn::retail::frontend_page_words::page_word;
using sgn::retail::frontend_page_words::set_page_word;
using sgn::retail::frontend_page_words::source_word;
void clear(FrontendState &f, W object, W bytes)
{
    for (W offset = 0; offset < bytes; offset += 4u)
        set_page_word(f, object, offset, 0);
}
W index_float(W selected)
{
    W out;
    sgn_select_team_index_45b831(&selected, &out);
    return out;
}
W initialize_45b7a0(FrontendState &f, W page, SourceNavigationPageServices &pages, SourceMainPageServices &main)
{
    const auto child = allocate_frontend_page_object_43d470(page_pool(f), 200);
    clear(f, child, 200); // 45B7BA before both callback reads
    const auto close = source_word(f, 0x4794e8), event = source_word(f, 0x4794ec);
    set_page_word(f, child, 12, close);
    set_page_word(f, child, 16, event);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x4891e0);
    set_page_word(f, child, 8, 0);
    if (initialize_frontend_menu_text_43c860(f, child, 0x4891a8, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        set_page_word(f, page, 0x14, 0);
    }
    else
        set_page_word(f, page, 0x14, child);
    const auto retained = page_word(f, page, 0x14); // 45B7FE once, both callbacks retain ESI
    (void)main.menu_text_event(f, retained, 24, 0);
    (void)main.menu_text_event(f, retained, 22, 0);
    const auto selected = source_word(f, 0x5e38f0); // 45B817 before clearing cancel flag
    source_word(f, 0x5e38ec) = 0;
    if (selected == 0)
        source_word(f, 0x5e38f0) = 1;                            // only zero, no clamp to1..6
    const auto position = index_float(source_word(f, 0x5e38f0)); // fresh45B831 FILD, FSUB1
    set_page_word(f, page, 0x1c, position);
    set_page_word(f, page, 0x18, position);
    return 1;
}
void tick_45b952(FrontendState &f, W page, W event, SourceMainPageServices &main)
{
    (void)main.menu_text_event(f, page_word(f, page, 0x14), event, 0);
    const auto selected = source_word(f, 0x5e38f0); // after child callback
    const auto previous = page_word(f, page, 0x18); // 45B962 capture before target publication
    const auto target = index_float(selected);
    set_page_word(f, page, 0x1c, target);
    const auto result = std::bit_cast<W>(approach_frontend_value_43d810(
        std::bit_cast<float>(previous), std::bit_cast<float>(target), std::bit_cast<float>(W{0x3a83126f})));
    set_page_word(f, page, 0x18, result); // 45B988
    // 43DF60 writes the actual six-word navigation target, after the page spill.
    source_word(f, 0x5ff488) = 0x40c00000;
    source_word(f, 0x5ff48c) = 0x3f800000;
    source_word(f, 0x5ff490) = result;
    source_word(f, 0x5ff494) = 0;
    source_word(f, 0x5ff498) = 0;
    source_word(f, 0x5ff49c) = 0;
}
void strip_45ba30(FrontendState &f, W page, SourceTitlePageServices &draw)
{
    const auto position = page_word(f, page, 0x18);
    W x;
    sgn_select_team_strip_45ba34(&position, &x);
    for (W descriptor = 0x25; descriptor < 0x2b; ++descriptor)
    {
        const auto captured = x; // 45BA4D: both callbacks retain EDI
        draw.sprite_438710(f, 18, descriptor + 6u, {captured, 0x43300000, 0x3f800000, 0x203});
        draw.sprite_438710(f, 17, descriptor, {captured, 0x43300000, 0x3f800000, 0x103});
        // 45BA89 advances even after the sixth pair, preserving the source spill.
        sgn_select_team_next_45ba89(&x, &x);
    }
}
auto language(FrontendState &f, W id)
{
    return resolve_frontend_language_text(f.source.language_strings(),
                                          frontend_language_text_441170(f.source.language_strings(), id));
}
std::span<const std::uint8_t> value_43d5f0(FrontendState &f, W value)
{
    // Negative source branch returns the actual mutable479664 owner without
    // touching scratch. The existing complete EDX0 child handles positive values.
    if (as_signed(value) < 0)
        return f.source.source_bytes(0x479664, 4);
    const auto raw = std::as_writable_bytes(std::span<std::uint8_t, 128>{f.source.source_bytes(0x5ff328, 128)});
    const auto text = format_campaign_value_43d5f0(value, raw);
    return {reinterpret_cast<const std::uint8_t *>(text.data()), text.size()};
}
void detail_45bab0(FrontendState &f, SourceTitlePageServices &draw, SourceMenuPageDrawServices &panels)
{
    const auto selected = source_word(f, 0x5e38f0); // before aligned text callback
    draw.text_438ba0(f, 12, 4, {0x43600000, 0x43a80000, selected + 0x1f1u, 0x3fc00000, 0xffffffff, 0x803});
    const SourceMenuRect rectangle{0x43dc0000, 0x43900000, 0x44160000, 0x43a20000};
    panels.frame_439450(f, 10, 111, rectangle, 0x40);
    panels.fill_439380(f, 13, 2, rectangle, 0, 0);
    // 438BE0 resolves language id27 and queues one unboxed line, argument1.
    const auto title = language(f, 27);
    queue_frontend_text_438990(f.source, 12, 1, 0x43e40000, 0x43990000, 4, title, 0x3fc00000, 0xffffffff, 3, 0);
    const auto sponsor = source_word(f, 0x5e38f0);                // 45BB52 fresh read after earlier draw children
    const auto amount = source_word(f, 0x5f9470u + sponsor * 8u); // no host sponsor clamp
    const auto text = value_43d5f0(f, amount);
    f.world().ui.fonts.metrics().digit_advance_bits = 0x41600000; // 438A20 ->4291A0, actual57B418
    queue_frontend_text_438990(f.source, 12, 0, 0x44120000, 0x43990000, 4, text, 0x3fc00000, 0xffffffff, 0x806,
                               0x41600000);
}
} // namespace
W create_frontend_select_team_page_43e000(FrontendState &f, W key, SourceNavigationPageServices &pages,
                                          SourceMainPageServices &main)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 32); // 43E5D8
    const auto event = source_word(f, 0x4891dc);                              // 43E5DD before REP clear
    clear(f, page, 32);
    const auto close = source_word(f, 0x4891d8); // 43E5F0 after clear
    set_page_word(f, page, 4, 11);
    set_page_word(f, page, 12, close);
    set_page_word(f, page, 16, event);
    set_page_word(f, page, 0, 0x47975c);
    set_page_word(f, page, 8, 0);
    if (initialize_45b7a0(f, page, pages, main) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), page);
        return 0;
    }
    set_page_word(f, page, 8, key);
    return page; // 43ED94 common factory epilogue
}
void destroy_frontend_select_team_page_45b850(FrontendState &f, W page)
{
    f.shared_platform()->require_owner_thread();
    const auto child = page_word(f, page, 0x14); // retained across close/free
    if (page_word(f, child, 12) != 0x43c970)
        throw FormatError("45B859 reached unimplemented MenuText close identity");
    // 43C970 is RET. The parent performs43D4C0 and only then clears its pointer.
    release_frontend_page_object_43d4c0(page_pool(f), child);
    set_page_word(f, page, 0x14, 0);
}
W dispatch_frontend_select_team_page_45b870(FrontendState &f, W page, W event, SourceNavigationPageServices &pages,
                                            CampaignScoreMusic &music, SourceMainPageServices &main,
                                            SourceTitlePageServices &draw, SourceMenuPageDrawServices &panels)
{
    f.shared_platform()->require_owner_thread();
    // Canonical45BA18 byte map and45B9F4 switch targets: noevent1 handler.
    switch (event)
    {
    case 2:
        tick_45b952(f, page, event, main);
        break;
    case 3:
        strip_45ba30(f, page, draw);
        set_frontend_clip_438190(f.source, {0, 0, 0x44200000, 0x43f00000});
        main.background_4392b0(f, 42, 0);
        (void)main.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        detail_45bab0(f, draw, panels);
        draw.text_438ff0(f, 2, 4,
                         {0x43a00000, 0x43d20000, 43, 0x3fa00000, 0xffffffff, 4, 0, 0x3f800000, 0x41, 0xfffffe70, 0});
        break;
    case 11:
    case 12:
    case 20:
        (void)main.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        break;
    case 13:
    case 14: {
        const auto selected = source_word(f, 0x5e38f0);
        if (event == 13 ? as_signed(selected) <= 1 : as_signed(selected) >= 6)
            break;
        source_word(f, 0x5e38f0) = selected + (event == 13 ? 0xffffffffu : 1u);
        main.feedback_440890(f, event);
        pages.feedback_440880(f, 20);
        break;
    }
    case 16:
        (void)main.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        (void)pop_frontend_page_43eef0(f, pages, music);
        break;
    case 17:
        if (pop_frontend_page_43eef0(f, pages, music) != 0)
        {
            source_word(f, 0x5e38ec) = 1;
            main.feedback_440890(f, event);
        }
        break;
    case 19:
        source_word(f, 0x5ff46c) = 4; // full4400A0(4), not the other pages'1
        set_frontend_packed_word_43dd10(f.source, 0x114311);
        source_word(f, 0x5ff448) = 107;
        source_word(f, 0x5ff44c) = 0x3e99999a;
        source_word(f, 0x5ff450) = 0xbf000000;
        (void)main.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        break;
    default:
        break;
    }
    return 0;
}
} // namespace sgn::retail
