#include "sgn/source_bmp_splash_page.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/race_session.hpp"
#include "sgn/frontend_page_words.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
namespace sgn::retail
{
extern "C" void sgn_bmp_splash_fraction_45b669(const std::uint32_t *, std::uint32_t *) noexcept;
extern "C" void sgn_bmp_splash_y_45b6b1(const std::uint32_t *, std::uint32_t *) noexcept;
namespace
{
using W = std::uint32_t;
using sgn::retail::frontend_page_words::page_object_bytes;
using sgn::retail::frontend_page_words::page_pool;
using sgn::retail::frontend_page_words::page_word;
using sgn::retail::frontend_page_words::set_page_word;
W initialize_splash_45b520(FrontendState &f, W page, SourceNavigationPageServices &pages)
{
    const auto child = allocate_frontend_page_object_43d470(page_pool(f), 200);
    const auto bytes = page_object_bytes(f, child, 200);
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    const auto close = f.source.source_word(0x4794e8), event = f.source.source_word(0x4794ec);
    set_page_word(f, child, 12, close);
    set_page_word(f, child, 16, event);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x489158);
    set_page_word(f, child, 8, 0);
    if (initialize_frontend_menu_text_43c860(f, child, 0x489120, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        set_page_word(f, page, 20, 0);
    }
    else
        set_page_word(f, page, 20, child);
    const auto retained = page_word(f, page, 20); // 45B57E, no parent reread between24/22
    pages.page_event(f, retained, 24, 0);
    pages.page_event(f, retained, 22, 0);
    set_page_word(f, page, 24, 0); // 45B597 only after both callbacks
    return 1;
}
void advance_fraction(FrontendState &f, W page)
{
    // 45B669 reads increment before45B66F reads the phase, AFTER child event2.
    const std::array<W, 2> inputs{f.source.source_word(0x5e3b40), page_word(f, page, 24)};
    const auto step = std::bit_cast<float>(inputs[0]), phase = std::bit_cast<float>(inputs[1]);
    const double sum = double(step) + double(phase);
    if (!std::isfinite(step) || !std::isfinite(phase) || !(sum >= -0x1p63 && sum < 0x1p63))
        throw FormatError("45B669 animation lies outside finite signed64 FTOL domain");
    W output;
    sgn_bmp_splash_fraction_45b669(inputs.data(), &output);
    set_page_word(f, page, 24, output);
}
W sprite_y(FrontendState &f, W page)
{
    const std::array<W, 2> inputs{page_word(f, page, 24), f.source.source_word(0x5e3b44)};
    const auto phase = std::bit_cast<float>(inputs[0]), amplitude = std::bit_cast<float>(inputs[1]);
    if (!std::isfinite(phase) || !std::isfinite(amplitude) ||
        !(std::abs(double(phase) * double(std::bit_cast<float>(W{0x40490fdb}))) < 0x1p63))
        throw FormatError("45B6B1 sprite wave lies outside finite source FSIN domain");
    W output;
    sgn_bmp_splash_y_45b6b1(inputs.data(), &output);
    return output;
}
void draw_splash(FrontendState &f, W page, SourceTitlePageServices &draw)
{
    auto left = f.source.source_word(0x5e3b38); // 45B69A after actual child draw
    if (left == 0)
        return;
    if (f.source.source_word(0x5e3b3c) == 0)
    {
        const auto y = sprite_y(f, page);
        draw.sprite_438710(f, 12, left, {0x43a00000, y, 0x3f800000, 3}); // 45B6DD
        left = f.source.source_word(0x5e3b38);                           // fresh45B6E2 even after callback
    }
    if (left == 0 || f.source.source_word(0x5e3b3c) == 0)
        return;
    auto y = sprite_y(f, page);
    draw.sprite_438710(f, 12, left, {0x43410000, y, 0x3f800000, 3}); // 45B721
    // Second wave and descriptor are re-read after the first sprite callback.
    const auto right = f.source.source_word(0x5e3b3c);
    y = sprite_y(f, page);
    draw.sprite_438710(f, 12, right, {0x43e00000, y, 0x3f800000, 3}); // 45B758
}
} // namespace
W create_frontend_bmp_splash_page_43e000(FrontendState &f, W key, SourceNavigationPageServices &pages)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 28); // 43EA91
    const auto close = f.source.source_word(0x489150);                        // 43EA96 before clear
    const auto bytes = page_object_bytes(f, page, 28);
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    set_page_word(f, page, 8, 0);
    const auto event = f.source.source_word(0x489154); // 43EAAC
    set_page_word(f, page, 12, close);
    set_page_word(f, page, 4, 32);
    set_page_word(f, page, 0, 0x479834);
    set_page_word(f, page, 16, event);
    if (initialize_splash_45b520(f, page, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), page);
        return 0;
    }
    set_page_word(f, page, 8, key);
    return page;
}
void destroy_frontend_bmp_splash_page_45b5b0(FrontendState &f, W page)
{
    f.shared_platform()->require_owner_thread();
    const auto child = page_word(f, page, 20);
    if (page_word(f, child, 12) != 0x43c970u)
        throw FormatError("45B5B9 reached unreconstructed MenuText destructor identity");
    release_frontend_page_object_43d4c0(page_pool(f), child);
    set_page_word(f, page, 20, 0);
}
W dispatch_frontend_bmp_splash_page_45b5d0(FrontendState &f, W page, W event, SourceNavigationPageServices &pages,
                                           CampaignScoreMusic &music, SourceMainPageServices &calls,
                                           SourceTitlePageServices &draw)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 19: {
        f.source.source_word(0x5ff46c) = 1;
        set_frontend_packed_word_43dd10(f.source, 0x114311);
        const auto background = f.source.source_word(0x5e3b34);
        f.source.source_word(0x5ff448) = background;
        f.source.source_word(0x5ff44c) = 0x3e99999a;
        f.source.source_word(0x5ff450) = 0xbf000000;
        (void)calls.menu_text_event(f, page_word(f, page, 20), event, 0);
        return 0;
    }
    case 11:
    case 12:
    case 20:
        (void)calls.menu_text_event(f, page_word(f, page, 20), event, 0);
        return 0;
    case 16:
        (void)calls.menu_text_event(f, page_word(f, page, 20), event, 0);
        (void)pop_frontend_page_43eef0(f, pages, music);
        return 0;
    case 17:
        pages.feedback_440880(f, 9);
        return 0; // 45B64B feedback only, no pop
    case 2:
        (void)calls.menu_text_event(f, page_word(f, page, 20), event, 0);
        advance_fraction(f, page);
        return 0;
    case 3:
        (void)calls.menu_text_event(f, page_word(f, page, 20), event, 0);
        draw_splash(f, page, draw);
        return 0;
    default:
        return 0;
    }
}
} // namespace sgn::retail
