#include "sgn/source_audio_primary.hpp"
#include "sgn/platform_audio.hpp"

namespace sgn::retail
{
[[noreturn]] void SourceAudioDeviceCapsBoundary::resume() const
{
    throw SourceAudioDefaultDeviceError(source_site, "461D4C actual device GetCaps is not qualified");
}
SourceAudioPrimaryOutcome create_source_audio_primary_461d25(const SourceAudioPrimaryBufferBoundary &reached,
                                                             WindowsAudioDevice &owner)
{
    const auto hr = owner.create_source_primary_buffer(reached.device, reached.request, reached.output);
    if (hr != 0)
        return SourceAudioPrimaryFailure{hr}; // TEST/JZ, not SUCCEEDED.
    // The source does not dereference the primary pointer at this reached
    // point. A zero HRESULT with a null output still reaches device GetCaps.
    return SourceAudioDeviceCapsBoundary{reached.device, reached.output};
}
} // namespace sgn::retail
