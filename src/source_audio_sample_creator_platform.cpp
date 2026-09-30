#include "sgn/source_audio_sample_creator.hpp"
#include "sgn/race_session.hpp"
namespace sgn::retail
{
SourceAudioSampleCreationOutcome create_platform_source_audio_sample_after_listener(
    const SourceAudioListenerContinuationOutcome &reached, RaceSessionPlatform &platform, std::size_t index,
    bool spatial, std::uint32_t flags)
{
    platform.require_owner_thread();
    if (const auto *failure = std::get_if<SourceAudioListenerAcquisitionFailure>(&reached))
        return *failure;
    auto &state = platform.state();
    if (index >= state.audio_driver.samples.size())
        throw std::out_of_range("source sample index");
    return create_source_audio_sample_after_listener(
        reached, platform.source_audio_owner(), state.audio_initialization.device, state.audio_driver.samples[index],
        state.audio_initialization.sample_fields.samples[index], spatial, flags);
}
} // namespace sgn::retail
