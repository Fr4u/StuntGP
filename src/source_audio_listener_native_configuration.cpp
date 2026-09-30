#include "sgn/source_audio_listener_native_configuration.hpp"
#include "sgn/platform_audio.hpp"

namespace sgn::retail
{
SourceAudioListenerContinuationOutcome continue_source_audio_listener_461aa0(
    const SourceAudioListenerAcquisitionOutcome &reached, WindowsAudioDevice &owner)
{
    if (const auto *failure = std::get_if<SourceAudioListenerAcquisitionFailure>(&reached))
        return *failure;
    if (const auto *samples = std::get_if<SourceAudioSamplesBoundary>(&reached))
        return *samples;
    return owner.configure_source_listener_after_query(
        std::get<SourceAudioListenerConfigurationBoundary>(reached).listener);
}
} // namespace sgn::retail
