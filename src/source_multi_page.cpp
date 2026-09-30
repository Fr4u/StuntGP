#include "sgn/source_multi_page.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_title_page.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/race_session.hpp"
#include "sgn/frontend_page_words.hpp"

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
using sgn::retail::frontend_page_words::page_pool;
using sgn::retail::frontend_page_words::page_word;
using sgn::retail::frontend_page_words::set_page_word;
Word initialize_frontend_multi_page_454220(FrontendState &f, Word page, SourceNavigationPageServices &pages,
                                           SourceMainPageServices &calls)
{
    const auto child = allocate_frontend_page_object_43d470(page_pool(f), 0xc8);
    for (Word offset = 0; offset != 0xc8; offset += 4)
        set_page_word(f, child, offset, 0);            // 45423A
    const auto close = f.source.source_word(0x4794e8); // both reads AFTER clear
    const auto event = f.source.source_word(0x4794ec);
    set_page_word(f, child, 12, close);
    set_page_word(f, child, 16, event);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x4881a0);
    set_page_word(f, child, 8, 0);
    if (initialize_frontend_menu_text_43c860(f, child, 0x488168, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        set_page_word(f, page, 0x14, 0);
    }
    else
        set_page_word(f, page, 0x14, child);
    const auto retained_child = page_word(f, page, 0x14);  // ESI retained across both callbacks
    (void)calls.menu_text_event(f, retained_child, 24, 0); // 454289
    (void)calls.menu_text_event(f, retained_child, 22, 0); // 454294
    return 1;
}
} // namespace
std::uint32_t create_frontend_multi_page_43e000(FrontendState &f, Word music_key, SourceNavigationPageServices &pages,
                                                SourceMainPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 0x18); // 43E1C7
    const auto event = f.source.source_word(0x48819c);                          // 43E1D1 BEFORE clear
    for (Word offset = 0; offset != 0x18; offset += 4)
        set_page_word(f, page, offset, 0);
    const auto close = f.source.source_word(0x488198); // 43E1E4 AFTER clear
    set_page_word(f, page, 4, 4);
    set_page_word(f, page, 12, close);
    set_page_word(f, page, 16, event);
    set_page_word(f, page, 0, 0x4796ac);
    set_page_word(f, page, 8, 0);
    if (initialize_frontend_multi_page_454220(f, page, pages, calls) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), page);
        return 0;
    }
    set_page_word(f, page, 8, music_key); // 43ED94, only after initializer completed
    return page;
}
void destroy_frontend_multi_page_4542a0(FrontendState &f, Word page)
{
    f.shared_platform()->require_owner_thread();
    const auto child = page_word(f, page, 0x14); // retained ESI through destructor and free
    if (page_word(f, child, 12) != 0x43c970u)
        throw FormatError("4542A0 reached an unreconstructed MenuText destructor");
    // Complete43C970 is RET. Parent page remains owned by its navigation caller.
    release_frontend_page_object_43d4c0(page_pool(f), child);
    set_page_word(f, page, 0x14, 0);
}
std::uint32_t dispatch_frontend_multi_page_4542c0(FrontendState &f, Word page, Word event,
                                                  SourcePageFactoryServices &factory,
                                                  SourceNavigationPageServices &pages, CampaignScoreMusic &music,
                                                  SourceMainPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 1:
        if (count_frontend_players_437100(f) < 2u)
        { // 454397 tail456000 ->437100
            pages.feedback_440880(f, 2);
            (void)pop_frontend_page_43eef0(f, pages, music);
        }
        break;
    case 3:
        calls.background_4392b0(f, 188, 0); // 4543BE, BEFORE child is reread
        [[fallthrough]];
    case 2:
    case 11:
    case 12:
    case 20:
        (void)calls.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        break;
    case 16: {
        calls.feedback_440890(f, event); // 45431C; no child event16 in source
        const auto selected = calls.menu_text_event(f, page_word(f, page, 0x14), 26, 0);
        if (selected == 0 || selected == 1)
        {
            f.source.source_word(0x5e3794) = 0;
            f.source.source_word(0x5e3790) = selected == 0 ? 6u : 7u;
            (void)push_frontend_page_43dfa0(f, 0, 36, factory, pages, music);
        }
        break;
    }
    case 17:
        if (pop_frontend_page_43eef0(f, pages, music) != 0)
            calls.feedback_440890(f, event);
        break;
    case 19:
        f.source.source_word(0x5ff46c) = 1; // 4400A0
        set_frontend_packed_word_43dd10(f.source, 0x114311);
        f.source.source_word(0x5ff448) = 99;
        f.source.source_word(0x5ff44c) = 0x3e99999a;
        f.source.source_word(0x5ff450) = 0xbf000000;
        (void)calls.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        break;
    default:
        break; // actual4543F0 remap and4543D4 jump table common return
    }
    return 0;
}
} // namespace sgn::retail
