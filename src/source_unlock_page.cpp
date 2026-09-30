#include "sgn/source_unlock_page.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_selection_data.hpp"
#include "sgn/source_menu_text.hpp"
#include "sgn/source_menu_draw_children.hpp"
#include "sgn/source_menu_fill.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <array>
#include <bit>

extern "C" std::uint32_t sgn_menu_option_fild_fstp(std::uint32_t) noexcept;
extern "C" void sgn_unlock_x_45dfc5(const std::uint32_t *, std::uint32_t *) noexcept;
extern "C" void sgn_unlock_rect_45e006(const std::uint32_t *, std::uint32_t *) noexcept;

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_signed;
using sgn::retail::frontend_page_words::page_pool;
using sgn::retail::frontend_page_words::page_word;
using sgn::retail::frontend_page_words::set_page_word;
using sgn::retail::frontend_page_words::source_word;
std::uint8_t &byte(FrontendState &f, W a)
{
    return f.source.source_bytes(a, 1)[0];
}
auto language(FrontendState &f, W id)
{
    return resolve_frontend_language_text(f.source.language_strings(),
                                          frontend_language_text_441170(f.source.language_strings(), id));
}
auto message_tail(FrontendState &f, W address)
{
    // NEW owner bound; do not impose an invented128-byte formatting limit.
    // Original writes may reach a row's descriptor or subsequent retained rows.
    if (address < 0x627ffcu || address >= 0x62a078u)
        throw FormatError("PageUnlock message leaves actual627FFC record owner");
    return f.source.source_bytes(address, 0x62a078u - address);
}
void formatted_message(FrontendState &f, W output, W name_id, W format_id, W return_site)
{
    // Both language calls precede46280C; keep their original order.
    const auto name_identity = frontend_language_text_441170(f.source.language_strings(), name_id);
    const auto format_identity = frontend_language_text_441170(f.source.language_strings(), format_id);
    const auto name = resolve_frontend_language_text(f.source.language_strings(), name_identity);
    const auto format = resolve_frontend_language_text(f.source.language_strings(), format_identity);
    const auto platform = f.shared_platform();
    const std::array<W, 1> args{name_identity.value};
    SourceScriptPathCall call;
    format_save_ui_46280c(platform->state().boot_config, platform->source_application_literals(), format, args,
                          message_tail(f, output), return_site, call, {4, name});
}
void copy_message(FrontendState &f, W address, std::span<const std::uint8_t> text)
{
    std::size_t n = 0;
    while (n < text.size() && text[n] != 0)
        ++n;
    if (n == text.size())
        throw FormatError("45DD0C source language scan lacks bounded NUL");
    ++n;
    auto out = message_tail(f, address);
    std::size_t copied = 0;
    for (; copied + 4u <= n; copied += 4u)
    {
        const W v =
            W(text[copied]) | (W(text[copied + 1]) << 8) | (W(text[copied + 2]) << 16) | (W(text[copied + 3]) << 24);
        if (copied > out.size() || out.size() - copied < 4u)
            throw FormatError("45DD20 language DWORD copy leaves actual Unlock record owner");
        for (W k = 0; k < 4; ++k)
            out[copied + k] = std::uint8_t(v >> (k * 8));
    }
    for (; copied < n; ++copied)
    {
        const auto value = text[copied];
        if (copied >= out.size())
            throw FormatError("45DD27 language byte copy leaves actual Unlock record owner");
        out[copied] = value;
    }
}
W next_unlock_45dc40(FrontendState &f, W row)
{
    byte(f, row) = 0;
    source_word(f, row + 0x80u) = 0;
    W track = 1;
    while (track < 25u && byte(f, 0x5df580u + track) != 2)
        ++track;
    if (track < 25u)
    {
        byte(f, 0x5df580u + track) = 1;
        source_word(f, row + 0x80u) = track + 0xc1u;
        const auto type = source_track_type_437360(track);
        if (type == 1 || type == 3)
            formatted_message(f, row, track + 0x1b7u, 0x27a, 0x45dcc6);
        else if (type == 2 || type == 4)
            formatted_message(f, row, track + 0x1b7u, 0x27b, 0x45dcee);
        else
            copy_message(f, row, language(f, track + 0x1b7u));
        return 1;
    }
    W car = 1;
    while (car < 22u && byte(f, 0x5df568u + car) != 2)
        ++car;
    if (car >= 22u)
        return 0;
    const auto index = car - 1u;
    byte(f, 0x5df568u + car) = 1;
    source_word(f, row + 0x80u) = car + 0xacu;
    // 45DDC4's21-byte remap:16/19=>277,20=>279, every other source car=>278.
    if (index <= 20u)
    {
        const W format = (car == 16u || car == 19u) ? 0x277u : car == 20u ? 0x279u : 0x278u;
        formatted_message(f, row, car + 0x18au, format, 0x45dd9a);
    }
    return 1;
}
void draw_unlock_45dfa0(FrontendState &f, W page, SourceTitlePageServices &draw)
{
    if (as_signed(source_word(f, 0x627ff8)) <= 0)
        return;
    W index = 0, row = 0x627ffcu;
    do
    {
        const auto sprite = source_word(f, row + 0x80u);
        const std::array<W, 2> x_args{index, page_word(f, page, 0x1c)};
        W x;
        sgn_unlock_x_45dfc5(x_args.data(), &x);
        draw.sprite_438710(f, 12, sprite, {x, 0x433e0000, 0x3f800000, 3});
        if (byte(f, row) != 0)
        {
            auto text = message_tail(f, row);
            W lines = 1;
            std::size_t i = 0;
            for (;;)
            {
                if (i >= text.size())
                    throw FormatError("438A60 Unlock text scan leaves retained owner");
                const auto ch = text[i++];
                if (ch == 0)
                    break;
                if (ch == '|' || ch == 255u)
                    ++lines;
            }
            // 438A60's unspilled height continues through45E01A..45E05F.
            const std::array<W, 2> args{x, lines};
            std::array<W, 4> r;
            sgn_unlock_rect_45e006(args.data(), r.data());
            const SourceMenuRect rect{r[0], r[1], r[2], r[3]};
            draw_frontend_frame_439450(f, 10, 111, rect, 0x40);
            draw_frontend_fill_439380(f, 13, 2, rect, 0, 0);
            (void)draw_frontend_aligned_borrowed_438ab0(f, {12, 4, x, 0x43ae0000, {}, {0x3fa00000, 0xffffffff, 3}},
                                                        text);
        }
        const auto count = source_word(f, 0x627ff8);
        ++index;
        row += 0x84u;
        if (as_signed(index) >= as_signed(count))
            break; // source rereads count after each row
    } while (true);
}
} // namespace
W create_frontend_unlock_page_43e000(FrontendState &f, W key, SourceNavigationPageServices &pages,
                                     SourceMainPageServices &main)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 36);
    const auto close = source_word(f, 0x489700); // 43EB99 before parent clear
    for (W o = 0; o < 36; o += 4)
        set_page_word(f, page, o, 0);
    set_page_word(f, page, 8, 0);
    const auto event = source_word(f, 0x489704);
    set_page_word(f, page, 12, close);
    set_page_word(f, page, 4, 35);
    set_page_word(f, page, 0, 0x479864);
    set_page_word(f, page, 16, event);
    set_page_word(f, page, 0x18, 0);
    set_page_word(f, page, 0x1c, 0);
    set_page_word(f, page, 0x20, 0); // 45DB8C..92
    const auto child = allocate_frontend_page_object_43d470(page_pool(f), 200);
    for (W o = 0; o < 200; o += 4)
        set_page_word(f, child, o, 0);
    const auto child_close = source_word(f, 0x4794e8), child_event = source_word(f, 0x4794ec);
    set_page_word(f, child, 12, child_close);
    set_page_word(f, child, 16, child_event);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x489708);
    set_page_word(f, child, 8, 0);
    if (initialize_frontend_menu_text_43c860(f, child, 0x4896d0, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        set_page_word(f, page, 0x14, 0);
    }
    else
        set_page_word(f, page, 0x14, child);
    const auto retained = page_word(f, page, 0x14);
    (void)main.menu_text_event(f, retained, 24, 0);
    (void)main.menu_text_event(f, retained, 22, 0);
    // 437870 is a literal RET; it must not create unlocks or clear another bank.
    source_word(f, 0x627ff8) = 0;
    W index = 0;
    for (;;)
    {
        if (next_unlock_45dc40(f, 0x627ffcu + index * 132u) == 0)
            break;
        index = source_word(f, 0x627ff8) + 1u;
        source_word(f, 0x627ff8) = index;
        if (as_signed(index) >= 63)
            break;
    }
    set_page_word(f, page, 8, key);
    return page;
}
void destroy_frontend_unlock_page_45dde0(FrontendState &f, W page)
{
    f.shared_platform()->require_owner_thread();
    const auto child = page_word(f, page, 0x14);
    if (page_word(f, child, 12) != 0x43c970)
        throw FormatError("Unlock reached an unreconstructed MenuText close callback");
    release_frontend_page_object_43d4c0(page_pool(f), child);
    set_page_word(f, page, 0x14, 0);
}
W dispatch_frontend_unlock_page_45de00(FrontendState &f, W page, W event, SourceNavigationPageServices &pages,
                                       CampaignScoreMusic &music, SourceMainPageServices &main,
                                       SourceTitlePageServices &draw)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 19:
        source_word(f, 0x5ff46c) = 1;
        set_frontend_packed_word_43dd10(f.source, 0x114311);
        source_word(f, 0x5ff448) = 92;
        source_word(f, 0x5ff44c) = 0xbecccccd;
        source_word(f, 0x5ff450) = 0x3e99999a;
        [[fallthrough]];
    case 20:
        (void)main.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        break;
    case 16: {
        const auto last = source_word(f, 0x627ff8) - 1u, index = page_word(f, page, 0x18);
        if (as_signed(index) < as_signed(last))
        {
            set_page_word(f, page, 0x18, index + 1u);
            main.feedback_440890(f, event);
            pages.feedback_440880(f, 20);
        }
        else
        {
            // 45DEA8:13 forward DWORD copies inside the existing persistence.
            for (W i = 0; i < 13; ++i)
            {
                const auto v = persistence_word(f.source.source_bytes(0x5df568u + i * 4u, 4), 0);
                persistence_word(f.source.source_bytes(0x5df59cu + i * 4u, 4), 0, v);
            }
            (void)main.menu_text_event(f, page_word(f, page, 0x14), event, 0);
            (void)pop_frontend_page_43eef0(f, pages, music);
        }
        break;
    }
    case 1:
        if (source_word(f, 0x627ff8) == 0)
            (void)pop_frontend_page_43eef0(f, pages, music);
        break;
    case 2: {
        (void)main.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        const auto target = sgn_menu_option_fild_fstp(page_word(f, page, 0x18)), current = page_word(f, page, 0x1c);
        set_page_word(f, page, 0x20, target);
        const auto value = approach_frontend_value_43d810(std::bit_cast<float>(current), std::bit_cast<float>(target),
                                                          std::bit_cast<float>(0x3a83126fu));
        set_page_word(f, page, 0x1c, std::bit_cast<W>(value));
        break;
    }
    case 3: {
        const auto last = source_word(f, 0x627ff8) - 1u, index = page_word(f, page, 0x18);
        const auto row = source_word(f, 0x4896f8);
        source_word(f, row + 12u) = as_signed(index) >= as_signed(last) ? 23u : 26u;
        (void)main.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        main.background_4392b0(f, 0x1fe, 0);
        if (source_word(f, 0x627ff8) != 0)
            draw_unlock_45dfa0(f, page, draw);
        break;
    }
    default:
        break; // including event17: source does not permit cancel here
    }
    return 0;
}
} // namespace sgn::retail
