#pragma once
#include "sgn/source_audio_listener_acquisition.hpp"
#include "sgn/source_audio_listener_configuration.hpp"

namespace sgn::retail
{
using SourceAudioListenerContinuationOutcome =
    std::variant<SourceAudioListenerAcquisitionFailure, SourceAudioSamplesBoundary>;
// Pass failure/skipped-QI through. Only the actual configuration boundary calls
// the existing leaf on the same retained native listener owner, exactly once.
SourceAudioListenerContinuationOutcome continue_source_audio_listener_461aa0(
    const SourceAudioListenerAcquisitionOutcome &, WindowsAudioDevice &);
SourceAudioListenerContinuationOutcome continue_platform_source_audio_listener_461aa0(
    const SourceAudioListenerAcquisitionOutcome &, RaceSessionPlatform &);
} // namespace sgn::retail
