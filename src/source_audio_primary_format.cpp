#include "sgn/source_audio_primary_format.hpp"
#include "sgn/platform_audio.hpp"

namespace sgn::retail
{
SourceAudioPrimaryFormatResult complete_source_audio_primary_format_461d4c(const SourceAudioDeviceCapsBoundary &reached,
                                                                           WindowsAudioDevice &owner,
                                                                           const std::uint32_t &rate,
                                                                           std::uint32_t &capability,
                                                                           std::uint32_t &enabled)
{
    if (reached.requested_size != 96)
        throw SourceAudioDefaultDeviceError(0x461d4cu, "461D4C requires the source caps size96 prefix");
    return owner.complete_source_primary_format(reached.device, reached.primary, rate, capability, enabled);
}
} // namespace sgn::retail
