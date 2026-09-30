#include "sgn/source_audio_primary_format.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
SourceAudioPrimaryFormatResult complete_platform_source_audio_primary_format_461d4c(
    const SourceAudioDeviceCapsBoundary &reached, RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    auto &world = platform.state();
    auto &audio = world.audio_initialization;
    if (reached.device != audio.device || &reached.primary != &audio.primary)
        throw SourceAudioDefaultDeviceError(0x461d4cu, "461D4C requires the same world's device and primary slot");
    return complete_source_audio_primary_format_461d4c(reached, platform.source_audio_owner(), audio.requested_rate,
                                                       audio.spatial_capability, audio.spatial_enabled);
}
} // namespace sgn::retail
