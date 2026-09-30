#include "sgn/source_main_page_navigation.hpp"
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
using sgn::retail::frontend_page_words::page_word;
using sgn::retail::frontend_page_words::set_page_word;
} // namespace
void open_frontend_main_exit_453430(FrontendState &f, Word page, SourceMainPageServices &calls)
{
    const auto child = page_word(f, page, 0x18); // 45343B retained ESI across both callbacks
    (void)calls.menu_text_event(f, child, 19, 0);
    (void)calls.menu_text_event(f, child, 28, 2);
    set_page_word(f, page, 0x1c, 1); // 45344F AFTER both returned
}
void close_frontend_main_exit_453460(FrontendState &f, Word page, SourceMainPageServices &calls)
{
    const auto child = page_word(f, page, 0x18);
    (void)calls.menu_text_event(f, child, 20, 0);
    set_page_word(f, page, 0x1c, 0); // 453470 after callback
}
Word dispatch_frontend_main_page_453210(FrontendState &f, Word page, Word event, Word,
                                        SourcePageFactoryServices &factory, SourceNavigationPageServices &pages,
                                        CampaignScoreMusic &music, SourceMainPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 1:
        refresh_frontend_main_page_4531a0(f);
        break;
    case 3:
        calls.background_4392b0(f, 62, 0); // 4533AC..B3 before either child
        [[fallthrough]];
    case 2:
        (void)calls.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        (void)calls.menu_text_event(f, page_word(f, page, 0x18), event, 0); // reread after first child
        break;
    case 11:
    case 12:
        (void)calls.menu_text_event(f, page_word(f, page, page_word(f, page, 0x1c) == 0 ? 0x14u : 0x18u), event, 0);
        break;
    case 16:
        if (page_word(f, page, 0x1c) != 0)
        {
            const auto child = page_word(f, page, 0x18); // EBX retained, not reloaded after event16
            (void)calls.menu_text_event(f, child, event, 0);
            if (calls.menu_text_event(f, child, 26, 0) == 1)
            {
                close_frontend_main_exit_453460(f, page, calls);
                return 0;
            }
            calls.exit_462dfa(f, 0);
            throw FormatError("462DFA exit child returned contrary to source nonreturning contract");
        }
        else
        {
            const auto child = page_word(f, page, 0x14);
            (void)calls.menu_text_event(f, child, event, 0);
            const auto selected = calls.menu_text_event(f, child, 26, 0);
            switch (selected)
            {
            case 0:
                (void)push_frontend_page_43dfa0(f, 3, 3, factory, pages, music);
                break;
            case 1:
                (void)push_frontend_page_43dfa0(f, 4, 6, factory, pages, music);
                break;
            case 2:
                (void)push_frontend_page_43dfa0(f, 5, 8, factory, pages, music);
                break;
            case 3:
                (void)push_frontend_page_43dfa0(f, 6, 21, factory, pages, music);
                break;
            case 4:
                (void)push_frontend_page_43dfa0(f, 7, 15, factory, pages, music);
                break;
            case 5:
                open_frontend_main_exit_453430(f, page, calls);
                break;
            default:
                break;
            }
        }
        break;
    case 17:
        if (page_word(f, page, 0x1c) != 0)
        {
            close_frontend_main_exit_453460(f, page, calls);
            calls.feedback_440890(f, event);
        }
        else if (pop_frontend_page_43eef0(f, pages, music) != 0)
            calls.feedback_440890(f, event);
        break;
    case 19:
        f.source.source_word(0x5ff46c) = 1; // 4400A0
        set_frontend_packed_word_43dd10(f.source, 0x114311);
        f.source.source_word(0x5ff448) = 99;
        f.source.source_word(0x5ff44c) = 0x3e99999a;
        f.source.source_word(0x5ff450) = 0xbf000000;
        [[fallthrough]];
    case 20:
        (void)calls.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        break;
    default:
        break; // source event jump table's common return
    }
    return 0;
}
} // namespace sgn::retail
