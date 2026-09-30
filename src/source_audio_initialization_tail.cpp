#include "sgn/source_audio_initialization_tail.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
SourceAudioInitializationTailOutcome complete_platform_source_audio_initialization_after_listener(
    const SourceAudioListenerContinuationOutcome &reached, RaceSessionPlatform &platform, const AssetStore &assets)
{
    platform.require_owner_thread(); // First guard, before any source access or side effect.
    if (const auto *failure = std::get_if<SourceAudioListenerAcquisitionFailure>(&reached))
        return *failure;
    // The existing A10 owner enforces its actual retained A6/A7 history. Keep its
    // source resets, native calls, ignored single-load false and exceptions.
    const auto loaded = load_platform_source_audio_catalog_after_listener(reached, platform, assets);
    if (const auto *failure = std::get_if<SourceAudioListenerAcquisitionFailure>(&loaded))
        return *failure;
    const auto &catalog = std::get<SourceAudioCatalogLoadResult>(loaded);
    auto &world = platform.state();
    const bool returned = complete_audio_initialization_tail_462187(
        catalog.source_return, world.audio_driver, world.audio_stream, world.audio_maintenance.music, world.audio_level,
        world.options, world.audio_initialization.spatial_enabled);
    return SourceAudioInitializationTailResult{returned, catalog.last_native_call, catalog.last_native_result};
}
} // namespace sgn::retail
