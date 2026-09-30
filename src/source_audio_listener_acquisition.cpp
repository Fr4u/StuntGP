#include "sgn/source_audio_listener_acquisition.hpp"
#include "sgn/platform_audio.hpp"

namespace sgn::retail
{
void SourceAudioSamplesBoundary::resume() const
{
    throw SourceAudioDefaultDeviceError(source_site, "462182 sample loading remains outside listener acquisition");
}
void SourceAudioListenerConfigurationBoundary::resume() const
{
    throw SourceAudioDefaultDeviceError(source_site, "461AA0 listener configuration is the next independent stage");
}
SourceAudioListenerAcquisitionOutcome acquire_source_audio_listener_462174(
    const SourceAudioPrimaryFormatResult &reached, WindowsAudioDevice &owner, AudioPrimaryId &primary,
    AudioListenerId &listener, const std::uint32_t &mode, std::uint32_t &enabled)
{
    if (!reached.source_return())
        throw SourceAudioDefaultDeviceError(0x46216du, "46216D cannot continue after a false461CF0 return");
    return owner.acquire_source_listener_after_primary_format(primary, listener, mode, enabled);
}
} // namespace sgn::retail
