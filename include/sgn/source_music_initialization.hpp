#pragma once
#include "sgn/source_music_buffer_size.hpp"
#include "sgn/audio_music_buffer.hpp"

namespace sgn::retail
{
struct RaceSessionState;
struct SourceMusicBufferDescription
{
    std::uint32_t size;
    std::uint32_t flags;
    std::uint32_t bytes;
    std::uint32_t reserved;
    AudioWaveFormat *format;
};
struct SourceMusicClearOutputs
{
    std::optional<std::span<std::uint8_t>> first;
    std::optional<std::uint32_t> first_bytes;
};
struct SourceMusicLockIO
{
    virtual ~SourceMusicLockIO() = default;
    virtual std::uint32_t lock(AudioBufferId, std::uint32_t offset, std::uint32_t bytes, SourceMusicClearOutputs &,
                               std::uint32_t flags) = 0;
    virtual std::uint32_t unlock(AudioBufferId, std::span<std::uint8_t> first, std::uint32_t first_bytes) = 0;
};
struct SourceMusicInitializationIO : SourceMusicLockedReadIO, SourceMusicLockIO
{
    virtual ~SourceMusicInitializationIO() = default;
    // The backend selects the actual62E0B0 device from this same session.
    // output is the real music.buffer owner; writes survive failure.
    virtual std::uint32_t create(RaceSessionState &, SourceMusicBufferDescription &, AudioMusicBuffer &output,
                                 std::uint32_t outer_unknown_zero) = 0;
    virtual std::uint32_t release(AudioBufferId) = 0;
    virtual std::uint32_t end_period(std::uint32_t period) = 0;
    virtual std::uint32_t exchange_flags(std::uint32_t &actual_flags, std::uint32_t value) = 0;
    virtual std::uint32_t kill_timer(std::uint32_t timer) = 0;
    virtual std::uint32_t stop_buffer(AudioBufferId) = 0;
    virtual std::uint32_t set_position(AudioBufferId, std::uint32_t position) = 0;
    virtual std::uint32_t get_status(AudioBufferId, std::uint32_t &actual_status) = 0;
    virtual std::uint16_t volume_word_4897f8() = 0;
    virtual std::uint32_t set_volume(AudioBufferId, std::int32_t volume) = 0;
    virtual std::uint32_t play(AudioBufferId, std::uint32_t reserved, std::uint32_t priority, std::uint32_t flags) = 0;
    // Map source callback460510 and userdata62DF08 to this weak session owner.
    // Native timer callbacks must be marshalled to the owner thread; no raw
    // source address is executable or a truncated modern host pointer.
    virtual std::uint32_t start_timer(std::uint32_t delay, std::uint32_t resolution,
                                      std::weak_ptr<RaceSessionPlatform> owner, std::uint32_t callback_site,
                                      std::uint32_t flags) = 0;
};
void source_music_clear_45fd40(AudioStreamState &, AudioBufferId, SourceMusicLockIO &);
std::uint32_t source_music_stop_460330(RaceSessionState &, SourceMusicInitializationIO &, SourceMusicWaveIO &);
std::uint32_t source_music_start_460240(std::shared_ptr<RaceSessionPlatform>, SourceMusicInitializationIO &);
std::uint32_t source_music_read_4604e0(AudioStreamState &, std::span<std::uint8_t>, std::uint32_t requested_bits,
                                       SourceMusicLockedReadIO &);
void source_music_close_45fce0(RaceSessionState &, SourceMusicInitializationIO &, SourceMusicWaveIO &);
struct SourceMusicInitializationResult
{
    SourceMusicBufferEntryResult prefix;
    std::optional<std::uint32_t> function_return;
    std::optional<std::uint32_t> next_site;
};
// Completes the45FDA0 caller on every path where prefix values are qualified.
SourceMusicInitializationResult source_music_initialize_45fda0(std::shared_ptr<RaceSessionPlatform>,
                                                               SourceMusicTimerCapsReceiving &,
                                                               SourceMusicDriveTypeReceiving &,
                                                               SourceMusicDiskFreeSpaceReceiving &, SourceMusicWaveIO &,
                                                               SourceMusicInitializationIO &);
} // namespace sgn::retail
