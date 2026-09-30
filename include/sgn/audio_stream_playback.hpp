#pragma once
#include "sgn/audio_music_track.hpp"
#include "sgn/audio_music_buffer_producer.hpp"
#include <functional>
#include <string_view>

namespace sgn::retail
{
struct AudioWaveChunk;

// Additional OS boundary required by the original streaming chain. Source
// state remains on its owning thread. A host implementation must serialize
// callbacks there and cancel them before their captured owners are destroyed.
struct AudioStreamPlaybackPlatform : AudioStreamPlatform
{
    virtual std::uint32_t resource_length(std::string_view relative_name) = 0;
    virtual bool full_path(std::string_view relative_name, std::span<std::uint8_t, 260> output) = 0;
    virtual std::uint32_t timer_capabilities(std::uint32_t &minimum, std::uint32_t &maximum) = 0;
    virtual std::uint32_t begin_timer_period(std::uint32_t) = 0;
    virtual std::uint32_t drive_type(std::string_view root) = 0;
    virtual bool disk_sector_bytes(std::string_view root, std::uint32_t &bytes) = 0;
    virtual AudioFileId open_wave(std::span<const std::uint8_t, 260> filename, std::uint32_t buffer_bytes,
                                  std::uint32_t flags) = 0;
    virtual std::uint32_t descend_wave(AudioFileId, AudioWaveChunk &, const AudioWaveChunk *parent,
                                       std::uint32_t flags) = 0;
    virtual std::uint32_t ascend_wave(AudioFileId, AudioWaveChunk &, std::uint32_t flags) = 0;
    virtual std::int32_t read_wave(AudioFileId, std::span<std::uint8_t>) = 0;
    virtual std::uint32_t start_timer(std::uint32_t delay, std::uint32_t resolution, std::function<void()> callback,
                                      std::uint32_t flags) = 0;
    virtual void start_open_worker(std::function<void()> callback) = 0;
    virtual void invoke_completion(std::uint32_t source_identity, std::uint32_t source_userdata) = 0;
    virtual std::uint32_t milliseconds() = 0;
};
struct AudioStreamCursorDevice
{
    virtual ~AudioStreamCursorDevice() = default;
    virtual AudioResult play_cursors(AudioBufferId, std::uint32_t &play, std::uint32_t &write) = 0;
};
struct AudioStreamPlaybackWorld
{
    AudioDriver audio; // reference-bearing view by value; safe to retain in timer callbacks
    AudioMusicState &music;
    AudioStreamState &stream;
    AudioDeviceId &device;
    AudioCreationDevice &creation;
    AudioStreamCursorDevice &cursors;
    AudioStreamPlaybackPlatform &platform;
    AudioMusicBufferProducer &music_producer; // same owner, narrow output reference
};

// Candidate source ports, pending whole-chain instruction qualification.
// The 18 imported files have fmt size16; their uninitialized source heap WORD
// copied into cbSize is deliberately 0 at the safe host PCM boundary.
AudioResult parse_audio_stream_wave(AudioStreamPlaybackWorld);          // 45FFD0
AudioResult initialize_audio_stream_playback(AudioStreamPlaybackWorld); // 45FDA0
AudioResult open_audio_stream(AudioStreamPlaybackWorld, std::string_view relative_name, std::uint32_t flags,
                              std::uint32_t period, std::uint32_t buffers, std::uint32_t completion,
                              std::uint32_t userdata);                             // 45FB50
AudioResult start_audio_stream(AudioStreamPlaybackWorld);                          // 460240
std::int32_t read_audio_stream(AudioStreamPlaybackWorld, std::span<std::uint8_t>); // 4604E0
void advance_audio_stream(AudioStreamPlaybackWorld);                               // 460510
void start_audio_music(AudioStreamPlaybackWorld, const AudioMusicTrack &, std::uint32_t requested,
                       std::uint32_t loop);                                                                    // 45F8F0
void advance_audio_music_delay(AudioStreamPlaybackWorld, const AudioMusicTrack &, std::uint32_t &delay_ticks); // 441F70
// Exact dependency projection of45F9D0/460330. All fields are borrowed;
// stopping never requires a creation device, cursor reader or opener service.
struct AudioMusicStopWorld
{
    AudioStreamState &stream;
    AudioMusicState &music;
    const LevelAudioState &level;
    AudioDevice &device;
    AudioStreamPlatform &platform;
};
void stop_audio_music(AudioMusicStopWorld);      // sole45F9D0 body
void stop_audio_music(AudioStreamPlaybackWorld); // existing playback API, forwards same references
} // namespace sgn::retail
