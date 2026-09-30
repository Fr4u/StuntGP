#include "sgn/source_music_open.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
SourceMusicOpenResult source_music_open_45fb50(std::shared_ptr<RaceSessionPlatform> owner, std::span<const char> input,
                                               SourceMusicOpenArguments a, SourceMusicOpenPathIO &path,
                                               SourceMusicTimerCapsReceiving &caps,
                                               SourceMusicDriveTypeReceiving &drive,
                                               SourceMusicDiskFreeSpaceReceiving &disk, SourceMusicWaveIO &wave,
                                               SourceMusicInitializationIO &io)
{
    if (!owner)
        throw std::invalid_argument("music open requires platform owner");
    owner->require_owner_thread();
    auto &state = owner->state();
    auto &s = state.audio_stream;
    if (s.allocated != 0)
        source_music_close_45fce0(state, io, wave);
    std::array<std::optional<std::uint8_t>, 260> local{}; // unknown, not source zeros
    path.fullpath_463f7b(input, local);                   // 45FB77: return deliberately not consumed
    // 463390 aligned260-byte strncpy path: each complete DWORD is stored
    // after null detection, then remaining DWORDs are padded with zeros.
    // Unknown bytes beyond the first null cannot affect these output stores.
    bool padding = false;
    for (std::size_t i = 0; i < 260; i += 4)
    {
        std::array<std::uint8_t, 4> word{};
        for (std::size_t j = 0; j < 4; ++j)
        {
            if (padding)
                continue;
            if (!local[i + j])
                throw SourceMusicWaveUnknown("463390 source filename stack byte unknown");
            word[j] = *local[i + j];
            padding = word[j] == 0;
        }
        auto destination = s.filename();
        for (std::size_t j = 0; j < 4; ++j)
            destination[i + j] = word[j];
        s.filename(destination); // preceding DWORD stores survive an unknown later word
    }
    switch (a.flags & 0x78u)
    { // 45FC8C/45FCA0 pinned jump tables
    case 8:
        s.field_df1c = 2000;
        s.field_df14 = 24;
        break;
    case 16:
        s.field_df1c = 600;
        s.field_df14 = 5;
        break;
    case 32:
        s.field_df1c = 400;
        s.field_df14 = 4;
        break;
    case 64:
        s.field_df1c = 100;
        s.field_df14 = 3;
        break;
    default:
        if (a.period == 0 || a.multiplier == 0)
            return {0x80070057u, std::nullopt, std::nullopt};
        s.field_df1c = a.period;
        s.field_df14 = a.multiplier;
    }
    s.flags = a.flags;
    s.field_df3c = a.completion;
    s.field_df40 = a.user_data;
    if ((a.flags & 2u) != 0)
        return {std::nullopt, 0x0045fc42u, std::nullopt}; // native CreateThread remains unbound
    auto initialized = source_music_initialize_45fda0(owner, caps, drive, disk, wave, io);
    return {initialized.function_return, initialized.next_site, std::move(initialized)};
}
} // namespace sgn::retail
