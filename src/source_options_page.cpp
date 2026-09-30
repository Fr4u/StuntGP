#include "sgn/source_options_page.hpp"
#include "sgn/source_audio_options_page.hpp"
#include "sgn/host_graphics_page.hpp"
#include "sgn/source_start_mode2.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_title_page.hpp"
#include "sgn/source_frontend_packed_word.hpp"
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
Word initialize_options_454ce0(FrontendState &f, Word page, SourceNavigationPageServices &pages)
{
    const auto count = f.source.source_word(0x5e38dc); // 454CE0, before either selector store.
    f.source.source_word(0x488380) = 0x4882b8;         // 454CED, always first.
    if (std::bit_cast<std::int32_t>(count) >= 2)
        f.source.source_word(0x488380) = 0x4881f8;
    const auto child = allocate_frontend_page_object_43d470(page_pool(f), 0xc8); // 454D08.
    const auto bytes = page_object_bytes(f, child, 0xc8);
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    const auto close = f.source.source_word(0x4794e8);
    const auto callback = f.source.source_word(0x4794ec);
    set_page_word(f, child, 12, close);
    set_page_word(f, child, 16, callback);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x488390);
    set_page_word(f, child, 8, 0);
    // NEW HOST (R23): the child reads a host copy of descriptor488358 whose rows
    // are the table454CE0 just selected (488380 keeps that source value) plus
    // the appended GRAPHICS row. Source rows, order and IDs are unchanged.
    const auto descriptor = build_host_options_descriptor_r23(f, 0x488358);
    if (initialize_frontend_menu_text_43c860(f, child, descriptor, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        set_page_word(f, page, 20, 0);
    }
    else
        set_page_word(f, page, 20, child);
    const auto retained = page_word(f, page, 20); // 454D5C, retain ESI across both virtual calls.
    pages.page_event(f, retained, 24, 0);         // 454D67, existing+10 dispatch, ignored EAX.
    pages.page_event(f, retained, 22, 0);         // 454D72, no child pointer reload.
    return 1;                                     // 454D75, only after both calls return.
}
} // namespace
Word create_frontend_options_page_43e000(FrontendState &f, Word key, SourceNavigationPageServices &pages)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 24); // 43E685.
    const auto close = f.source.source_word(0x488388);                        // 43E68A before REP clear.
    const auto bytes = page_object_bytes(f, page, 24);
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    set_page_word(f, page, 8, 0);                         // 43E69D.
    const auto callback = f.source.source_word(0x48838c); // 43E6A0.
    set_page_word(f, page, 12, close);
    set_page_word(f, page, 4, 13);
    set_page_word(f, page, 0, 0x479780);
    set_page_word(f, page, 16, callback);
    if (initialize_options_454ce0(f, page, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), page);
        return 0;
    }
    set_page_word(f, page, 8, key); // 43ED94, common factory publication after initializer.
    return page;
}
void destroy_frontend_options_page_454d80(FrontendState &f, Word page)
{
    f.shared_platform()->require_owner_thread();
    const auto child = page_word(f, page, 20); // 454D84 retained in ESI across close.
    if (page_word(f, child, 12) != 0x43c970u)
        throw FormatError("454D89 reached unreconstructed MenuText destructor identity");
    // The complete43C970 child is RET. Parent caller then releases its child.
    release_frontend_page_object_43d4c0(page_pool(f), child); // 454D8E.
    set_page_word(f, page, 20, 0); // 454D93, only after release. Parent release stays with page system.
}
Word dispatch_frontend_options_page_454da0(FrontendState &f, Word page, Word event, SourcePageFactoryServices &factory,
                                           SourceNavigationPageServices &pages, CampaignScoreMusic &music,
                                           SourceMainPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 19:
        f.source.source_word(0x5ff46c) = 1; // 4400A0 from454DC6.
        set_frontend_packed_word_43dd10(f.source, 0x114311);
        f.source.source_word(0x5ff448) = 99;
        f.source.source_word(0x5ff44c) = 0x3e99999a;
        f.source.source_word(0x5ff450) = 0xbf000000;
        (void)calls.menu_text_event(f, page_word(f, page, 20), event, 0);
        return 0;
    case 16: {
        const auto child = page_word(f, page, 20); // 454DFA retains ESI across16 and26.
        (void)calls.menu_text_event(f, child, event, 0);
        const auto selection = calls.menu_text_event(f, child, 26, 0);
        // Return26 is the row's ID, NOT its physical row index. Audio's one
        // actual454E2A branch remains owned by the existing kind17 package.
        if (route_frontend_audio_option_454e2a(f, selection, factory, pages, music))
            return 0;
        // NEW HOST (R23): the appended GRAPHICS row ID is outside0..5/99.
        if (route_host_graphics_option_r23(f, selection, pages, music))
            return 0;
        switch (selection)
        {
        case 1:
            (void)push_frontend_page_43dfa0(f, 0, 18, factory, pages, music);
            break; // 454E44.
        case 2:
            (void)push_frontend_page_43dfa0(f, 0, 19, factory, pages, music);
            break; // 454E57.
        case 3:
            (void)push_frontend_page_43dfa0(f, 0, 16, factory, pages, music);
            break; // 454E6A.
        case 4:
            f.source.source_word(0x5e37b4) = 0; // 454E7D, remains if factory fails.
            (void)push_frontend_page_43dfa0(f, 0, 29, factory, pages, music);
            break; // 454E87.
        case 5:
            (void)push_frontend_page_43dfa0(f, 0, 30, factory, pages, music);
            break; // 454E9A.
        case 99:
            (void)push_frontend_page_43dfa0(f, 0, 26, factory, pages, music);
            break; // 454EAD.
        default:
            break; // JA>99 and table holes return0, no fabricated selection.
        }
        return 0;
    }
    case 17:
        calls.feedback_440890(f, event);                                // 454EBB always BEFORE pop, even empty stack.
        (void)pop_frontend_page_43eef0(f, pages, music);                // 454EC0, return ignored.
        request_frontend_save_page_43d500(f, 0, factory, pages, music); // 454EC7 even when pop returns0.
        return 0;
    case 3:
        calls.background_4392b0(f, 73, 0); // 454EDA before a fresh child pointer read.
        [[fallthrough]];
    case 2:
    case 11:
    case 12:
    case 20:
        (void)calls.menu_text_event(f, page_word(f, page, 20), event, 0);
        break;
    default:
        break;
    }
    return 0;
}
} // namespace sgn::retail
