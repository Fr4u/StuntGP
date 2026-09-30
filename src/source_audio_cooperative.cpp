#include "sgn/source_audio_cooperative.hpp"
#include "sgn/platform_audio.hpp"

namespace sgn::retail
{
[[noreturn]] void SourceAudioPrimaryBufferBoundary::resume() const
{
    throw SourceAudioDefaultDeviceError(source_site, "461D25 actual primary CreateSoundBuffer is not qualified");
}
SourceAudioPrimaryBufferBoundary set_source_audio_cooperative_462165(const SourceAudioCooperativeLevelBoundary &,
                                                                     AudioDeviceId device, AudioPrimaryId &primary,
                                                                     std::uintptr_t source_window,
                                                                     WindowsAudioDevice &owner)
{
    const auto observed = owner.set_source_cooperative_level(device, source_window, 3);
    // 462168 calls461CF0 without testing HRESULT. Its local prefix writes all
    // five source descriptor fields before461D25; no primary/caps owner write.
    return {observed, device, primary, AudioBufferRequest{20, 0x11, 0, 0, {}}};
}
} // namespace sgn::retail
