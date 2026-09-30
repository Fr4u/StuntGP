#include "sgn/source_cold_audio_input.hpp"
#include "sgn/source_audio_default_device.hpp"
#include "sgn/source_audio_primary.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
namespace
{
SourceColdAudioResult listener_false(const SourceAudioListenerAcquisitionFailure &failure)
{
    return {false, failure.source_site, failure.query_result};
}
} // namespace
SourceColdAudioResult initialize_platform_source_audio_462090(RaceSessionPlatform &platform, InputWindowHandles handles)
{
    platform.require_owner_thread(); // First guard, before prefix writes/native calls.
    auto &w = platform.state();
    if (!initialize_audio_prefix_462090(w.audio_initialization, w.audio_driver, w.audio_maintenance, w.audio_level))
        return {false, 0x4620fdu, std::nullopt};
    const auto enumerated = enumerate_session_source_audio_46210e(platform);
    if (const auto *failure = std::get_if<SourceAudioEnumerationFailure>(&enumerated))
        return {false, 0x46210eu, failure->result};
    const auto created = create_session_source_audio_default_device_462146(
        std::get<SourceAudioDefaultDeviceBoundary>(enumerated), platform);
    if (const auto *failure = std::get_if<SourceAudioDefaultDeviceFailure>(&created))
        return {false, 0x462146u, failure->result};
    const auto cooperative = set_platform_source_audio_cooperative_462165(
        std::get<SourceAudioCooperativeLevelBoundary>(created), platform, handles);
    // 462168 proceeds into461CF0 after EVERY returned cooperative HRESULT.
    const auto primary = create_platform_source_audio_primary_461d25(cooperative, platform);
    if (const auto *failure = std::get_if<SourceAudioPrimaryFailure>(&primary))
        return {false, 0x461d25u, failure->result};
    const auto formatted = complete_platform_source_audio_primary_format_461d4c(
        std::get<SourceAudioDeviceCapsBoundary>(primary), platform);
    if (!formatted.source_return())
        return {false, formatted.last_call, formatted.result};
    const auto acquired = acquire_platform_source_audio_listener_462174(formatted, platform);
    if (const auto *failure = std::get_if<SourceAudioListenerAcquisitionFailure>(&acquired))
        return listener_false(*failure);
    const auto configured = continue_platform_source_audio_listener_461aa0(acquired, platform);
    if (const auto *failure = std::get_if<SourceAudioListenerAcquisitionFailure>(&configured))
        return listener_false(*failure);
    // Use the platform's retained AssetStore, not an unrelated supplied store.
    // A missing host resource owner remains an exception after the A7 prefix.
    const auto completed =
        complete_platform_source_audio_initialization_after_listener(configured, platform, platform.assets());
    if (const auto *failure = std::get_if<SourceAudioListenerAcquisitionFailure>(&completed))
        return listener_false(*failure);
    const auto &result = std::get<SourceAudioInitializationTailResult>(completed);
    return {result.source_return, result.last_native_call, result.last_native_result};
}
} // namespace sgn::retail
