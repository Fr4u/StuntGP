#pragma once
#include "sgn/retail_audio_initialize.hpp"
#include <windows.h>
#include <mmreg.h>
#include <dsound.h>

namespace sgn::retail
{
// Exact SDK A entry, independent of the project's UNICODE setting. An explicit
// entry parameter permits authored ABI/failure receiving without substituting
// device lists in production. The default is the actual import, not MMDevice.
using DirectSoundEnumerateAEntry = decltype(&::DirectSoundEnumerateA);
class Win32SourceAudioEnumeration final : public AudioEnumerationPlatform
{
    DirectSoundEnumerateAEntry entry_;

  public:
    explicit Win32SourceAudioEnumeration(DirectSoundEnumerateAEntry entry = &::DirectSoundEnumerateA);
    AudioResult enumerate(AudioEnumerationReceiver &) override;
};
} // namespace sgn::retail
