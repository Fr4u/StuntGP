#include "sgn/source_audio_sample_creator.hpp"
#include "sgn/platform_audio.hpp"
namespace sgn::retail
{
SourceAudioSampleCreationOutcome create_source_audio_sample_after_listener(
    const SourceAudioListenerContinuationOutcome &reached, WindowsAudioDevice &owner, AudioDeviceId device,
    AudioSample &sample, const AudioSampleLoadFields &fields, bool spatial, std::uint32_t flags)
{
    if (const auto *failure = std::get_if<SourceAudioListenerAcquisitionFailure>(&reached))
        return *failure;
    return owner.create_source_sample_buffer(device, sample, fields, spatial, flags);
}
} // namespace sgn::retail
