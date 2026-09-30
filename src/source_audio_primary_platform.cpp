#include "sgn/source_audio_primary.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
SourceAudioPrimaryOutcome create_platform_source_audio_primary_461d25(const SourceAudioPrimaryBufferBoundary &reached,
                                                                      RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    auto &world = platform.state();
    if (&reached.output != &world.audio_initialization.primary || reached.device != world.audio_initialization.device)
        throw SourceAudioDefaultDeviceError(0x461d25u,
                                            "461D25 requires this world's reached device and primary output slot");
    return create_source_audio_primary_461d25(reached, platform.source_audio_owner());
}
} // namespace sgn::retail
