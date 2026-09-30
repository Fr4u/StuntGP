#pragma once
#include "sgn/source_audio_listener_acquisition.hpp"
#include <iosfwd>

namespace sgn::retail::receiving {
//Main retains the actual A1-A5 Session/platform and its same HWND. No stage is rerun.
SourceAudioListenerAcquisitionOutcome receive_listener_acquisition_with_live_window(
    const SourceAudioPrimaryFormatResult&,RaceSessionPlatform&,InputWindowHandles,std::ostream&);
}
