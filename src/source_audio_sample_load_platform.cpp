#include "sgn/source_audio_sample_load.hpp"
#include "sgn/race_session.hpp"
namespace sgn::retail
{
SourceAudioSampleLoadOutcome load_platform_source_audio_sample_after_listener(
    const SourceAudioListenerContinuationOutcome &reached, RaceSessionPlatform &platform, const AssetStore &assets,
    std::size_t index, std::string_view resource, std::uint32_t flags)
{
    platform.require_owner_thread();
    if (const auto *failure = std::get_if<SourceAudioListenerAcquisitionFailure>(&reached))
        return *failure;
    auto &state = platform.state();
    if (index >= state.audio_driver.samples.size())
        throw std::out_of_range("source sample index");
    return load_source_audio_sample_after_listener(reached, platform.source_audio_owner(),
                                                   state.audio_initialization.device, state.audio_driver.samples[index],
                                                   state.audio_initialization.sample_fields.samples[index], assets,
                                                   resource, state.audio_initialization.spatial_enabled, flags);
}
} // namespace sgn::retail
