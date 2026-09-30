#pragma once
#include "sgn/retail_audio_startup.hpp"
#include <bit>
#include <limits>
#include <stdexcept>

namespace sgn::retail
{
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
class SourceAudioListenerConfigurationError final : public std::runtime_error
{
  public:
    const std::uint32_t source_site;
    SourceAudioListenerConfigurationError(std::uint32_t site, const char *message)
        : std::runtime_error(message), source_site(site)
    {
    }
};
// Reached461AA0 after QueryInterface returned exactly zero. The caller retains
// the actual62F11C owner: each source load must observe writes by prior calls.
// Pure leaf only; it neither obtains/publishes an interface nor selects a host
// backend. The supplied device must implement the acquired listener's setters.
// IDirectSound3DListener slots15/12/11 are ROLLOFF/DOPPLER/DISTANCE respectively.
// A normal return is461A80's true result, not full462090 audio readiness.
inline bool configure_source_audio_listener_461aa0(const AudioListenerId &listener, AudioListenerSetupDevice &device)
{
    auto current = listener; // 461AA0
    if (!current.value)
        throw SourceAudioListenerConfigurationError(0x461aadu, "461AAD null listener before SetRolloffFactor");
    (void)device.listener_rolloff(current, std::bit_cast<float>(std::uint32_t{0x3f800000}), 0u); // 461AAF,+3C
    current = listener; // 461AB2: retain changes made during the prior call
    if (!current.value)
        throw SourceAudioListenerConfigurationError(0x461abeu, "461ABE null listener before SetDopplerFactor");
    (void)device.listener_doppler(current, std::bit_cast<float>(std::uint32_t{0x40c00000}), 0u); // 461AC1,+30
    current = listener;                                                                          // 461AC4
    if (!current.value)
        throw SourceAudioListenerConfigurationError(0x461ad0u, "461AD0 null listener before SetDistanceFactor");
    (void)device.listener_distance(current, std::bit_cast<float>(std::uint32_t{0x3c23d70a}), 0u); // 461AD3,+2C
    return true; // 461AD6/461ADB: all three returned HRESULTs are ignored
}
} // namespace sgn::retail
