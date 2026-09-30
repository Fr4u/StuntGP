#include "sgn/source_text_splash_page.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_title_page.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/source_menu_text.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_signed;
W spill(double value)
{
    return std::bit_cast<W>(vehicle_f32(value));
}
using sgn::retail::frontend_page_words::page_pool;
using sgn::retail::frontend_page_words::page_word;
using sgn::retail::frontend_page_words::set_page_word;
void clear(FrontendState &f, W page, W size)
{
    for (W at = 0; at < size; at += 4u)
        set_page_word(f, page, at, 0);
}
using sgn::retail::frontend_page_words::source_word;
W row_word(FrontendState &f, SourceTextPanelRows rows, W index)
{
    if (rows.source_address != 0)
        return source_word(f, rows.source_address + index * 4u);
    if (index >= rows.local_words.size())
        throw FormatError("45BE20 row read leaves caller local owner");
    return rows.local_words[index];
}
auto text_tail(FrontendState &f, W identity)
{
    // The260-byte campaign text extent includes the four-byte initials alias
    // at61BAC8. Later roster fields belong to the same storage, not this text.
    if (identity >= 0x61b9c8u && identity < 0x61baccu)
        return f.source.source_bytes(identity, 0x61baccu - identity);
    if (identity >= 0x61bdccu && identity < 0x61bed0u)
        return f.source.source_bytes(identity, 0x61bed0u - identity);
    if (identity >= 0x61c0c0u && identity < 0x61c1c0u)
        return f.source.source_bytes(identity, 0x61c1c0u - identity);
    if (identity == frontend_catalog_text_tag)
        return f.source.source_bytes(0x6087a4, 4);
    if ((identity & frontend_catalog_text_tag) != 0)
    {
        const auto offset = (identity & ~frontend_catalog_text_tag) - 1u;
        constexpr auto size = frontend_language_catalog_words * 4u;
        if (offset >= size)
            throw FormatError("45BE20 catalog identity leaves actual owner");
        return f.source.source_bytes(0x5e37b8u + offset, size - offset);
    }
    if (identity == 0 || identity > f.source.language_arena.size())
        throw FormatError("45BE20 text dereferences null or an unbound source owner");
    return std::span{f.source.language_arena}.subspan(identity - 1u);
}
double text_height_438a60(FrontendState &f, W identity)
{
    const auto text = text_tail(f, identity);
    W lines = 1;
    for (std::size_t i = 0;; ++i)
    {
        if (i >= text.size())
            throw FormatError("438A60 text scan leaves its actual owner");
        if (text[i] == 0)
            break;
        if (text[i] == 0x7c || text[i] == 0xff)
            ++lines;
    }
    // No intermediate float store between438950,FMUL4793D8,FIMUL line count.
    return (1.25 * 16.0 * double(as_float(0x3f59999a))) * double(as_signed(lines));
}
W initialize_splash_45bd70(FrontendState &f, W page, SourceNavigationPageServices &pages, SourceMainPageServices &calls)
{
    const auto child = allocate_frontend_page_object_43d470(page_pool(f), 200);
    clear(f, child, 200);
    const auto close = source_word(f, 0x4794e8), event = source_word(f, 0x4794ec);
    set_page_word(f, child, 12, close);
    set_page_word(f, child, 16, event);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x489280);
    set_page_word(f, child, 8, 0);
    if (initialize_frontend_menu_text_43c860(f, child, 0x489248, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        set_page_word(f, page, 20, 0);
    }
    else
        set_page_word(f, page, 20, child);
    const auto retained = page_word(f, page, 20);
    (void)calls.menu_text_event(f, retained, 24, 0);
    (void)calls.menu_text_event(f, retained, 22, 0);
    source_word(f, 0x5e3b48) = 0;
    return 1;
}
} // namespace
std::span<std::uint8_t> borrow_frontend_panel_text(FrontendState &f, W identity)
{
    return text_tail(f, identity);
}
double measure_frontend_panel_text_438a60(FrontendState &f, W identity)
{
    return text_height_438a60(f, identity);
}
void draw_frontend_text_panel_45be20(FrontendState &f, SourceTextPanelRows rows, W count, std::array<W, 4> stack,
                                     SourceMenuPageDrawServices &panels)
{
    f.shared_platform()->require_owner_thread();
    float height = 0;
    for (W i = 0; as_signed(i) < as_signed(count); ++i)
        height = vehicle_f32(text_height_438a60(f, row_word(f, rows, i * 2u + 1u)) + double(height));
    height = vehicle_f32(double(as_signed(count - 1u)) * double(as_float(stack[3])) + double(height));
    const double half_width = (double(as_float(stack[2])) + 40.0) * 0.5;
    const auto padded = vehicle_f32(double(height) + 20.0);
    const auto left = spill(double(as_float(stack[0])) - half_width),
               right = spill(half_width + double(as_float(stack[0])));
    const double half_height = double(padded) * 0.5;
    const auto top = spill(double(as_float(stack[1])) - half_height),
               bottom = spill(half_height + double(as_float(stack[1])) + 4.0);
    const SourceMenuRect rect{left, top, right, bottom};
    panels.frame_439450(f, 10, 111, rect, 0x40);
    panels.fill_439380(f, 13, 2, rect, 0, 0);
    auto y = vehicle_f32(double(as_float(stack[1])) - double(height) * 0.5);
    for (W i = 0; as_signed(i) < as_signed(count); ++i)
    {
        // 45BF05 color then45BF07 text: reread after both drawing callbacks.
        const auto color = row_word(f, rows, i * 2u), identity = row_word(f, rows, i * 2u + 1u);
        const auto advance = draw_frontend_aligned_borrowed_438ab0(
            f, {12, 4, stack[0], std::bit_cast<W>(y), {}, {0x3fa00000, color, 1}}, text_tail(f, identity));
        y = vehicle_f32(advance + double(y) + double(as_float(stack[3])));
    }
}
W create_frontend_text_splash_page_43e000(FrontendState &f, W key, SourceNavigationPageServices &pages,
                                          SourceMainPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 24);
    const auto event = source_word(f, 0x48927c);
    clear(f, page, 24);
    const auto close = source_word(f, 0x489278);
    set_page_word(f, page, 4, 33);
    set_page_word(f, page, 12, close);
    set_page_word(f, page, 16, event);
    set_page_word(f, page, 0, 0x479844);
    set_page_word(f, page, 8, 0);
    if (initialize_splash_45bd70(f, page, pages, calls) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), page);
        return 0;
    }
    set_page_word(f, page, 8, key);
    return page;
}
void destroy_frontend_text_splash_page_45be00(FrontendState &f, W page)
{
    f.shared_platform()->require_owner_thread();
    const auto child = page_word(f, page, 20);
    if (page_word(f, child, 12) != 0x43c970u)
        throw FormatError("45BE09 reached an unreconstructed MenuText close identity");
    // 43C970 is the complete RET; this parent releases its child afterwards.
    release_frontend_page_object_43d4c0(page_pool(f), child);
    set_page_word(f, page, 20, 0);
}
W dispatch_frontend_text_splash_page_45bf40(FrontendState &f, W page, W event, SourceNavigationPageServices &pages,
                                            CampaignScoreMusic &music, SourceMainPageServices &calls,
                                            SourceMenuPageDrawServices &panels)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 19:
        source_word(f, 0x5ff46c) = 1;
        set_frontend_packed_word_43dd10(f.source, 0x114311);
        {
            const auto background = source_word(f, 0x5e3b4c);
            source_word(f, 0x5ff448) = background;
        }
        source_word(f, 0x5ff44c) = 0x3e99999a;
        source_word(f, 0x5ff450) = 0xbf000000;
        [[fallthrough]];
    case 2:
    case 11:
    case 12:
    case 20:
        (void)calls.menu_text_event(f, page_word(f, page, 20), event, 0);
        break;
    case 16:
        (void)calls.menu_text_event(f, page_word(f, page, 20), event, 0);
        (void)pop_frontend_page_43eef0(f, pages, music);
        break;
    case 17:
        calls.feedback_440890(f, event);
        source_word(f, 0x5e3b48) = 1;
        (void)pop_frontend_page_43eef0(f, pages, music);
        break;
    case 3:
        (void)calls.menu_text_event(f, page_word(f, page, 20), event, 0);
        {
            const auto count = source_word(f, 0x5e3bf0);
            draw_frontend_text_panel_45be20(f, {{}, 0x5e3b50}, count, {0x43a00000, 0x43700000, 0x43c80000, 0x41200000},
                                            panels);
        }
        break;
    default:
        break;
    }
    return 0;
}
} // namespace sgn::retail
