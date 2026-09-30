#include "sgn/source_audio_catalog_load.hpp"
#include "sgn/platform_audio.hpp"
namespace sgn::retail
{
SourceAudioCatalogLoadOutcome load_source_audio_catalog_after_listener(
    const SourceAudioListenerContinuationOutcome &reached, WindowsAudioDevice &owner, AudioDriverState &driver,
    AudioSampleLoadStorage &storage, std::array<std::uint32_t, 72> &types, const AudioDeviceId &device,
    const AssetStore &assets, const std::uint32_t &enabled)
{
    if (const auto *failure = std::get_if<SourceAudioListenerAcquisitionFailure>(&reached))
        return *failure;
    return owner.load_source_catalog(driver, storage, types, device, assets, enabled);
}
} // namespace sgn::retail
