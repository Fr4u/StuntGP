#include "sgn/source_audio_default_device.hpp"
#include "sgn/platform_audio.hpp"

namespace sgn::retail
{
[[noreturn]] void SourceAudioCooperativeLevelBoundary::resume() const
{
    throw SourceAudioDefaultDeviceError(source_site, "462165 SetCooperativeLevel cold continuation is not qualified");
}
SourceAudioDefaultDeviceOutcome create_source_audio_default_device_462146(const SourceAudioDefaultDeviceBoundary &,
                                                                          AudioDeviceId &output,
                                                                          WindowsAudioDevice &owner)
{
    const auto result = owner.create_source_default_device(output);
    if (result != 0)
        return SourceAudioDefaultDeviceFailure{result}; // 46214B/14D
    // A valid successful DSCreate returns an interface. A broken receiving/API
    // output would reach the source vtable load at462162; never fake a device.
    if (!output.value)
        throw SourceAudioDefaultDeviceError(0x462162u, "462162 successful creation returned a null interface");
    return SourceAudioCooperativeLevelBoundary{};
}
} // namespace sgn::retail
