#pragma once
#include "sgn/audio_music_buffer.hpp"
#include "sgn/retail_camera_initialization.hpp"
namespace sgn::retail
{
struct AudioMusicState
{
    AudioMusicBuffer buffer{};                             // 62DF08, the actual music buffer owned by playback
    std::uint32_t requested_volume{}, gain{}, gain_step{}; // 62E0FC/62E104/62E108
    std::int32_t pan{}, cached_volume{}, cached_pan{};     // 62E110/62F5B8/62F5BC
};
struct AudioMaintenanceState
{
    AudioListenerId listener{}; // 62F11C
    AudioMusicState music;
};
class SourceMusicCallBinding;
struct AudioMaintenance
{
    AudioDriver &driver;
    LevelAudioState &level; // same owner as driver.level, updatesoutput_b
    AudioMaintenanceState &state;
    const CameraPresentationState &primary_presentation; // Controller0+8C/+9C
    SourceMusicCallBinding *source_music{};              // invocation-only; platform retains this binding
};
void audio_recycle_duplicates(AudioDriver &); // 460DF0, all32 slots72..103
void audio_recount_playing(AudioDriver &);    // 462200,72+duplicate_count bound
void audio_update_listener(AudioDriver &, AudioListenerId, const contact::player::Controller &,
                           const CameraPresentationState &);                                             // 461AE0
void audio_apply_music_levels(AudioDriver &, AudioMusicState &, std::int32_t pan, std::uint32_t volume); // 45F7D0
void audio_advance_music_gain(AudioDriver &, AudioMusicState &);                                         // 45F880
void audio_advance_sound_gain(LevelAudioState &) noexcept;                                               // 460CE0
void audio_maintain(AudioMaintenance); // 462280, full outer tick
} // namespace sgn::retail
