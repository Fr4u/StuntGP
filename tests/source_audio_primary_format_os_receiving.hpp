#pragma once
#include "sgn/source_audio_primary_format.hpp"

namespace sgn::retail::receiving {
//An observed zero SetFormat result qualifies the source function's result,
//not physical endpoint format equality or full audio startup/playback.
SourceAudioPrimaryFormatResult receive_primary_format_with_live_window(
    const SourceAudioDeviceCapsBoundary&,RaceSessionPlatform&,InputWindowHandles);
}
