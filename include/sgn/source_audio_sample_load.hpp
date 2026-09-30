#pragma once
#include "sgn/source_audio_sample_creator.hpp"
namespace sgn::retail
{
// NEW receiving facts, not the source's early metadata-ready word.
struct SourceAudioSampleLoadResult
{
    bool source_return{}, buffer_retained{}, spatial_query_attempted{}, spatial_retained{}, copy_phase_reached{};
    std::uint32_t last_native_call{};
    std::optional<AudioResult> last_native_result;
};
using SourceAudioSampleLoadOutcome = std::variant<SourceAudioListenerAcquisitionFailure, SourceAudioSampleLoadResult>;
SourceAudioSampleLoadOutcome load_source_audio_sample_after_listener(const SourceAudioListenerContinuationOutcome &,
                                                                     WindowsAudioDevice &, const AudioDeviceId &,
                                                                     AudioSample &, AudioSampleLoadFields &,
                                                                     const AssetStore &, std::string_view,
                                                                     const std::uint32_t &spatial_enabled,
                                                                     std::uint32_t flags);
SourceAudioSampleLoadOutcome load_platform_source_audio_sample_after_listener(
    const SourceAudioListenerContinuationOutcome &, RaceSessionPlatform &, const AssetStore &, std::size_t index,
    std::string_view resource, std::uint32_t flags);
} // namespace sgn::retail
