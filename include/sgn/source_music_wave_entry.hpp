#pragma once
#include "sgn/source_music_disk_space_prefix.hpp"
#include "sgn/source_music_wave_open.hpp"

namespace sgn::retail
{
struct SourceMusicWaveEntryResult
{
    SourceMusicDiskFreeSpacePrefixObservation disk_prefix;
    std::optional<std::uint32_t> helper_result;
    std::optional<std::uint32_t> function_return;
    std::optional<std::uint32_t> next_site;
};
// Same live 45FDA0 invocation through disk and the entire 45FFD0 helper.
// Helper failure returns ZERO from 45FDA0 while preserving the helper HRESULT
// in stream.result. Helper success continues at45FE6D (buffer sizing pending).
SourceMusicWaveEntryResult receive_source_music_wave_entry_45fda0(std::shared_ptr<RaceSessionPlatform>,
                                                                  SourceMusicTimerCapsReceiving &,
                                                                  SourceMusicDriveTypeReceiving &,
                                                                  SourceMusicDiskFreeSpaceReceiving &,
                                                                  SourceMusicWaveIO &);
} // namespace sgn::retail
