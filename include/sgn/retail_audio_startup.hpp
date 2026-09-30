#pragma once
#include "sgn/retail_audio_maintenance.hpp"
#include "sgn/audio_wave_format.hpp"
#include "sgn/audio_timer_capabilities.hpp"
#include <cstring>

namespace sgn::retail
{
struct AudioPrimaryId
{
    std::uint64_t value{};
    bool operator==(const AudioPrimaryId &) const = default;
};
struct AudioFileId
{
    std::uint64_t value{};
    bool operator==(const AudioFileId &) const = default;
};
// Initialization-only platform calls. The existing17-operation tick interface
// remains unchanged. Setters return the real host result; the source ignores it.
struct AudioListenerSetupDevice
{
    virtual ~AudioListenerSetupDevice() = default;
    virtual AudioResult query_listener(AudioPrimaryId, AudioListenerId &) = 0;
    virtual AudioResult listener_doppler(AudioListenerId, float, std::uint32_t apply) = 0;
    virtual AudioResult listener_rolloff(AudioListenerId, float, std::uint32_t apply) = 0;
    virtual AudioResult listener_distance(AudioListenerId, float, std::uint32_t apply) = 0;
};
bool initialize_audio_listener(AudioPrimaryId, AudioListenerId &, AudioListenerSetupDevice &); // 461A80

// Additional world streaming state only. Music buffer, gain and pan retain their
// existing owner in AudioMusicState. Unknown source roles retain address names.
struct AudioStreamState
{
    std::uint32_t field_df0c{}, field_df14{}, field_df18{}, field_df1c{}, field_df20{}, field_df24{};
    std::uint32_t field_df28{}, field_df2c{}, field_df30{}, field_df34{}, flags{}, field_df3c{}, field_df40{};
    std::uint32_t timer_period{}, timer{}, field_df54{};
    AudioFileId file;
    std::uint32_t data_offset{}, field_e084{}, allocated{}, result{}, field_e090{}, field_e098{}, field_e09c{},
        field_e0a0{};
    std::uint32_t playback_active{}, field_e100{}, field_e10c{};
    // Extend the existing source owners without duplicating the filename's
    // first DWORD or MMCKINFO's data offset, which old codecs already expose.
    std::array<std::uint8_t, 256> filename_tail{};
    AudioTimerCapabilities timer_capabilities;
    AudioWaveFormat format;
    std::uint32_t chunk_id{}, chunk_size{}, chunk_type{}, chunk_flags{}, stop_threshold{}, status{};
    std::array<std::uint8_t, 260> filename() const noexcept
    {
        std::array<std::uint8_t, 260> bytes{};
        std::memcpy(bytes.data(), &field_df54, 4);
        std::memcpy(bytes.data() + 4, filename_tail.data(), 256);
        return bytes;
    }
    void filename(std::span<const std::uint8_t, 260> value) noexcept
    {
        std::memcpy(&field_df54, value.data(), 4);
        std::memcpy(filename_tail.data(), value.data() + 4, 256);
    }
    AudioWaveChunk chunk() const noexcept
    {
        return {chunk_id, chunk_size, chunk_type, data_offset, chunk_flags};
    }
    void chunk(const AudioWaveChunk &value) noexcept
    {
        chunk_id = value.id;
        chunk_size = value.size;
        chunk_type = value.type;
        data_offset = value.data_offset;
        chunk_flags = value.flags;
    }
};
static_assert(std::is_standard_layout_v<AudioStreamState>);
static_assert(std::is_trivially_copyable_v<AudioStreamState>);
struct AudioStreamPlatform
{
    virtual ~AudioStreamPlatform() = default;
    virtual std::uint32_t kill_timer(std::uint32_t) = 0;
    virtual std::uint32_t end_timer_period(std::uint32_t) = 0;
    virtual AudioResult set_play_cursor(AudioBufferId, std::uint32_t) = 0;
    virtual std::int32_t seek_wave(AudioFileId, std::int32_t offset, std::int32_t origin) = 0;
    virtual std::uint32_t close_wave(AudioFileId, std::uint32_t flags) = 0;
};
void reset_audio_stream(AudioStreamState &, AudioMusicState &) noexcept; // 45FAC0, exact selected fields
void initialize_audio_music(AudioStreamState &, AudioMusicState &, LevelAudioState &) noexcept;             // 45FA60
bool audio_stream_ready(const AudioStreamState &) noexcept;                                                 // 460200
AudioResult stop_audio_stream(AudioStreamState &, AudioMusicState &, AudioDevice &, AudioStreamPlatform &); // 460330
void close_audio_stream(AudioStreamState &, AudioMusicState &, AudioDevice &, AudioStreamPlatform &);       // 45FCE0
void shutdown_audio_music(AudioStreamState &, AudioMusicState &, const LevelAudioState &, AudioDevice &,
                          AudioStreamPlatform &); // 45FAB0
} // namespace sgn::retail
