#include "sgn/source_catalog_page.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/retail_frontend_clip.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <array>
#include <bit>

extern "C" std::uint32_t sgn_menu_option_fild_fstp(std::uint32_t) noexcept;
extern "C" void sgn_catalog_coordinate_4528c6(const std::uint32_t *, std::uint32_t *) noexcept;

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
W category_count_437a90(W category)
{
    if (category == 1)
        return 20;
    if (category == 2)
        return 24;
    if (category - 3u <= 9u)
        return 5;
    return category == 13 ? 3u : 0u;
}
W first_material_437af0(W category)
{
    constexpr W values[]{173, 194, 150, 120, 135, 130, 155, 125, 145, 140, 165, 160, 170};
    return category - 1u < 13u ? values[category - 1u] : 0u;
}
W row_count_4527d0(FrontendState &f, W row)
{
    return category_count_437a90(source_word(f, 0x486b8cu + row * 0x108u));
}
// The source bubble sorts act on these invocation-local64-DWORD lists.
// Forward adjacent swaps and signed keys match4375A0/437710.
template <std::size_t N> void sort_ids(std::array<W, 64> &ids, W count, const std::array<W, N> &ranks)
{
    const auto rank = [&](W id) { return id - 1u < N ? ranks[id - 1u] : 0u; };
    for (W remaining = count - 1u; as_signed(remaining) >= 0; --remaining)
        for (W i = 0; i < remaining; ++i)
        {
            const auto next = ids[i + 1u], current = ids[i];
            if (as_signed(rank(next)) < as_signed(rank(current)))
            {
                ids[i] = next;
                ids[i + 1u] = current;
            }
        }
}
W initialize_catalog_4522c0(FrontendState &f, W page, SourceNavigationPageServices &pages,
                            SourceMainPageServices &calls)
{
    const auto child = allocate_frontend_page_object_43d470(page_pool(f), 0xc8);
    for (W offset = 0; offset != 0xc8; offset += 4)
        set_page_word(f, child, offset, 0);
    const auto close = source_word(f, 0x4794e8), event = source_word(f, 0x4794ec);
    set_page_word(f, child, 12, close);
    set_page_word(f, child, 16, event);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x487928);
    set_page_word(f, child, 8, 0);
    if (initialize_frontend_menu_text_43c860(f, child, 0x4878f0, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        set_page_word(f, page, 0x14, 0);
    }
    else
        set_page_word(f, page, 0x14, child);
    (void)calls.menu_text_event(f, page_word(f, page, 0x14), 24, 0);
    const auto first = source_word(f, 0x486b8c); // 452333, BEFORE clearing five page fields
    for (W offset = 0x18; offset <= 0x28; offset += 4)
        set_page_word(f, page, offset, 0);
    if (first != 0)
    {
        W cursor = 0x486b8c;
        do
        {
            const auto next = source_word(f, cursor + 0x108u);
            cursor += 0x108u;
            if (next == 0)
                break;
        } while (true);
    }
    if (source_word(f, 0x4869f4) != 0)
    {
        W cursor = 0x4869f4;
        do
        {
            const auto next = source_word(f, cursor + 32u);
            cursor += 32u;
            if (next == 0)
                break;
        } while (true);
    }
    for (W row = 0x486c10; row < 0x487870; row += 0x108u)
        for (W i = 0; i < 32; ++i)
        {
            source_word(f, row - 0x80u + i * 4u) = 0;
            source_word(f, row + i * 4u) = 0;
        }
    std::array<W, 64> cars, tracks;
    W count = 0;
    for (W car = 1; car < 21; ++car)
        if (f.source.source_bytes(0x5df568u + car, 1)[0] != 0)
            cars[count++] = car;
    sort_ids(cars, count, std::array<W, 21>{12, 4, 7, 8, 15, 5, 1, 14, 13, 3, 2, 11, 6, 10, 9, 16, 17, 18, 19, 20, 21});
    W i = 0;
    for (; i < count; ++i)
    {
        const auto category = source_word(f, 0x486b8c), car = cars[i];
        const auto base = first_material_437af0(category);
        source_word(f, 0x486b90u + i * 4u) = base + car - 1u;
        const auto text_base = source_word(f, 0x486b88);
        source_word(f, 0x486c10u + i * 4u) = text_base + car - 1u;
    }
    for (; as_signed(i) < as_signed(category_count_437a90(source_word(f, 0x486b8c))); ++i)
    {
        source_word(f, 0x486b90u + i * 4u) = 119;
        source_word(f, 0x486c10u + i * 4u) = 0;
    }
    count = 0;
    for (W track = 1; track < 25; ++track)
        if (f.source.source_bytes(0x5df580u + track, 1)[0] != 0)
            tracks[count++] = track;
    sort_ids(tracks, count,
             std::array<W, 24>{19, 1, 16, 7, 18, 17, 21, 8, 12, 13, 14, 5, 6, 24, 20, 15, 23, 9, 10, 2, 22, 11, 3, 4});
    for (i = 0; i < count; ++i)
    {
        const auto category = source_word(f, 0x486c94), track = tracks[i];
        const auto base = first_material_437af0(category);
        source_word(f, 0x486c98u + i * 4u) = base + track - 1u;
        const auto text_base = source_word(f, 0x486c90);
        source_word(f, 0x486d18u + i * 4u) = text_base + track - 1u;
    }
    for (; as_signed(i) < as_signed(category_count_437a90(source_word(f, 0x486c94))); ++i)
    {
        source_word(f, 0x486c98u + i * 4u) = 119;
        source_word(f, 0x486d18u + i * 4u) = 0;
    }
    for (W group = 0x486d9c; group < 0x4877ec; group += 0x108u)
    {
        const auto total = category_count_437a90(source_word(f, group));
        const auto base = first_material_437af0(source_word(f, group)); // fresh source category read
        for (W item = 0; as_signed(item) < as_signed(total); ++item)
        {
            source_word(f, group + 4u + item * 4u) = base + item;
            const auto text_base = source_word(f, group - 4u);
            source_word(f, group + 0x84u + item * 4u) = text_base + item;
        }
    }
    return 1;
}
W approach(W current, W target)
{
    return std::bit_cast<W>(approach_frontend_value_43d810(std::bit_cast<float>(current), std::bit_cast<float>(target),
                                                           std::bit_cast<float>(W{0x3a83126f})));
}
void draw_catalog_strip_452830(FrontendState &f, W xphase, W yphase, SourceTitlePageServices &draw)
{
    const auto xi = static_cast<W>(frontend_ftol_f32_462860(xphase));
    const auto yi = static_cast<W>(frontend_ftol_f32_462860(yphase));
    W first = xi - 1u, limit = xi + 3u, last_row = yi + 2u, row = yi;
    if (as_signed(first) < 0)
        first = 0;
    if (as_signed(row) < 0)
        row = 0;
    if (as_signed(limit) > 32)
        limit = 32;
    if (as_signed(last_row) > 12)
        last_row = 12;
    for (; as_signed(row) < as_signed(last_row); ++row)
    {
        W end = row_count_4527d0(f, row);
        if (as_signed(end) > as_signed(limit))
            end = limit;
        if (as_signed(first) >= as_signed(end))
            continue;
        const std::array<W, 3> y_input{row, yphase, 0x43460000};
        W y;
        sgn_catalog_coordinate_4528c6(y_input.data(), &y); // 4528C6..DB, once for this row
        for (W item = first; as_signed(item) < as_signed(end); ++item)
        {
            const auto material = source_word(f, 0x486b90u + row * 0x108u + item * 4u);
            const std::array<W, 3> x_input{item, xphase, 0x43c00000};
            W x;
            sgn_catalog_coordinate_4528c6(x_input.data(), &x); // 4528DE..52903
            draw.sprite_438710(f, 17, material, {x, y, 0x3f800000, 3});
        }
    }
}
void draw_catalog_name_4527f0(FrontendState &f, W item, W row, SourceTitlePageServices &draw)
{
    const auto text = source_word(f, 0x486c10u + (item + row * 66u) * 4u);
    if (text != 0)
        draw.text_438ba0(f, 12, 4, {0x43c00000, 0x43bf0000, text, 0x3fc00000, 0xffc8c8ff, 3});
}
} // namespace
std::uint32_t create_frontend_catalog_page_43e000(FrontendState &f, W music_key, SourceNavigationPageServices &pages,
                                                  SourceMainPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 0x2c); // 43E274
    const auto close = source_word(f, 0x487920);                                // before REP clear
    for (W offset = 0; offset != 0x2c; offset += 4)
        set_page_word(f, page, offset, 0);
    set_page_word(f, page, 8, 0);
    const auto event = source_word(f, 0x487924);
    set_page_word(f, page, 12, close);
    set_page_word(f, page, 4, 6);
    set_page_word(f, page, 0, 0x4796c4);
    set_page_word(f, page, 16, event);
    if (initialize_catalog_4522c0(f, page, pages, calls) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), page);
        return 0;
    }
    set_page_word(f, page, 8, music_key);
    return page;
}
void destroy_frontend_catalog_page_452530(FrontendState &f, W page)
{
    f.shared_platform()->require_owner_thread();
    const auto child = page_word(f, page, 0x14);
    if (page_word(f, child, 12) != 0x43c970u)
        throw FormatError("452530 reached an unreconstructed MenuText destructor");
    release_frontend_page_object_43d4c0(page_pool(f), child);
    set_page_word(f, page, 0x14, 0);
}
std::uint32_t dispatch_frontend_catalog_page_452550(FrontendState &f, W page, W event,
                                                    SourceNavigationPageServices &pages, CampaignScoreMusic &music,
                                                    SourceMainPageServices &main, SourceTitlePageServices &draw)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 2: {
        const auto child = page_word(f, page, 0x14);
        (void)main.menu_text_event(f, child, event, 0);
        const auto row = main.menu_text_event(f, child, 26, 0);
        const auto count = row_count_4527d0(f, row), selected = page_word(f, page, 0x18);
        const auto item = as_signed(selected) >= as_signed(count) ? count - 1u : selected;
        const auto x = sgn_menu_option_fild_fstp(item), y = sgn_menu_option_fild_fstp(row);
        set_page_word(f, page, 0x24, x);
        set_page_word(f, page, 0x28, y);
        set_page_word(f, page, 0x1c, approach(page_word(f, page, 0x1c), x));
        set_page_word(f, page, 0x20, approach(page_word(f, page, 0x20), y));
        const std::array<W, 6> range{
            sgn_menu_option_fild_fstp(count), 0x3f800000, sgn_menu_option_fild_fstp(item), 0x41400000, 0x3f800000, y};
        for (W i = 0; i < 6; ++i)
            source_word(f, 0x5ff488u + i * 4u) = range[i]; // full43DF60
        break;
    }
    case 3: {
        const auto y = page_word(f, page, 0x20), x = page_word(f, page, 0x1c);
        draw_catalog_strip_452830(f, x, y, draw);
        set_frontend_clip_438190(f.source, {0, 0, 0x44200000, 0x43f00000});
        main.background_4392b0(f, 209, 0);
        const auto child = page_word(f, page, 0x14);
        (void)main.menu_text_event(f, child, event, 0);
        const auto row = main.menu_text_event(f, child, 26, 0);
        auto item = page_word(f, page, 0x18);
        const auto last = row_count_4527d0(f, row) - 1u;
        if (as_signed(item) > as_signed(last))
            item = last;
        const auto fresh_row = main.menu_text_event(f, child, 26, 0);
        draw_catalog_name_4527f0(f, item, fresh_row, draw);
        break;
    }
    case 11:
    case 12:
        if (main.menu_text_event(f, page_word(f, page, 0x14), event, 0) != 0)
            pages.feedback_440880(f, 20);
        break;
    case 13: {
        const auto row = main.menu_text_event(f, page_word(f, page, 0x14), 26, 0);
        const auto count = row_count_4527d0(f, row);
        if (as_signed(page_word(f, page, 0x18)) >= as_signed(count))
            set_page_word(f, page, 0x18, count - 1u);
        const auto item = page_word(f, page, 0x18);
        if (item != 0)
        {
            set_page_word(f, page, 0x18, item - 1u);
            pages.feedback_440880(f, 20);
            main.feedback_440890(f, event);
        }
        break;
    }
    case 14: {
        const auto child = page_word(f, page, 0x14), item = page_word(f, page, 0x18);
        const auto row = main.menu_text_event(f, child, 26, 0);
        if (as_signed(item) < as_signed(row_count_4527d0(f, row) - 1u))
        {
            set_page_word(f, page, 0x18, item + 1u);
            pages.feedback_440880(f, 20);
            main.feedback_440890(f, event);
        }
        break;
    }
    case 16:
    case 17:
        if (pop_frontend_page_43eef0(f, pages, music) != 0)
            main.feedback_440890(f, event);
        break;
    case 19:
        source_word(f, 0x5ff46c) = 3;
        set_frontend_packed_word_43dd10(f.source, 0x534527);
        source_word(f, 0x5ff448) = 105;
        source_word(f, 0x5ff44c) = 0xbe99999a;
        source_word(f, 0x5ff450) = 0x3f000000;
        [[fallthrough]];
    case 20:
        (void)main.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        break;
    default:
        break;
    }
    return 0;
}
} // namespace sgn::retail
