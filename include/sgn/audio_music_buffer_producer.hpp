#pragma once
#include "sgn/audio_music_buffer.hpp"
#include "sgn/retail_audio_catalog_startup.hpp"
namespace sgn::retail
{
// NEW narrow host interface. It borrows the actual persistent 62DF08 cell;
// it neither owns that cell nor promises ownership of any numeric value.
struct AudioMusicBufferProducer
{
    virtual ~AudioMusicBufferProducer() = default;
    virtual AudioResult create_music_buffer(AudioDeviceId, const AudioBufferRequest &, AudioMusicBuffer &) = 0;
};
} // namespace sgn::retail
