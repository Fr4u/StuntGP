#include "sgn/source_time_trial_selection.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/source_boot_config.hpp"
#include "sgn/vehicle_stunt_awards.hpp"
#include <cstring>
#include <optional>
#include <type_traits>

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
static_assert(sizeof(SelectionRecord) == 0x150 && std::is_trivially_copyable_v<SelectionRecord>);
Word replay_read(RaceSessionPlatform &platform, ReplayHandle handle, Word offset, Word width, Word pc)
{
    if (handle.slot >= ReplayBank::slots)
        throw SourceBootConfigError(pc, "Reached replay read outside bank slot identity");
    const auto at = std::uint64_t(handle.slot) * ReplayBank::stride + offset;
    const auto bytes = platform.state().replays.bytes();
    if (at > bytes.size() || bytes.size() - at < width)
        throw SourceBootConfigError(pc, "Reached replay read outside same bank");
    Word value = 0;
    std::memcpy(&value, bytes.data() + at, width);
    return value;
}
void copy_selection(RaceSessionPlatform &platform, Word destination)
{
    auto &records = platform.state().selection;
    const auto *from = reinterpret_cast<const unsigned char *>(&records[0]);
    auto *to = reinterpret_cast<unsigned char *>(&records[destination]);
    for (Word offset = 0; offset < 0x150; offset += 4)
    {
        Word value;
        std::memcpy(&value, from + offset, 4);
        std::memcpy(to + offset, &value, 4);
    }
}
void select_previous_443010(RaceSessionPlatform &platform, bool nonnull)
{
    auto &s = platform.state();
    s.trial_selection.availability[0] = 0; // 443012 BEFORE branch and bank read
    if (!nonnull)
        return;
    const auto ticks = replay_read(platform, {2}, 0x1c, 4, 0x44301e); // fixedslot2, not argument
    s.trial_selection.availability[0] = 1;
    s.ghosts.roles[1].replay = {2};
    s.ghosts.roles[1].vehicle = 3;
    s.ghosts.roles[1].lap_ticks = ticks;
}
void select_track_442f60(RaceSessionPlatform &platform, std::optional<ReplayHandle> source)
{
    auto &s = platform.state();
    s.trial_selection.availability[1] = 0;
    if (!source)
        return;
    s.trial_selection.availability[1] = 1; // 442F7C before first copy read
    // REP MOVSD is forward copying, not memmove/snapshot. Borrow each reached
    // DWORD from the same ReplayBank, including header bytes and aliases.
    for (Word offset = 0; offset < ReplayBank::stride; offset += 4)
    {
        const auto value = replay_read(platform, *source, offset, 4, 0x442f86);
        auto bytes = s.replays.bytes();
        std::memcpy(bytes.data() + ReplayBank::stride + offset, &value, 4);
    }
    const auto ticks = replay_read(platform, {1}, 0x1c, 4, 0x442f88);
    s.ghosts.roles[0].replay = {1};
    s.ghosts.roles[0].vehicle = 4;
    s.ghosts.roles[0].lap_ticks = ticks;
}
} // namespace
void reset_source_time_trial_ghosts_443060(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    auto &s = platform.state();
    for (auto &value : s.trial_selection.availability)
        value = 0; // 443065..74
    constexpr Word vehicles[] = {4, 3, 1, 2};
    for (Word i = 0; i < 4; ++i)
    {
        auto &role = s.ghosts.roles[i];
        role.active = 0;
        role.vehicle = vehicles[i];
        role.replay = {i + 1u};
        role.lap_ticks = 0xffffffffu;
    }
}
void prepare_source_time_trial_selection_445150(RaceSessionPlatform &platform, SourceTrackReplay442ef0 &files)
{
    platform.require_owner_thread();
    auto &s = platform.state();
    auto &selection = s.selection;
    auto value = selection[0].livery; // 445150
    if (value == 8)
        selection[0].livery = 0; // 44515A, before random call
    if (value == 8 || value == 0)
    {
        const auto draw = vehicle_random_integer(s.random, 5); // same463289stream through42F1F0
        const auto bits = static_cast<Word>(draw);
        selection[0].livery = (draw < 0 ? 0u - bits : bits) + 2u; // CDQ/XOR/SUB, no signed abs overflow
    }
    selection[0].ai_skill = 1; // 445184
    for (Word i = 1; i < 3; ++i)
    {
        copy_selection(platform, i);
        selection[i].control = 2;
    }
    const auto selected = s.trial_selection.selected.replay; // EBX captured4451B8
    if (selected)
    {
        select_previous_443010(platform, true);
        s.trial_selection.selected.header_value = 0;
        s.trial_selection.selected.replay.reset();
        s.trial_selection.selected.vehicle.reset();
        const auto model = replay_read(platform, *selected, 0x20, 2, 0x4451e9); // BEFORE REP copy
        copy_selection(platform, 3);
        selection[3].model = model;
        const auto config = replay_read(platform, *selected, 0x22, 2, 0x4451f6);
        const auto tyre = replay_read(platform, *selected, 0x26, 2, 0x4451fc);
        const auto livery = replay_read(platform, *selected, 0x24, 2, 0x445202);
        selection[3].config = config;
        selection[3].tyre = tyre;
        selection[3].livery = livery;
    }
    else
        copy_selection(platform, 3);
    Word unused = 4;
    files.load_track_replay_442ef0(platform);          // 44522F, all prior writes already reached
    const auto track = s.trial_selection.track.replay; // EBX captured445234
    if (track)
    {
        select_track_442f60(platform, track);
        s.trial_selection.track.header_value = 0;
        s.trial_selection.track.replay.reset();
        s.trial_selection.track.vehicle.reset();
        const auto tyre = replay_read(platform, *track, 0x26, 2, 0x445265); // BEFORE REP copy
        copy_selection(platform, 4);
        const auto model = replay_read(platform, *track, 0x20, 2, 0x44526f);
        const auto config = replay_read(platform, *track, 0x22, 2, 0x445273);
        selection[4].model = model;
        selection[4].config = config;
        selection[4].tyre = tyre;
        selection[4].livery = 1;
        unused = 5;
    }
    // 4452CB whole represented source scalar records, no constructor defaults.
    auto *bytes = reinterpret_cast<unsigned char *>(selection.data());
    const Word zero = 0;
    for (Word offset = unused * 0x150u; offset < 6u * 0x150u; offset += 4)
        std::memcpy(bytes + offset, &zero, 4);
    // Original remainder count is zero because both total and start are DWORD aligned.
}
void prepare_source_time_trial_445360(RaceSessionPlatform &platform, SourceTrackReplay442ef0 &files)
{
    reset_source_time_trial_ghosts_443060(platform);             // 445360
    prepare_source_time_trial_selection_445150(platform, files); // 445365
    auto &s = platform.state();
    s.trial_selection.split_selection = 0; // 44536C
    s.split_setting = 0;
    s.trial_selection.result_b.header_value = 0;
    s.trial_selection.result_b.vehicle.reset();
    s.trial_selection.result_b.replay.reset();
    s.trial_selection.result_a.header_value = 0;
    s.trial_selection.result_a.vehicle.reset();
    s.trial_selection.result_a.replay.reset();
    s.ghosts.reference_completed = s.trial_selection.previous_completed; // 445394..99
}
} // namespace sgn::retail
