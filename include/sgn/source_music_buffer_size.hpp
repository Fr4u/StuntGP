#pragma once
#include "sgn/retail_audio_startup.hpp"
#include "sgn/source_music_wave_entry.hpp"
#include <cstddef>
#include <type_traits>

namespace sgn::retail
{
// Host pointers name existing owners, not offsets into a fabricated x86 object.
// All pointers must remain valid throughout the call. shift is the actual BL
// saved by the live45FDA0 invocation, not a later reread of stream.field_df18.
struct SourceMusicBufferSizeFields
{
    const std::uint32_t *period;
    const std::uint32_t *byte_rate;
    const std::uint32_t *multiplier;
    std::uint32_t *quarter_period;
    std::uint32_t *buffer_bytes;
    std::uint32_t *sector_count;
    std::uint32_t *threshold;
    std::uint32_t shift;
};
static_assert(std::is_standard_layout_v<SourceMusicBufferSizeFields>);
#if defined(_M_X64)
static_assert(offsetof(SourceMusicBufferSizeFields, shift) == 56);
static_assert(offsetof(SourceMusicBufferSizeFields, threshold) == 48);
extern "C" std::uint32_t source_music_buffer_size_x64(SourceMusicBufferSizeFields *) noexcept;
#endif
// Executes45FE6D..45FEE2 and, on JNC,45FEFD..45FF2A. Returns true for
// sufficient rounded capacity. Does not call45FCE0 on false: caller owns that
// source cleanup and subsequent80004005 store. Source x87 control is inherited.
bool source_music_buffer_size_45fe6d(AudioStreamState &, std::uint8_t saved_bl);
struct SourceMusicBufferEntryResult
{
    SourceMusicWaveEntryResult wave;
    std::optional<bool> sufficient_capacity;
    std::optional<std::uint32_t> next_site;
};
// One invocation from timer setup through parsing and sizing; no prefix replay.
// False capacity stops BEFORE45FEE4 cleanup, true BEFORE45FF30 descriptor tail.
SourceMusicBufferEntryResult receive_source_music_buffer_entry_45fda0(std::shared_ptr<RaceSessionPlatform>,
                                                                      SourceMusicTimerCapsReceiving &,
                                                                      SourceMusicDriveTypeReceiving &,
                                                                      SourceMusicDiskFreeSpaceReceiving &,
                                                                      SourceMusicWaveIO &);
} // namespace sgn::retail
