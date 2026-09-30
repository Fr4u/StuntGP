#include "sgn/source_audio_sample_load.hpp"
#include "sgn/platform_audio.hpp"
namespace sgn::retail
{
SourceAudioSampleLoadOutcome load_source_audio_sample_after_listener(
    const SourceAudioListenerContinuationOutcome &reached, WindowsAudioDevice &owner, const AudioDeviceId &device,
    AudioSample &sample, AudioSampleLoadFields &fields, const AssetStore &assets, std::string_view resource,
    const std::uint32_t &enabled, std::uint32_t flags)
{
    if (const auto *failure = std::get_if<SourceAudioListenerAcquisitionFailure>(&reached))
        return *failure;
    return owner.load_source_sample(device, sample, fields, assets, resource, enabled, flags);
}
} // namespace sgn::retail
