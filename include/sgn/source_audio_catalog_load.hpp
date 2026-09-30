#pragma once
#include "sgn/source_audio_sample_load.hpp"
namespace sgn::retail
{
// NEW receiving facts. Source461F50 ignores single-load false and returns true
// after both loops; this result does not assert that any/all samples are usable.
struct SourceAudioCatalogLoadResult
{
    bool source_return{};
    std::uint32_t last_native_call{};
    std::optional<AudioResult> last_native_result;
};
using SourceAudioCatalogLoadOutcome = std::variant<SourceAudioListenerAcquisitionFailure, SourceAudioCatalogLoadResult>;
SourceAudioCatalogLoadOutcome load_source_audio_catalog_after_listener(const SourceAudioListenerContinuationOutcome &,
                                                                       WindowsAudioDevice &, AudioDriverState &,
                                                                       AudioSampleLoadStorage &,
                                                                       std::array<std::uint32_t, 72> &descriptor_types,
                                                                       const AudioDeviceId &, const AssetStore &,
                                                                       const std::uint32_t &spatial_enabled);
SourceAudioCatalogLoadOutcome load_platform_source_audio_catalog_after_listener(
    const SourceAudioListenerContinuationOutcome &, RaceSessionPlatform &, const AssetStore &);
} // namespace sgn::retail
