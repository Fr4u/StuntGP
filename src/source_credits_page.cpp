#include "sgn/source_credits_page.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_title_page.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/source_menu_text.hpp"
#include "sgn/race_session.hpp"
#include "sgn/frontend_page_words.hpp"
#include <algorithm>
#include <bit>
namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
using sgn::retail::frontend_page_words::page_object_bytes;
using sgn::retail::frontend_page_words::page_pool;
using sgn::retail::frontend_page_words::page_word;
using sgn::retail::frontend_page_words::set_page_word;
Word initialize_credits_452940(FrontendState &f, Word page, SourceNavigationPageServices &pages)
{
    const auto child = allocate_frontend_page_object_43d470(page_pool(f), 200); // 45294A
    const auto bytes = page_object_bytes(f, child, 200);
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    const auto close = f.source.source_word(0x4794e8);
    const auto callback = f.source.source_word(0x4794ec);
    set_page_word(f, child, 12, close);
    set_page_word(f, child, 16, callback);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x4879b0);
    set_page_word(f, child, 8, 0);
    if (initialize_frontend_menu_text_43c860(f, child, 0x487978, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        set_page_word(f, page, 20, 0);
    }
    else
        set_page_word(f, page, 20, child);
    const auto retained = page_word(f, page, 20); // 45299E retains ESI across both callbacks
    pages.page_event(f, retained, 24, 0);
    pages.page_event(f, retained, 22, 0);
    return 1;
}
void draw_credits_452ac0(FrontendState &f)
{
    // 438950 multiplies the supplied1.25 by PE literal46E49C (16.0).
    // 452AD1 stores spacing without popping; all these fixed values are exact.
    const double height = 1.25 * 16.0;
    const float spacing = vehicle_f32(height);
    float y = vehicle_f32((480.0 - height * 16.0) * 0.5);
    for (Word id = 0x2b6; id < 0x2c6; ++id)
    {
        // Complete438BE0 path:441170 lookup then438990, layer12/list1/font4.
        // Use the actual loaded language owner, not a duplicate credits table.
        const auto text = frontend_language_text_441170(f.source.language_strings(), id);
        const auto bytes = resolve_frontend_language_text(f.source.language_strings(), text);
        queue_frontend_text_438990(f.source, 12, 1, 0x43a00000, std::bit_cast<Word>(y), 4, bytes, 0x3fa00000,
                                   0xffffffff, 3, 0);
        y = vehicle_f32(double(y) + double(spacing)); // 452B10..452B1D, after submission
    }
}
} // namespace
Word create_frontend_credits_page_43e000(FrontendState &f, Word key, SourceNavigationPageServices &pages)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 24); // 43E9E4
    const auto callback = f.source.source_word(0x4879ac);                     // 43E9E9 before clear
    const auto bytes = page_object_bytes(f, page, 24);
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    const auto close = f.source.source_word(0x4879a8); // 43E9FC after clear
    set_page_word(f, page, 4, 30);
    set_page_word(f, page, 12, close);
    set_page_word(f, page, 16, callback);
    set_page_word(f, page, 0, 0x479818);
    set_page_word(f, page, 8, 0);
    if (initialize_credits_452940(f, page, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), page);
        return 0;
    }
    set_page_word(f, page, 8, key); // 43ED94 common publication only after initialization
    return page;
}
void destroy_frontend_credits_page_4529c0(FrontendState &f, Word page)
{
    f.shared_platform()->require_owner_thread();
    const auto child = page_word(f, page, 20);
    if (page_word(f, child, 12) != 0x43c970u)
        throw FormatError("4529C9 reached unreconstructed MenuText destructor identity");
    // Complete43C970 is RET; the parent owns child release after that call.
    release_frontend_page_object_43d4c0(page_pool(f), child);
    set_page_word(f, page, 20, 0); // 4529D3 after release
}
Word dispatch_frontend_credits_page_4529e0(FrontendState &f, Word page, Word event, SourceNavigationPageServices &pages,
                                           CampaignScoreMusic &music, SourceMainPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 19:
        f.source.source_word(0x5ff46c) = 1; // 4400A0
        set_frontend_packed_word_43dd10(f.source, 0x114311);
        f.source.source_word(0x5ff448) = 95; // 43DD40
        f.source.source_word(0x5ff44c) = 0x3e99999a;
        f.source.source_word(0x5ff450) = 0xbf000000;
        [[fallthrough]];
    case 2:
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
        calls.feedback_440890(f, event);
        (void)pop_frontend_page_43eef0(f, pages, music);
        return 0;
    case 3:
        calls.background_4392b0(f, 0x2b4, 0);
        (void)calls.menu_text_event(f, page_word(f, page, 20), event, 0);
        draw_credits_452ac0(f);
        return 0;
    default:
        return 0;
    }
}
} // namespace sgn::retail
