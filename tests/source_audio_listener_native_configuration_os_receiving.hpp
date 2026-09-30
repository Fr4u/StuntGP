#pragma once
#include "sgn/source_audio_listener_native_configuration.hpp"
#include <iosfwd>

namespace sgn::retail::receiving {
SourceAudioListenerContinuationOutcome receive_listener_configuration_with_live_window(
    const SourceAudioListenerAcquisitionOutcome&,RaceSessionPlatform&,InputWindowHandles,std::ostream&);
}
