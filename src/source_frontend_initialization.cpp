#include "sgn/source_frontend_initialization.hpp"
#include "sgn/retail_frontend_initialization.hpp"
#include "sgn/source_frontend_catalog.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/source_selection_data.hpp"
#include "sgn/race_session.hpp"
#include "sgn/source_application_literals.hpp"
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
void put(FrontendState &f, W a, W v)
{
    persistence_word(f.source.source_bytes(a, 4), 0, v);
}
void byte(FrontendState &f, W a, std::uint8_t v)
{
    f.source.source_bytes(a, 1)[0] = v;
}
void initials(FrontendState &f, W to, W source)
{
    const auto app = f.shared_platform()->source_application_literals();
    const auto seeds = app.frontend_csv_literal(source);
    const auto dest = f.source.source_bytes(to, 3);
    dest[0] = seeds[0];
    dest[1] = seeds[1];
    dest[2] = seeds[2];
    // Source writes WORD + BYTE only: fourth byte is deliberately retained.
}
} // namespace
void initialize_source_unlocks_43c370(FrontendState &f)
{
    f.shared_platform()->require_owner_thread();
    for (W a = 0x5df568; a < 0x5df59c; a += 4)
        put(f, a, 0); // original13 STOSD, not a second cold clear
    for (const W a :
         {0x5df569u, 0x5df56cu, 0x5df56fu, 0x5df570u, 0x5df571u, 0x5df572u, 0x5df573u, 0x5df577u, 0x5df57du})
        byte(f, a, 1);
    for (W track = 1; track < 25; ++track)
    {
        const auto type = source_track_type_437360(track);
        byte(f, 0x5df580 + track, (type == 1 || type == 2) ? 0 : 1);
    }
    byte(f, 0x5df586, 1);
    byte(f, 0x5df58b, 1);
    byte(f, 0x5df597, 1);
    copy_persistence_comparison(f.source.persistence_view()); // 43C3F8 tail43C400, before records initialization
}
void initialize_source_records_43c130(FrontendState &f)
{
    f.shared_platform()->require_owner_thread();
    for (W p = 0x5df678; p < 0x5df8d8; p += 32)
    {
        put(f, p - 4, 7200);
        initials(f, p, 0x479420);
        put(f, p + 4, 36000);
        initials(f, p + 8, 0x479424);
        put(f, p + 12, 0);
        initials(f, p + 16, 0x479428);
        put(f, p + 20, 0);
        initials(f, p + 24, 0x47942c);
    }
    for (W p = 0x5df8d8; p < 0x5dfbf8; p += 8)
    {
        put(f, p - 4, 0);
        initials(f, p, 0x479430);
    }
    for (W p = 0x5e1360; p < 0x5e1680; p += 8)
    {
        put(f, p - 4, 0);
        initials(f, p, 0x479434);
    }
    for (W p = 0x5dfbfc; p < 0x5e0430; p += 84)
    {
        put(f, p - 8, 0);
        for (W k = 0; k < 10; ++k)
        {
            put(f, p + k * 8u - 4u, 7200);
            initials(f, p + k * 8u, 0x479438);
        }
    }
    for (W p = 0x5e0f7c; p < 0x5e129c; p += 32)
    {
        initials(f, p + 4, 0x47943c);
        put(f, p, 7200);
        initials(f, p + 12, 0x479440);
        put(f, p + 8, 36000);
        initials(f, p + 20, 0x479444);
        put(f, p + 16, 0);
        initials(f, p + 28, 0x479448);
        put(f, p + 24, 0);
    }
    for (W p = 0x5e12bc; p < 0x5e130c; p += 8)
    {
        initials(f, p + 4, 0x47944c);
        put(f, p, 0);
    }
    for (W p = 0x5e130c; p < 0x5e135c; p += 8)
    {
        initials(f, p + 4, 0x479450);
        put(f, p, 0);
    }
}
void continue_source_frontend_43d114(FrontendState &f, SourceBootFilePlatform &files, SourceMenuUpdateChildren &menu,
                                     SourceNavigationPageServices &pages, PersistenceServices &persistence,
                                     SourceFrontendDataStartup &data)
{
    f.shared_platform()->require_owner_thread();
    set_frontend_packed_word_43dd10(f.source, 0); // 43D114 XOR ECX before43D116
    menu.apply_options_43c000(f);                 // 43D11B
    data.initialize_data_434b50(f, files);        // 43D120, must complete before any later stores/menu publication
    initialize_source_unlocks_43c370(f);
    initialize_source_records_43c130(f);
    // 445AD0 is XOR EAX,EAX;RET. Its zero is passed verbatim to440C40.
    if (menu.select_language_440c40(f, 0) < 0)
        (void)menu.select_language_440c40(f,
                                          f.source.source_word(0x5e37dc)); // fresh source catalog read after first call
    read_persistence_auxiliary(f.source.persistence_view(), persistence);  // 43D14A
    (void)pages.select_page_43ef10(f, 0, 5); // actual title factory, key0; no substitute screen
    for (W track = 1; track < 25; ++track)
    {
        (void)source_track_type_437360(track);
        (void)source_track_value_437da0(track);
    } // actual final1..24 calls; both leaves are pure value switches
}
void initialize_source_frontend_43d0b0(FrontendState &f, SourceBootFilePlatform &files, SourceMenuUpdateChildren &menu,
                                       SourceNavigationPageServices &pages, PersistenceServices &persistence,
                                       SourceFrontendDataStartup &data)
{
    const auto stop = initialize_frontend_prefix_43d0b0(f.source, f.world().frontend_launch);
    if (stop != FrontendInitializationStop::before_440ae0)
        throw FormatError("43D0B0 prefix has not reached43D10F");
    (void)initialize_source_frontend_catalog_440ae0(f, files);
    continue_source_frontend_43d114(f, files, menu, pages, persistence, data);
}
} // namespace sgn::retail
