#include "sgn/source_audio_listener_native_configuration.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
SourceAudioListenerContinuationOutcome continue_platform_source_audio_listener_461aa0(
    const SourceAudioListenerAcquisitionOutcome &reached, RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    if (const auto *configuration = std::get_if<SourceAudioListenerConfigurationBoundary>(&reached))
        if (&configuration->listener != &platform.state().audio_maintenance.listener)
            throw SourceAudioDefaultDeviceError(0x461aa0u, "461AA0 requires the same world's acquired listener slot");
    return continue_source_audio_listener_461aa0(reached, platform.source_audio_owner());
}
} // namespace sgn::retail
