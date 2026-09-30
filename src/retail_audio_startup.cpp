#include "sgn/retail_audio_startup.hpp"
#include "sgn/source_audio_listener_configuration.hpp"
#include <atomic>
#include <bit>

namespace sgn::retail
{
bool initialize_audio_listener(AudioPrimaryId primary, AudioListenerId &listener, AudioListenerSetupDevice &device)
{
    if (!primary.value)
        return false;
    if (device.query_listener(primary, listener) != 0)
        return false;
    return configure_source_audio_listener_461aa0(listener, device);
}
void reset_audio_stream(AudioStreamState &s, AudioMusicState &music) noexcept
{
    s.field_df0c = s.field_df14 = s.field_df1c = s.field_df20 = s.field_df24 = 0;
    s.field_df18 &= 0xffffff00;
    s.field_df28 = s.field_df2c = s.field_df30 = s.field_df34 = s.flags = s.field_df3c = s.field_df40 = 0;
    s.timer = 0;
    s.field_df54 &= 0xffffff00;
    s.file = {};
    s.field_e084 = s.allocated = s.result = s.field_e090 = s.field_e098 = s.field_e09c = s.field_e0a0 = 0;
    music.buffer = {};
}
void initialize_audio_music(AudioStreamState &s, AudioMusicState &music, LevelAudioState &level) noexcept
{
    if (s.playback_active == 0 && level.output_a != 0)
    {
        reset_audio_stream(s, music);
        level.output_a = 0x66;
        s.field_e10c = 0;
        music.requested_volume = 0;
        music.pan = 0;
        s.field_e100 = 0;
        music.gain = 0;
        music.gain_step = 0;
        s.playback_active = 0;
    }
}
bool audio_stream_ready(const AudioStreamState &s) noexcept
{
    return s.allocated != 0 && (s.flags & 0x4000) != 0;
}
AudioResult stop_audio_stream(AudioStreamState &s, AudioMusicState &music, AudioDevice &device,
                              AudioStreamPlatform &platform)
{
    if (!audio_stream_ready(s))
    {
        s.result = 0x887800aa;
        return std::bit_cast<AudioResult>(s.result);
    }
    std::atomic_ref<std::uint32_t>(s.flags).exchange(s.flags | 0x8000, std::memory_order_seq_cst);
    if (s.timer)
    {
        platform.kill_timer(s.timer);
        s.timer = 0;
    }
    device.stop(music.buffer.borrowed());
    platform.set_play_cursor(music.buffer.borrowed(), 0);
    s.field_df28 = s.field_df2c = s.field_df30 = s.field_df34 = s.field_e084 = 0;
    if (s.file.value)
        platform.seek_wave(s.file, std::bit_cast<std::int32_t>(s.data_offset + 4u), 0);
    s.playback_active = 0;
    s.result = 0;
    return 0;
}
void close_audio_stream(AudioStreamState &s, AudioMusicState &music, AudioDevice &device, AudioStreamPlatform &platform)
{
    if (s.allocated)
    {
        stop_audio_stream(s, music, device, platform);
        device.release(music.buffer.borrowed());
        platform.end_timer_period(s.timer_period);
        if (s.file.value)
        {
            platform.close_wave(s.file, 0);
            s.file = {};
        }
        s.allocated = 0;
        s.flags &= 0xffffbfff;
    }
}
void shutdown_audio_music(AudioStreamState &s, AudioMusicState &music, const LevelAudioState &level,
                          AudioDevice &device, AudioStreamPlatform &platform)
{
    if (level.output_a)
        close_audio_stream(s, music, device, platform);
}
} // namespace sgn::retail
