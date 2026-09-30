#pragma once
#include "sgn/source_audio_catalog_load.hpp"

namespace sgn::retail
{
// NEW receiving facts after the source462090 return, never host playback
// readiness and never a token authorizing a different owner or invocation.
struct SourceAudioInitializationTailResult
{
    bool source_return{};
    std::uint32_t last_native_call{};
    std::optional<AudioResult> last_native_result;
};
using SourceAudioInitializationTailOutcome =
    std::variant<SourceAudioListenerAcquisitionFailure, SourceAudioInitializationTailResult>;
// Borrows the one platform world. Executes existing A10 once in this call and
// then its tail; never accepts a detached catalog result as provenance. A caller
// must replace its A10 call with this composition, not call A10 twice.
SourceAudioInitializationTailOutcome complete_platform_source_audio_initialization_after_listener(
    const SourceAudioListenerContinuationOutcome &, RaceSessionPlatform &, const AssetStore &);
} // namespace sgn::retail
