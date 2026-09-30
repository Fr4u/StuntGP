#include "sgn/source_audio_enumeration.hpp"

namespace sgn::retail
{
[[noreturn]] void SourceAudioDefaultDeviceBoundary::resume() const
{
    throw SourceAudioEnumerationError(
        source_site,
        "462146 DirectSoundCreate(NULL) cold continuation is not qualified by the existing XAudio2 generation bridge");
}
SourceAudioEnumerationOutcome enumerate_source_audio_stage_46210e(AudioEnumerationState &state,
                                                                  AudioListenerId &listener,
                                                                  AudioEnumerationPlatform &platform)
{
    const auto result = enumerate_audio_devices_46210e(state, listener, platform);
    if (result != 0)
        return SourceAudioEnumerationFailure{result};
    return SourceAudioDefaultDeviceBoundary{};
}
} // namespace sgn::retail
