#pragma once
#include "sgn/source_audio_primary.hpp"

namespace sgn::retail
{
struct SourceAudioPrimaryFormatResult
{
    std::uint32_t last_call; // 461D4C, 461D7E or 461DEA.
    AudioResult result;      // Actual tested HRESULT from the last native call.
    bool source_return() const noexcept
    {
        return last_call == 0x461deau && result == 0;
    }
};
// Native scratch remains local to the complete remaining source function.
// The rate reference is read after GetFormat; it is not an entry snapshot.
SourceAudioPrimaryFormatResult complete_source_audio_primary_format_461d4c(const SourceAudioDeviceCapsBoundary &,
                                                                           WindowsAudioDevice &,
                                                                           const std::uint32_t &requested_rate,
                                                                           std::uint32_t &spatial_capability,
                                                                           std::uint32_t &spatial_enabled);
SourceAudioPrimaryFormatResult complete_platform_source_audio_primary_format_461d4c(
    const SourceAudioDeviceCapsBoundary &, RaceSessionPlatform &);
} // namespace sgn::retail
