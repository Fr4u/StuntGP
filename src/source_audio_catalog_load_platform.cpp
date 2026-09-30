#include "sgn/source_audio_catalog_load.hpp"
#include "sgn/race_session.hpp"
namespace sgn::retail
{
SourceAudioCatalogLoadOutcome load_platform_source_audio_catalog_after_listener(
    const SourceAudioListenerContinuationOutcome &reached, RaceSessionPlatform &platform, const AssetStore &assets)
{
    platform.require_owner_thread();
    if (const auto *failure = std::get_if<SourceAudioListenerAcquisitionFailure>(&reached))
        return *failure;
    auto &state = platform.state();
    return load_source_audio_catalog_after_listener(
        reached, platform.source_audio_owner(), state.audio_driver, state.audio_initialization.sample_fields,
        state.audio_initialization.descriptor_types, state.audio_initialization.device, assets,
        state.audio_initialization.spatial_enabled);
}
} // namespace sgn::retail
