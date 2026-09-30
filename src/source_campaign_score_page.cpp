#include "sgn/source_campaign_score_page.hpp"
#include "sgn/source_campaign_menu_page.hpp"
#include "sgn/source_text_splash_page.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/source_menu_text.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <bit>

namespace sgn::retail
{
extern "C" void sgn_campaign_score_add_45a52a(const double *, const std::uint32_t *, std::uint32_t *);
extern "C" void sgn_campaign_score_bounds_45a543(const std::uint32_t *, std::uint32_t *);
extern "C" void sgn_campaign_score_y_45a5ad(const std::uint32_t *, std::uint32_t *);
extern "C" void sgn_campaign_score_next_45a5ec(const double *, const std::uint32_t *, std::uint32_t *);
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_signed;
using sgn::retail::frontend_page_words::page_pool;
using sgn::retail::frontend_page_words::page_word;
using sgn::retail::frontend_page_words::set_page_word;
void clear(FrontendState &f, W page, W size)
{
    for (W offset = 0; offset < size; offset += 4u)
        set_page_word(f, page, offset, 0);
}
using sgn::retail::frontend_page_words::source_word;
W initialize_45a360(FrontendState &f, W page, SourceNavigationPageServices &pages, SourceMainPageServices &main)
{
    const auto child = allocate_frontend_page_object_43d470(page_pool(f), 200);
    clear(f, child, 200);
    const auto close = source_word(f, 0x4794e8), event = source_word(f, 0x4794ec);
    set_page_word(f, child, 12, close);
    set_page_word(f, child, 16, event);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x488e70);
    set_page_word(f, child, 8, 0);
    if (initialize_frontend_menu_text_43c860(f, child, 0x488e38, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        set_page_word(f, page, 20, 0);
    }
    else
        set_page_word(f, page, 20, child);
    const auto retained = page_word(f, page, 20); // 45A3BE: same receiver for both callbacks
    (void)main.menu_text_event(f, retained, 24, 0);
    (void)main.menu_text_event(f, retained, 22, 0);
    source_word(f, 0x5fcf08) = 0;
    return 1;
}
void draw_awards_45a500(FrontendState &f, W rows, W count, SourceMenuPageDrawServices &panels)
{
    W height = 0;
    if (as_signed(count) > 0)
    {
        auto remaining = count, entry = rows + 4u;
        do
        {
            const auto identity = source_word(f, entry);
            const auto measured = measure_frontend_panel_text_438a60(f, identity);
            sgn_campaign_score_add_45a52a(&measured, &height, &height);
            entry += 8u;
            --remaining;
        } while (remaining != 0);
    }
    // FST at45A554 publishes a rounded total but retains unrounded ST0 for
    // the padded half-height. This is not45BE20's different spill sequence.
    const std::array<W, 2> input{count - 1u, height};
    std::array<W, 3> bounds;
    sgn_campaign_score_bounds_45a543(input.data(), bounds.data());
    const SourceMenuRect rectangle{0x42c80000, bounds[1], 0x44070000, bounds[2]};
    panels.frame_439450(f, 10, 111, rectangle, 0x40);
    panels.fill_439380(f, 13, 2, rectangle, 0, 0);
    W y;
    sgn_campaign_score_y_45a5ad(&bounds[0], &y); // after both draw callbacks
    if (as_signed(count) > 0)
    {
        auto remaining = count, entry = rows;
        do
        {
            // 45A5C6/45A5C8: fresh color then text after earlier draw children.
            const auto color = source_word(f, entry), identity = source_word(f, entry + 4u);
            const auto advance = draw_frontend_aligned_borrowed_438ab0(
                f, {12, 4, 0x43a00000, y, {}, {0x3fa00000, color, 1}}, borrow_frontend_panel_text(f, identity));
            sgn_campaign_score_next_45a5ec(&advance, &y, &y);
            entry += 8u;
            --remaining;
        } while (remaining != 0);
    }
}
} // namespace
W create_frontend_campaign_score_page_43e000(FrontendState &f, W key, SourceNavigationPageServices &pages,
                                             SourceMainPageServices &main)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 24);
    const auto event = source_word(f, 0x488e6c); // 43ECF2 before REP clear
    clear(f, page, 24);
    const auto close = source_word(f, 0x488e68);
    set_page_word(f, page, 4, 37);
    set_page_word(f, page, 12, close);
    set_page_word(f, page, 16, event);
    set_page_word(f, page, 0, 0x479898);
    set_page_word(f, page, 8, 0);
    if (initialize_45a360(f, page, pages, main) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), page);
        return 0;
    }
    set_page_word(f, page, 8, key);
    return page;
}
void destroy_frontend_campaign_score_page_45a3f0(FrontendState &f, W page)
{
    f.shared_platform()->require_owner_thread();
    const auto retained = page_word(f, page, 20);
    if (page_word(f, retained, 12) != 0x43c970)
        throw FormatError("45A3F9 reached an unimplemented MenuText close identity");
    // 43C970 is RET;45A3FE frees the retained child before clearing parent+14.
    release_frontend_page_object_43d4c0(page_pool(f), retained);
    set_page_word(f, page, 20, 0);
}
W dispatch_frontend_campaign_score_page_45a410(FrontendState &f, W page, W event, SourceNavigationPageServices &pages,
                                               CampaignScoreMusic &music, SourceMainPageServices &main,
                                               SourceMenuPageDrawServices &panels)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 19:
        source_word(f, 0x5ff46c) = 1;
        set_frontend_packed_word_43dd10(f.source, 0x114311);
        {
            const auto background = source_word(f, 0x5fcf0c);
            source_word(f, 0x5ff448) = background;
        }
        source_word(f, 0x5ff44c) = 0x3e99999a;
        source_word(f, 0x5ff450) = 0xbf000000;
        [[fallthrough]];
    case 2:
    case 11:
    case 12:
    case 20:
        (void)main.menu_text_event(f, page_word(f, page, 20), event, 0);
        break;
    case 16:
        (void)main.menu_text_event(f, page_word(f, page, 20), event, 0);
        (void)pop_frontend_page_43eef0(f, pages, music);
        break;
    case 17:
        main.feedback_440890(f, event);
        source_word(f, 0x5fcf08) = 1;
        (void)pop_frontend_page_43eef0(f, pages, music);
        break;
    case 3: {
        // The source draw case has no MenuText callback and no4392B0 call.
        const auto count = source_word(f, 0x5fcfb0);
        draw_awards_45a500(f, 0x5fcf10, count, panels);
        const auto maximum = source_word(f, 0x5fcfb8), race = source_word(f, 0x5fcfb4);
        draw_frontend_campaign_standings_450e50(f, race, maximum, 0x43a00000, 0x43340000, panels);
        break;
    }
    default:
        break;
    }
    return 0;
}
} // namespace sgn::retail
