#include "sgn/source_audio_listener_acquisition.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
SourceAudioListenerAcquisitionOutcome acquire_platform_source_audio_listener_462174(
    const SourceAudioPrimaryFormatResult &reached, RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    auto &world = platform.state();
    return acquire_source_audio_listener_462174(
        reached, platform.source_audio_owner(), world.audio_initialization.primary, world.audio_maintenance.listener,
        world.audio_driver.spatial_mode, world.audio_initialization.spatial_enabled);
}
} // namespace sgn::retail
