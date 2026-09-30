#include "sgn/source_video_options_page.hpp"
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
Word setting(FrontendState &f)
{
    return persistence_word(f.source.persistence.bytes, 0x84);
}
void setting(FrontendState &f, Word value)
{
    persistence_word(f.source.persistence.bytes, 0x84, value);
}
Word initialize_video_options_4555e0(FrontendState &f, Word page, SourceNavigationPageServices &pages)
{
    const auto child = allocate_frontend_page_object_43d470(page_pool(f), 200); // 4555EA.
    const auto bytes = page_object_bytes(f, child, 200);
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    const auto close = f.source.source_word(0x4794e8); // 4555FC after REP clear.
    const auto callback = f.source.source_word(0x4794ec);
    set_page_word(f, child, 12, close);
    set_page_word(f, child, 16, callback);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x488600);
    set_page_word(f, child, 8, 0);
    if (initialize_frontend_menu_text_43c860(f, child, 0x4885c8, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        set_page_word(f, page, 20, 0);
    }
    else
        set_page_word(f, page, 20, child);
    const auto retained = page_word(f, page, 20); // 45563E: same ESI child for both virtual calls.
    pages.page_event(f, retained, 24, 0);         // 455649.
    pages.page_event(f, retained, 22, 0);         // 455654; no child/settings reload before this returns.
    const auto current = std::bit_cast<std::int32_t>(setting(f)); // 455657 actual5DF5E4.
    const auto clamped = std::clamp(current, 0, 100);             // 45565D..455665, full43D790 signed clamp.
    setting(f, static_cast<Word>(clamped - clamped % 10));        // 45566D..45567F: signed IDIV10 then SUB.
    return 1;
}
} // namespace
Word create_frontend_video_options_page_43e000(FrontendState &f, Word key, SourceNavigationPageServices &pages)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 24); // 43E788.
    const auto close = f.source.source_word(0x4885f8);                        // 43E78D BEFORE REP clear.
    const auto bytes = page_object_bytes(f, page, 24);
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    set_page_word(f, page, 8, 0);                         // 43E7A0.
    const auto callback = f.source.source_word(0x4885fc); // 43E7A3 AFTER clear.
    set_page_word(f, page, 12, close);
    set_page_word(f, page, 4, 16);
    set_page_word(f, page, 0, 0x4797ac);
    set_page_word(f, page, 16, callback);
    if (initialize_video_options_4555e0(f, page, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), page);
        return 0;
    }
    set_page_word(f, page, 8, key); // 43ED94 common publication only after successful initializer.
    return page;
}
void destroy_frontend_video_options_page_455690(FrontendState &f, Word page)
{
    f.shared_platform()->require_owner_thread();
    const auto child = page_word(f, page, 20); // 455694 retains child across destructor.
    if (page_word(f, child, 12) != 0x43c970u)
        throw FormatError("455699 reached unreconstructed MenuText destructor identity");
    // Complete child43C970 is RET. The source caller then frees the same child.
    release_frontend_page_object_43d4c0(page_pool(f), child); // 45569E.
    set_page_word(f, page, 20, 0); // 4556A3 only after release; page system still owns parent release.
}
Word dispatch_frontend_video_options_page_4556b0(FrontendState &f, Word page, Word event,
                                                 SourcePageFactoryServices &factory,
                                                 SourceNavigationPageServices &pages, CampaignScoreMusic &music,
                                                 SourceMainPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 19:
        f.source.source_word(0x5ff46c) = 1; // 4556D6 ->4400A0(1).
        set_frontend_packed_word_43dd10(f.source, 0x114311);
        f.source.source_word(0x5ff448) = 99;
        f.source.source_word(0x5ff44c) = 0x3e99999a;
        f.source.source_word(0x5ff450) = 0xbf000000;
        (void)calls.menu_text_event(f, page_word(f, page, 20), event, 0);
        return 0;
    case 16: {
        const auto child = page_word(f, page, 20); // 45570A retains EDI across16 and26.
        (void)calls.menu_text_event(f, child, event, 0);
        const auto selected = calls.menu_text_event(f, child, 26, 0); // row ID, not index.
        switch (selected)
        {
        case 0: {
            const auto next = setting(f) + 10u; // 455753..45575A: wrapping x86 ADD.
            setting(f, next);                   // 45575E store BEFORE signed JLE result handling.
            if (std::bit_cast<std::int32_t>(next) > 100)
                setting(f, 0); // 455767.
            break;
        }
        case 1:
            (void)push_frontend_page_43dfa0(f, 0, 4, factory, pages, music);
            break; // 455747.
        case 2:
            (void)push_frontend_page_43dfa0(f, 0, 22, factory, pages, music);
            break; // 455734.
        default:
            break;
        }
        return 0;
    }
    case 17:
        if (pop_frontend_page_43eef0(f, pages, music) != 0)
            calls.feedback_440890(f, event); // 455776..455781.
        return 0;
    case 3:
        calls.background_4392b0(f, 80, 0); // 455794, then fresh child pointer read.
        [[fallthrough]];
    case 2:
    case 11:
    case 12:
    case 20:
        (void)calls.menu_text_event(f, page_word(f, page, 20), event, 0);
        break;
    default:
        break; // All holes/out-of-range events reach source common return0.
    }
    return 0;
}
} // namespace sgn::retail
