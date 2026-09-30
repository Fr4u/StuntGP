#pragma once
#include "sgn/source_audio_enumeration.hpp"

namespace sgn
{
class WindowsAudioDevice;
namespace retail
{
class SourceAudioDefaultDeviceError : public std::runtime_error
{
  public:
    const std::uint32_t source_site;
    SourceAudioDefaultDeviceError(std::uint32_t site, const char *message)
        : std::runtime_error(message), source_site(site)
    {
    }
};
struct SourceAudioDefaultDeviceFailure
{
    AudioResult result;
};
struct SourceAudioCooperativeLevelBoundary
{
    static constexpr std::uint32_t source_site = 0x462165u;
    [[noreturn]] void resume() const;
};
using SourceAudioDefaultDeviceOutcome =
    std::variant<SourceAudioDefaultDeviceFailure, SourceAudioCooperativeLevelBoundary>;
// Requires the reached A1 boundary, same owner, and actual zero62E0B0 prefix.
// No enumeration/prefix rerun or host backend preparation. Output is retained
// for failure and success. Host admission errors are typed exceptions, not HRs.
SourceAudioDefaultDeviceOutcome create_source_audio_default_device_462146(const SourceAudioDefaultDeviceBoundary &,
                                                                          AudioDeviceId &, WindowsAudioDevice &);
SourceAudioDefaultDeviceOutcome create_session_source_audio_default_device_462146(
    const SourceAudioDefaultDeviceBoundary &, RaceSessionPlatform &);
} // namespace retail
} // namespace sgn
