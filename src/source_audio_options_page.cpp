#include "sgn/source_audio_options_page.hpp"
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
using U = std::uint32_t;
using sgn::retail::frontend_page_words::page_object_bytes;
using sgn::retail::frontend_page_words::page_pool;
using sgn::retail::frontend_page_words::page_word;
using sgn::retail::frontend_page_words::set_page_word;
U setting(FrontendState &f, U address)
{
    return persistence_word(f.source.persistence.bytes, address - 0x5df560u);
}
void setting(FrontendState &f, U address, U value)
{
    persistence_word(f.source.persistence.bytes, address - 0x5df560u, value);
}
U clamp100(U bits)
{
    return static_cast<U>(std::clamp(std::bit_cast<std::int32_t>(bits), 0, 100));
}
} // namespace
bool route_frontend_audio_option_454e2a(FrontendState &f, U selection, SourcePageFactoryServices &factory,
                                        SourceNavigationPageServices &pages, CampaignScoreMusic &music)
{
    if (selection != 0)
        return false;
    (void)push_frontend_page_43dfa0(f, 0, 17, factory, pages, music);
    return true;
}
U create_frontend_audio_options_43e000(FrontendState &f, U music_key, SourceNavigationPageServices &pages,
                                       SourceMainPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 24);
    const auto parent = page_object_bytes(f, page, 24);
    std::fill(parent.begin(), parent.end(), std::byte{0});
    const auto callback = f.source.source_word(0x488574);
    set_page_word(f, page, 8, 0);
    const auto close = f.source.source_word(0x488570);
    set_page_word(f, page, 12, close);
    set_page_word(f, page, 16, callback);
    set_page_word(f, page, 4, 15);
    set_page_word(f, page, 0, 0x47979c);
    // Full455340 initializer: the child shares the existing page pool.
    const auto child = allocate_frontend_page_object_43d470(page_pool(f), 200);
    const auto bytes = page_object_bytes(f, child, 200);
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    const auto child_close = f.source.source_word(0x4794e8);
    const auto child_callback = f.source.source_word(0x4794ec);
    set_page_word(f, child, 12, child_close);
    set_page_word(f, child, 16, child_callback);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x488578);
    set_page_word(f, child, 8, 0);
    if (initialize_frontend_menu_text_43c860(f, child, 0x488540, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        set_page_word(f, page, 20, 0);
    }
    else
        set_page_word(f, page, 20, child);
    const auto retained = page_word(f, page, 20);
    (void)calls.menu_text_event(f, retained, 24, 0);
    (void)calls.menu_text_event(f, retained, 22, 0);
    const auto music = clamp100(setting(f, 0x5df5dc));
    const auto effects = setting(f, 0x5df5e0); // 4553C6 before publishing clamped music
    setting(f, 0x5df5dc, music);
    const auto clamped_effects = clamp100(effects);
    setting(f, 0x5df5e0, clamped_effects - clamped_effects % 10u);
    const auto live_music = setting(f, 0x5df5dc);
    setting(f, 0x5df5dc, live_music - static_cast<U>(std::bit_cast<std::int32_t>(live_music) % 10));
    set_page_word(f, page, 8, music_key); // 43ED94 after initializer returned1
    return page;
}
void destroy_frontend_audio_options_455410(FrontendState &f, U page)
{
    f.shared_platform()->require_owner_thread();
    const auto child = page_word(f, page, 20);
    if (page_word(f, child, 12) != 0x43c970u)
        throw FormatError("455410 child destructor is outside MenuText43C970");
    // 43C970 is RET.45541B retains the child across that call before free.
    release_frontend_page_object_43d4c0(page_pool(f), child);
    set_page_word(f, page, 20, 0);
}
U dispatch_frontend_audio_options_455430(FrontendState &f, U page, U event, SourceNavigationPageServices &pages,
                                         CampaignScoreMusic &music, SourceMainPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 19:
        f.source.source_word(0x5ff46c) = 1;
        set_frontend_packed_word_43dd10(f.source, 0x114311);
        f.source.source_word(0x5ff448) = 99;
        f.source.source_word(0x5ff44c) = 0x3e99999a;
        f.source.source_word(0x5ff450) = 0xbf000000;
        (void)calls.menu_text_event(f, page_word(f, page, 20), event, 0);
        return 0;
    case 16: {
        const auto child = page_word(f, page, 20);
        (void)calls.menu_text_event(f, child, event, 0);
        const auto selected = calls.menu_text_event(f, child, 26, 0);
        if (selected == 0 || selected == 1)
        {
            const auto address = selected == 0 ? 0x5df5dcu : 0x5df5e0u;
            const auto next = setting(f, address) + 10u;
            setting(f, address, next);
            if (std::bit_cast<std::int32_t>(next) > 100)
                setting(f, address, 0);
        }
        else if (selected == 2)
        {
            auto &mode = f.source.source_word(0x5e37a0);
            if (mode == 2)
                mode = 1;
            else if (mode == 1)
                mode = 2;
        }
        return 0;
    }
    case 17:
        if (pop_frontend_page_43eef0(f, pages, music) != 0)
            calls.feedback_440890(f, event);
        return 0;
    case 3: {
        calls.background_4392b0(f, 77, 0);
        const auto a = static_cast<U>(std::bit_cast<std::int32_t>(setting(f, 0x5df5dc)) / 10) + 0x6du;
        f.source.source_word(f.source.source_word(0x488568) + 12u) = a;
        const auto b = static_cast<U>(std::bit_cast<std::int32_t>(setting(f, 0x5df5e0)) / 10) + 0x61u;
        f.source.source_word(f.source.source_word(0x488568) + 44u) = b;
        [[fallthrough]];
    }
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
