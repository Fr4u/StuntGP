#pragma once
#include "sgn/retail_audio_catalog_startup.hpp"
#include <stdexcept>

namespace sgn::retail
{
class SourceAudioEnumerationError : public std::runtime_error
{
  public:
    const std::uint32_t source_site;
    SourceAudioEnumerationError(std::uint32_t site, const char *message)
        : std::runtime_error(message), source_site(site)
    {
    }
};
// 462030 writes into the context itself, not through the cursor stored in it.
// The cursor is an opaque32-bit compatibility word, never a dereferenceable
// host pointer. Its subtraction origin is preserved for the source count math.
struct AudioEnumerationState
{
    static constexpr std::uint32_t source_origin = 0x62f128;
    std::uint32_t cursor_word{}, count{}, adjacent_word{}; // 62F118/120/124; listener11C borrowed
    std::array<std::uint8_t, 1152> records{};
};
bool record_audio_enumeration(AudioEnumerationState &, AudioListenerId &,
                              const std::optional<std::array<std::uint32_t, 4>> &guid,
                              std::string_view description); // 462030
// The ANSI entry point publishes GUID words before scanning the description.
bool record_audio_enumeration_ansi(AudioEnumerationState &, AudioListenerId &,
                                   const std::optional<std::array<std::uint32_t, 4>> &guid,
                                   const char *description); // 462030
struct AudioEnumerationReceiver
{
    virtual ~AudioEnumerationReceiver() = default;
    virtual bool visit(const std::optional<std::array<std::uint32_t, 4>> &, std::string_view) = 0;
    // Default keeps existing receivers compatible. The source receiver overrides
    // this to preserve GUID writes even if description scanning fails.
    virtual bool visit_ansi(const std::optional<std::array<std::uint32_t, 4>> &, const char *);
};
struct AudioEnumerationPlatform
{
    virtual ~AudioEnumerationPlatform() = default;
    virtual AudioResult enumerate(AudioEnumerationReceiver &) = 0;
};
// Returns the exact platform result; publishes count only for result==0. No
// prefix reset and no device creation. The same helper serves explicit preview
// initialization and the separately bounded source cold stage.
AudioResult enumerate_audio_devices_46210e(AudioEnumerationState &, AudioListenerId &, AudioEnumerationPlatform &);
struct AudioInitializationPlatform : AudioCreationDevice, AudioListenerSetupDevice, AudioEnumerationPlatform
{
    virtual AudioResult create_default_device(AudioDeviceId &) = 0;
    virtual AudioResult cooperative_level(AudioDeviceId, std::uint64_t window, std::uint32_t level) = 0;
    virtual std::uint32_t release_device(AudioDeviceId) = 0;
};
struct AudioInitializationState
{
    AudioDeviceId device{};
    AudioPrimaryId primary{};
    AudioEnumerationState enumeration;
    std::array<std::uint32_t, 72> descriptor_types{};
    AudioSampleLoadStorage sample_fields;
    std::uint32_t spatial_capability{}, spatial_enabled{}, saved_spatial_mode{},
        requested_rate{}; // 4897AC/B0,4899D8,489794
};
struct AudioStartupWorld
{
    AudioDriver &audio;
    AudioMaintenanceState &maintenance;
    AudioStreamState &stream;
    AudioInitializationState &state;
    LevelAudioState &level; // must be the same owner as audio.level
    LevelOverrideOptions &options;
    const AssetStore &assets;
    AudioInitializationPlatform &platform;
    AudioStreamPlatform &stream_platform;
    std::uint64_t window{};
};
// Shared462090 prefix through4620FD. Preserves resets even when input_b==0.
// This is the existing descriptor-type projection, not the full raw216-DWORD copy.
bool initialize_audio_prefix_462090(AudioInitializationState &, AudioDriverState &, AudioMaintenanceState &,
                                    LevelAudioState &);
// Shared462090 continuation after the actual461F50 return. A false value
// performs no writes; normal true preserves source initialized/music/level/mode
// order. This pure value helper is not native admission or an owner token.
bool complete_audio_initialization_tail_462187(bool catalog_return, AudioDriverState &, AudioStreamState &,
                                               AudioMusicState &, LevelAudioState &, LevelOverrideOptions &,
                                               const std::uint32_t &spatial_enabled) noexcept;
bool initialize_audio(AudioStartupWorld &);            // 462090, full reachable callees
void shutdown_audio(AudioStartupWorld &);              // 4621D0
void reconfigure_audio_for_split(AudioStartupWorld &); // 460B80
} // namespace sgn::retail
