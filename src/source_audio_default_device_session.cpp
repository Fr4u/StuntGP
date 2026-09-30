#include "sgn/source_audio_default_device.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
SourceAudioDefaultDeviceOutcome create_session_source_audio_default_device_462146(
    const SourceAudioDefaultDeviceBoundary &reached, RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    auto &device = platform.prepare_source_audio_owner();
    return create_source_audio_default_device_462146(reached, platform.state().audio_initialization.device, device);
}
} // namespace sgn::retail
