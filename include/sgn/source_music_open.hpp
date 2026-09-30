#pragma once
#include "sgn/source_music_initialization.hpp"
#include <array>

namespace sgn::retail
{
struct SourceMusicOpenPathIO
{
    virtual ~SourceMusicOpenPathIO() = default;
    // Implements the caller-reachable _fullpath(local260,input,260).
    // Unwritten stack bytes remain unknown. The source ignores its return.
    virtual void fullpath_463f7b(std::span<const char> input, std::array<std::optional<std::uint8_t>, 260> &local) = 0;
};
struct SourceMusicOpenArguments
{
    std::uint32_t flags, period, multiplier, completion, user_data;
};
struct SourceMusicOpenResult
{
    std::optional<std::uint32_t> function_return;
    std::optional<std::uint32_t> next_site;
    std::optional<SourceMusicInitializationResult> initialization;
};
SourceMusicOpenResult source_music_open_45fb50(std::shared_ptr<RaceSessionPlatform>, std::span<const char>,
                                               SourceMusicOpenArguments, SourceMusicOpenPathIO &,
                                               SourceMusicTimerCapsReceiving &, SourceMusicDriveTypeReceiving &,
                                               SourceMusicDiskFreeSpaceReceiving &, SourceMusicWaveIO &,
                                               SourceMusicInitializationIO &);
} // namespace sgn::retail
