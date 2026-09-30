#include "sgn/source_audio_enumeration.hpp"
#include "sgn/source_audio_enumeration_native.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
SourceAudioEnumerationOutcome enumerate_session_source_audio_46210e(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    auto &world = platform.state();
    Win32SourceAudioEnumeration native;
    return enumerate_source_audio_stage_46210e(world.audio_initialization.enumeration, world.audio_maintenance.listener,
                                               native);
}
} // namespace sgn::retail
