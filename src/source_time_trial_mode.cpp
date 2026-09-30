#include "sgn/source_time_trial_mode.hpp"
#include "sgn/source_time_trial_phases.hpp"
#include "sgn/source_time_trial_save.hpp"
#include "sgn/source_title_page.hpp"
#include "sgn/source_start_mode2.hpp"
#include "sgn/source_frontend_initialization.hpp"
#include "sgn/source_selection_data.hpp"
#include "sgn/retail_frontend_language.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_persistence.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/frontend_page_words.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using sgn::retail::frontend_page_words::source_word;
W read(FrontendState &f, W address)
{
    return persistence_word(f.source.source_bytes(address, 4), 0);
}
void put(FrontendState &f, W address, W value)
{
    persistence_word(f.source.source_bytes(address, 4), 0, value);
}
bool initials_match(FrontendState &f, W literal)
{
    const auto source = 0x5e3a34u + (source_word(f, 0x5e3798) << 6u); // fresh slot, x86 wrap
    // 44D85D/44D89C REPE CMPSB reads only reached bytes, including the fourth
    // NUL, and stops at the first mismatch. No strlen or three-byte shortcut.
    for (W i = 0; i < 4u; ++i)
    {
        const auto selected = f.source.source_bytes(source + i, 1)[0];
        const auto expected = f.source.source_bytes(literal + i, 1)[0];
        if (selected != expected)
            return false;
    }
    return true;
}
} // namespace
void set_source_unlocks_43c310(FrontendState &f, std::uint8_t value)
{
    f.shared_platform()->require_owner_thread();
    const auto repeated = W(value) * 0x01010101u; // 43C313..323 replicates only CL
    for (W address = 0x5df569; address < 0x5df57d; address += 4u)
        put(f, address, repeated);                 // five unaligned DWORD writes, original order
    f.source.source_bytes(0x5df57d, 1)[0] = value; // 43C347
    for (W address = 0x5df581; address < 0x5df599; address += 4u)
        put(f, address, repeated); // 43C355 REP STOSD, six DWORDs
    put(f, 0x5df5d4, 1);           // 43C357, irrespective of input byte
}
void advance_frontend_time_trial_mode_44d740(FrontendState &f, SourcePageFactoryServices &factory,
                                             SourceNavigationPageServices &pages, CampaignScoreMusic &music)
{
    f.shared_platform()->require_owner_thread();
    auto push = [&](W key, W kind) { (void)push_frontend_page_43dfa0(f, key, kind, factory, pages, music); };
    switch (source_word(f, 0x5e3794))
    {
    case 0: // 44D75C..44D78E
        source_word(f, 0x5e3798) = 0;
        source_word(f, 0x5e379c) = 1;
        source_word(f, 0x5e3794) = 1;
        source_word(f, 0x61c1c0) = 0;
        source_word(f, 0x61c1c4) = 0;
        source_word(f, 0x5e39d4) = 0;
        source_word(f, 0x5e3954) = 0;
        return;
    case 1: // 44D78F..44D7AE; phase before factory callback
        source_word(f, 0x5e3794) = 2;
        push(39, 9);
        return;
    case 2: // 44D7AF..44D829
        if (source_word(f, 0x5e3a30) != 0)
        {
            (void)pop_frontend_page_43eef0(f, pages, music);
            return;
        }
        source_word(f, 0x5e38f8) = 0;
        for (W car = 1; car < 21u; ++car)
        {
            const auto available = source_car_available_437230(f, car, source_word(f, 0x5e3790));
            if (available != 0 || read(f, 0x5df5d4) != 0)
            {
                const auto count = source_word(f, 0x5e38f8);
                source_word(f, 0x5e38fc + count * 4u) = car;
                ++source_word(f, 0x5e38f8); // 44D7F9 reread after the row store
            }
        }
        source_word(f, 0x5e3794) = 3;
        push(40, 10);
        return;
    case 3: // 44D82A..44D94F
        if (source_word(f, 0x5e38f4) != 0)
        {
            const auto code_state = source_word(f, 0x61c204);
            if (code_state == 0)
            {
                if (initials_match(f, 0x48642c))
                {
                    source_word(f, 0x61c204) = 1;
                    source_word(f, 0x5e3794) = 1;
                    return;
                }
            }
            else if (code_state == 1 && initials_match(f, 0x486430))
            {
                set_source_unlocks_43c310(f, 1);     // 44D8A2
                initialize_source_records_43c130(f); // 44D8A7, after unlocks
                source_word(f, 0x5e3c1c) = 0;
                source_word(f, 0x5e3c20) = 1;
                source_word(f, 0x5e3c28) = 3;
                source_word(f, 0x5e3c24) = 0;
                push(0, 27); // 44D8D3 precedes both final stores
            }
            source_word(f, 0x61c204) = 0;
            source_word(f, 0x5e3794) = 1;
            return;
        }
        source_word(f, 0x5e3968) = 0;
        for (W track = 1; track < 25u; ++track)
        {
            if (source_track_available_4371c0(f, track, source_word(f, 0x5e3790)) != 0)
            {
                (void)source_track_value_437da0(track); // 44D90D, EAX discarded
                const auto count = source_word(f, 0x5e3968);
                source_word(f, 0x5e396c + count * 4u) = track;
                ++source_word(f, 0x5e3968); // 44D91F fresh read
            }
        }
        source_word(f, 0x5e3794) = 4;
        push(41, 11);
        return;
    case 4:
    case 5: // 44D950..44DB31, accepted source child
        advance_frontend_time_trial_phases4_5_44d740(f, factory, pages, music);
        return;
    case 6: // 44DB32..44DB51; source phase store AFTER callback
        push(43, 40);
        source_word(f, 0x5e3794) = 7;
        return;
    case 7: {                                        // 44DB52..44DBBE
        const auto track = source_word(f, 0x5e39d4); // retained ESI across441170
        source_word(f, 0x5e3bf4) = 0x1fc;
        const auto text = frontend_language_text_441170(f.source.language_strings(), track + 0x1b7u);
        source_word(f, 0x5e3bf8) = text.value; // same native identity used by HighScore39
        source_word(f, 0x5e3bfc) = 3;
        source_word(f, 0x5e3c00) = 10;
        source_word(f, 0x5e3794) = 8;
        source_word(f, 0x5e3c04) = 0x5dfbf8u + track * 84u;
        push(42, 39);
        return;
    }
    case 8: // 44DBBF..44DBE3; full43BF30, then phase, then callback
        clear_persistence_record_changes(f.source.persistence_view());
        source_word(f, 0x5e3794) = 9;
        push(43, 40);
        return;
    case 9: // 44DBE4..44DC05
        source_word(f, 0x61c1c8) = 0;
        source_word(f, 0x5e3794) = 10;
        request_frontend_save_page_43d500(f, 0, factory, pages, music);
        return;
    case 10: { // 44DC06..44DC29: DEC wraps; JG is signed after TEST
        const auto decremented = source_word(f, 0x61c1c8) - 1u;
        source_word(f, 0x61c1c8) = decremented;
        if (std::bit_cast<std::int32_t>(decremented) <= 0)
            source_word(f, 0x5e3794) = 11;
        return;
    }
    case 11: // 44DC2A..44DCC6, Main's source child
        advance_time_trial_save_phase11_44dc2a(f, factory, pages, music);
        return;
    case 12:
        source_word(f, 0x5e3794) = 3;
        return; // 44DCC7
    default:
        return; // 44D74F unsigned JA, including negative DWORD patterns
    }
}
} // namespace sgn::retail
