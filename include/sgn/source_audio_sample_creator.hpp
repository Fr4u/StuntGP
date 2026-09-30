#pragma once
#include "sgn/source_audio_listener_native_configuration.hpp"
#include "sgn/retail_audio_catalog_startup.hpp"

namespace sgn::retail
{
// NEW diagnostic: source true is not proof of usable interfaces or loaded PCM.
struct SourceAudioSampleCreationResult
{
    bool source_return{};
    bool native_interfaces_present{};
    std::uint32_t source_function{};
};
using SourceAudioSampleCreationOutcome =
    std::variant<SourceAudioListenerAcquisitionFailure, SourceAudioSampleCreationResult>;
SourceAudioSampleCreationOutcome create_source_audio_sample_after_listener(
    const SourceAudioListenerContinuationOutcome &, WindowsAudioDevice &, AudioDeviceId, AudioSample &,
    const AudioSampleLoadFields &, bool spatial, std::uint32_t flags);
// One existing world record; never a catalog loop or sample-load continuation.
SourceAudioSampleCreationOutcome create_platform_source_audio_sample_after_listener(
    const SourceAudioListenerContinuationOutcome &, RaceSessionPlatform &, std::size_t sample_index, bool spatial,
    std::uint32_t flags);
} // namespace sgn::retail
