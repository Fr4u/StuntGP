#include "sgn/source_time_trial_phases.hpp"
#include "sgn/source_time_trial_launch.hpp"
#include "sgn/source_title_page.hpp"
#include "sgn/source_replay_decompress.hpp"
#include "sgn/source_selection_data.hpp"
#include "sgn/retail_championship.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_signed;
W read(FrontendState &f, W address, W count = 4)
{
    const auto b = f.source.source_bytes(address, count);
    W value = 0;
    for (W i = 0; i < count; ++i)
        value |= W(b[i]) << (i * 8u);
    return value;
}
void write(FrontendState &f, W address, W value, W count = 4)
{
    const auto b = f.source.source_bytes(address, count);
    for (W i = 0; i < count; ++i)
        b[i] = static_cast<std::uint8_t>(value >> (i * 8u));
}
void unlock_437880(FrontendState &f, W car)
{
    const auto flag = std::as_writable_bytes(f.source.source_bytes(0x5df568u + car, 1));
    unlock_campaign_car_437880(std::span<std::byte, 1>{flag.data(), 1});
}
W insert_record_437800(FrontendState &f, W table, W count, W initials, W value)
{
    W row = 0;
    if (as_signed(count) <= 0)
        return 0xffffffffu;
    for (; as_signed(row) < as_signed(count); ++row)
    {
        if (as_signed(value) < as_signed(read(f, table + row * 8u)))
            break; // strict signed comparison
    }
    if (row == count)
        return 0xffffffffu;
    auto last = count - 1u;
    while (as_signed(last) > as_signed(row))
    {
        const auto destination = table + last * 8u;
        const auto time = read(f, destination - 8u);
        write(f, destination, time);
        const auto name = read(f, destination - 4u);
        write(f, destination + 4u, name);
        --last;
    }
    const auto destination = table + row * 8u;
    write(f, destination + 7u, 1, 1); // 437847 flag BEFORE record and initials reads
    write(f, destination, value);
    const auto first = read(f, initials, 2);
    write(f, destination + 4u, first, 2);
    const auto third = read(f, initials + 2u, 1);
    write(f, destination + 6u, third, 1);
    return row;
}
W source_recording_word(FrontendState &f, W address, W at)
{
    const auto offset = address - ReplayBank::source_base;
    if (offset >= ReplayBank::size || offset % ReplayBank::stride != 0)
        throw FormatError("TimeTrial input recording does not name an existing ReplayBank slot");
    const auto b = f.world().replays.bytes().subspan(offset + at, 4);
    return W(b[0]) | (W(b[1]) << 8) | (W(b[2]) << 16) | (W(b[3]) << 24);
}
W native_recording_word(FrontendState &f, W identity, W at)
{
    const auto handle = frontend_replay_handle(identity);
    if (!handle)
        throw FormatError("TimeTrial result dereferences a null recording identity");
    return source_recording_word(f, ReplayBank::source_base + ReplayBank::base(*handle), at);
}
void phase4(FrontendState &f, SourcePageFactoryServices &factory, SourceNavigationPageServices &pages,
            CampaignScoreMusic &music)
{
    if (read(f, 0x5e3964) != 0)
    {
        write(f, 0x5e3794, 2);
        return;
    } // 44D950..969
    const auto selected = read(f, 0x5e3798), track = read(f, 0x5e39d4);
    write(f, 0x61c1d8, track); // 44D977
    const auto model = read(f, 0x5e3954u + selected * 4u);
    write(f, 0x61c1dc, 1);
    write(f, 0x61c1d4, model);
    const auto initials = 0x5e3a34u + selected * 64u;
    const auto first = read(f, initials, 2), third = read(f, initials + 2u, 1);
    write(f, 0x61c1d0, first, 2);
    write(f, 0x61c1d2, third, 1);
    const auto record = read(f, 0x5dfbf8u + track * 84u), completed = read(f, 0x5dfbf4u + track * 84u);
    write(f, 0x61c1e0, record);
    write(f, 0x61c1e4, completed);
    if (track == read(f, 0x5e3c44))
    {
        const auto destination = read(f, 0x5f26a8);
        (void)decompress_frontend_saved_replay_44d9e9(f, destination);
        const auto retained = read(f, 0x5f26a8); // 44D9EE fresh read AFTER decoder
        write(f, 0x61c1e8, retained);
        const auto old_time = source_recording_word(f, retained, 0x1c);
        f.source.source_word(0x61c1c4) = old_time; // Main's shared progress owner
    }
    else
        write(f, 0x61c1e8, 0);                  // does NOT clear previous61C1C4 here
    prepare_frontend_time_trial_race_43a910(f); // 44DA17, actual input/result owners
    write(f, 0x5e3794, 5);                      // 44DA23 BEFORE synchronous Start25 construction
    (void)push_frontend_page_43dfa0(f, 0, 25, factory, pages, music);
}
void phase5(FrontendState &f)
{
    const auto track = read(f, 0x5e39d4), offset = track * 84u; // EDI retained across calls
    if (read(f, 0x5dfbf4u + offset) == 0 && read(f, 0x61c1f4) != 0)
    {
        unlock_437880(f, 19);
        write(f, 0x5dfbf4u + offset, 1);
    }
    bool complete = true;
    W row = 1, address = 0x5dfc48;
    do
    {
        if (!complete)
            break; // 44DA82 before437DA0
        if (source_track_value_437da0(row) != 0 && read(f, address) == 0)
            complete = false;
        address += 84;
        ++row;
    } while (as_signed(address) < as_signed(0x5e0428u));
    if (complete)
        unlock_437880(f, 20);
    const auto selected = read(f, 0x5e3798), lap = read(f, 0x61c200); // 44DABD/C2
    const auto position = insert_record_437800(f, 0x5dfbf8u + offset, 10, 0x5e3a34u + selected * 64u, lap);
    const auto recording = read(f, 0x61c1f8); // 44DAE2 retained through the gate writes
    f.source.source_word(0x61c1c0) = 0;
    if (recording != 0)
    {
        if (position == 0)
            f.source.source_word(0x61c1c0) = 1;
        const auto previous = f.source.source_word(0x61c1c4);
        if (previous != 0 && as_signed(native_recording_word(f, recording, 0x1c)) < as_signed(previous))
            f.source.source_word(0x61c1c0) = 1;
    }
    // Complete43BFE0 is RET. No invented save/refresh callback.
    write(f, 0x5e3794, 6); // 44DB21
}
} // namespace
void advance_frontend_time_trial_phases4_5_44d740(FrontendState &f, SourcePageFactoryServices &factory,
                                                  SourceNavigationPageServices &pages, CampaignScoreMusic &music)
{
    f.shared_platform()->require_owner_thread();
    switch (read(f, 0x5e3794))
    {
    case 4:
        phase4(f, factory, pages, music);
        return;
    case 5:
        phase5(f);
        return;
    default:
        throw FormatError("44D740 phase outside reconstructed4/5 body; caller must retain other required phases");
    }
}
} // namespace sgn::retail
