#include "sgn/source_single_page_navigation.hpp"
#include "sgn/source_title_page.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/race_session.hpp"
#include "sgn/frontend_page_words.hpp"
namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
using sgn::retail::frontend_page_words::page_word;
using sgn::retail::frontend_page_words::set_page_word;
} // namespace
Word dispatch_frontend_single_page_45a810(FrontendState &f, Word page, Word event, Word,
                                          SourcePageFactoryServices &factory, SourceNavigationPageServices &pages,
                                          CampaignScoreMusic &music, SourceMainPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 3:
        calls.background_4392b0(f, 177, 0);
        [[fallthrough]];
    case 2:
    case 11:
    case 12:
    case 20:
        (void)calls.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        break;
    case 16: {
        const auto child = page_word(f, page, 0x14); // retained ESI through both calls
        (void)calls.menu_text_event(f, child, event, 0);
        const auto selected = calls.menu_text_event(f, child, 26, 0);
        if (selected > 4u)
            break;
        f.source.source_word(0x5e3794) = 0;
        f.source.source_word(0x5e3790) = selected + 1u;
        (void)push_frontend_page_43dfa0(f, 0, 36, factory, pages, music);
        break;
    }
    case 17:
        if (pop_frontend_page_43eef0(f, pages, music) != 0)
            calls.feedback_440890(f, event);
        break;
    case 19:
        f.source.source_word(0x5ff46c) = 1;
        set_frontend_packed_word_43dd10(f.source, 0x114311);
        f.source.source_word(0x5ff448) = 99;
        f.source.source_word(0x5ff44c) = 0x3e99999a;
        f.source.source_word(0x5ff450) = 0xbf000000;
        (void)calls.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        break;
    default:
        break;
    }
    return 0;
}
Word dispatch_frontend_game_loop_452fe0(FrontendState &f, Word page, Word event, SourceGameLoopModeServices &modes)
{
    f.shared_platform()->require_owner_thread();
    if (event == 19)
    {
        f.source.source_word(0x5ff46c) = 0; // 452FF1 ECX0, unlike PageSingle
        set_frontend_packed_word_43dd10(f.source, 0);
        f.source.source_word(0x5ff448) = 0;
        f.source.source_word(0x5ff44c) = 0;
        f.source.source_word(0x5ff450) = 0;
        if (page_word(f, page, 0x14) == 0)
            set_page_word(f, page, 0x14, 1);
    }
    else if (event == 1 && page_word(f, page, 0x14) != 0)
    {
        const auto mode = f.source.source_word(0x5e3790);
        constexpr Word entries[]{0x44b5e0, 0x44cd70, 0x44d740, 0x44d4c0, 0x44c0a0, 0x44d100, 0x44dd10};
        if (mode - 1u < 7u)
            modes.run_mode(f, page, entries[mode - 1u]);
    }
    return 0;
}
} // namespace sgn::retail
