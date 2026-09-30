#pragma once
#include "sgn/retail_audio_initialize.hpp"
#include "sgn/source_audio_listener_acquisition.hpp"
#include "sgn/audio_stream_playback.hpp"
#include "sgn/audio_environment.hpp"
#include <memory>

namespace sgn
{
namespace retail
{
struct SourceAudioPrimaryFormatResult;
struct SourceAudioSampleCreationResult;
struct SourceAudioSampleLoadResult;
struct SourceAudioCatalogLoadResult;
} // namespace retail
struct DeferredAudioConstructionOnly
{
};
struct SourceAudioDefaultDeviceNativeApi;
struct PcmWave
{
    std::uint32_t sample_rate{};
    std::uint16_t channels{}, bits_per_sample{}, block_align{};
    Bytes samples;
};
PcmWave decode_pcm_wave(std::span<const std::uint8_t>);
struct AudioError : std::runtime_error
{
    retail::AudioResult result;
    AudioError(retail::AudioResult code, const std::string &operation) : std::runtime_error(operation), result(code) {}
};
struct AudioDeviceInfo
{
    unsigned channels{}, sample_rate{};
    std::uint32_t channel_mask{};
};
struct AudioVoiceInfo
{
    std::uint64_t samples_played{};
    unsigned queued{}, references{};
    bool started{};
    float frequency_ratio{};
    std::vector<float> output_matrix;
    bool streaming{};
    std::uint64_t stream_submitted_bytes{}, stream_buffers_completed{}, stream_publications{};
};

// Windows10+ XAudio2.9/X3DAudio platform boundary. All public calls are made on
// the owning game thread. The audio engine consumes immutable shared PCM data.
// This supplies modern mixing, not bit-identical DirectSound DSP output.
class WindowsAudioDevice final : public retail::AudioDevice,
                                 public retail::AudioInitializationPlatform,
                                 public retail::AudioStreamCursorDevice,
                                 public retail::AudioEnvironmentDevice,
                                 public retail::AudioMusicBufferProducer
{
    struct Impl;
    std::unique_ptr<Impl> impl_;
    template <class Output>
    retail::AudioResult create_buffer_value(retail::AudioDeviceId, const retail::AudioBufferRequest &, Output &);

  public:
    // Construction only: no COM apartment, XAudio2 engine/master, or DSCreate.
    WindowsAudioDevice(DeferredAudioConstructionOnly, float master_gain = 1.0f);
    // Explicit native import seam for receiving. Production uses the default API.
    WindowsAudioDevice(DeferredAudioConstructionOnly, float master_gain, const SourceAudioDefaultDeviceNativeApi &);
    explicit WindowsAudioDevice(float master_gain = 1.0f); // existing preview compatibility
    void prepare_host_backend();
    bool host_backend_ready() const;
    std::uint64_t source_device_identity() const; // borrowed pointer-width token, no AddRef
    retail::AudioResult create_source_default_device(retail::AudioDeviceId &); // actual462146
    // Actual retained source interface; exact level3 only in this bounded stage.
    retail::AudioResult set_source_cooperative_level(retail::AudioDeviceId, std::uintptr_t source_window,
                                                     std::uint32_t level);
    // Actual 461D25 on the retained A2/A3 device. Output remains nonowning.
    retail::AudioResult create_source_primary_buffer(retail::AudioDeviceId, const retail::AudioBufferRequest &,
                                                     retail::AudioPrimaryId &);
    std::uint64_t source_primary_identity() const;
    // Actual remaining461CF0 native operations; same source state references.
    retail::SourceAudioPrimaryFormatResult complete_source_primary_format(retail::AudioDeviceId,
                                                                          retail::AudioPrimaryId &,
                                                                          const std::uint32_t &requested_rate,
                                                                          std::uint32_t &spatial_capability,
                                                                          std::uint32_t &spatial_enabled);
    retail::SourceAudioListenerAcquisitionOutcome acquire_source_listener_after_primary_format(
        retail::AudioPrimaryId &, retail::AudioListenerId &, const std::uint32_t &spatial_mode,
        std::uint32_t &spatial_enabled);
    std::uint64_t source_listener_identity() const; // Borrowed, no AddRef.
    retail::SourceAudioSamplesBoundary configure_source_listener_after_query(const retail::AudioListenerId &);
    // Same retained device after the actual A6 skip or completed A7 configuration.
    retail::SourceAudioSampleCreationResult create_source_sample_buffer(retail::AudioDeviceId, retail::AudioSample &,
                                                                        const retail::AudioSampleLoadFields &,
                                                                        bool spatial, std::uint32_t flags);
    // Single source461E00; borrows actual source owners and immutable asset storage.
    retail::SourceAudioSampleLoadResult load_source_sample(const retail::AudioDeviceId &, retail::AudioSample &,
                                                           retail::AudioSampleLoadFields &, const AssetStore &,
                                                           std::string_view, const std::uint32_t &spatial_enabled,
                                                           std::uint32_t flags);
    // Source461F50, one shared algorithm and this owner's retained native registry.
    retail::SourceAudioCatalogLoadResult load_source_catalog(retail::AudioDriverState &,
                                                             retail::AudioSampleLoadStorage &,
                                                             std::array<std::uint32_t, 72> &descriptor_types,
                                                             const retail::AudioDeviceId &, const AssetStore &,
                                                             const std::uint32_t &spatial_enabled);
    // Host admission only: same retained sample token, no native operation.
    // Owner thread and active-operation guards apply before the registry read.
    bool source_sample_stop_admitted(retail::AudioBufferId) const;
    // Actual460A80/460700 children on this owner's retained source spatial sample.
    // Separate from the preview XAudio2 property-capability policy below.
    retail::AudioResult source_query_audio_property(retail::AudioSpatialId, const retail::AudioPropertyGuid &,
                                                    retail::AudioPropertyId &);
    retail::AudioResult source_audio_property_support(retail::AudioPropertyId, const retail::AudioPropertyGuid &,
                                                      std::uint32_t, std::uint32_t &);
    retail::AudioResult source_set_audio_property(retail::AudioPropertyId, const retail::AudioPropertyGuid &,
                                                  std::uint32_t, std::span<const std::uint8_t>,
                                                  std::span<const std::uint8_t>);
    // The optional site only labels a reached source failure. It changes no
    // native operation or success/failure semantics for existing2D callers.
    retail::AudioResult source_sample_status(retail::AudioBufferId, std::uint32_t &, std::uint32_t site = 0x460f87);
    retail::AudioResult source_sample_duplicate(retail::AudioDeviceId, retail::AudioBufferId, retail::AudioBufferId &,
                                                std::uint32_t site = 0x460fd5);
    // 460EFD: QI on the retained duplicate; one actual QI reference in the SAME
    // source sample ledger.4610A5: mode operation on a retained3D identity.
    retail::AudioResult source_sample_query_spatial(retail::AudioBufferId, retail::AudioSpatialId &);
    retail::AudioResult source_sample_spatial_mode(retail::AudioSpatialId, std::uint32_t mode, std::uint32_t apply);
    retail::AudioResult source_sample_frequency(retail::AudioBufferId, std::uint32_t, std::uint32_t site = 0x4611e8);
    retail::AudioResult source_sample_volume(retail::AudioBufferId, std::int32_t, std::uint32_t site = 0x461214);
    retail::AudioResult source_sample_pan(retail::AudioBufferId, std::int32_t, std::uint32_t site = 0x461241);
    retail::AudioResult source_sample_play(retail::AudioBufferId, std::uint32_t flags, std::uint32_t site = 0x461258);
    // NEW R17: 461300 SetAllParameters and 461730 SetPosition/SetVelocity on a
    // retained source spatial identity of this SAME owner (no preview registry).
    retail::AudioResult source_sample_spatial_parameters(retail::AudioSpatialId, const retail::AudioSpatialParameters &,
                                                         std::uint32_t apply, std::uint32_t site = 0x46142b);
    retail::AudioResult source_sample_spatial_position(retail::AudioSpatialId, Vec3, std::uint32_t apply,
                                                       std::uint32_t site = 0x4617f0);
    retail::AudioResult source_sample_spatial_velocity(retail::AudioSpatialId, Vec3, std::uint32_t apply,
                                                       std::uint32_t site = 0x46180a);
    std::uint32_t source_sample_release(retail::AudioBufferId, std::uint32_t site = 0x460e39);
    std::uint32_t source_sample_release_spatial(retail::AudioSpatialId);
    retail::AudioResult source_sample_listener_position(retail::AudioListenerId, Vec3);
    retail::AudioResult source_sample_listener_velocity(retail::AudioListenerId, Vec3);
    retail::AudioResult source_sample_listener_orientation(retail::AudioListenerId, Vec3 forward, Vec3 up);
    ~WindowsAudioDevice() override;
    WindowsAudioDevice(const WindowsAudioDevice &) = delete;
    WindowsAudioDevice &operator=(const WindowsAudioDevice &) = delete;
    retail::AudioSample create_sample(PcmWave, bool spatial);
    void load_catalog(const AssetStore &, retail::AudioDriverState &, bool spatial = true);
    retail::AudioListenerId listener() const;
    AudioDeviceInfo info() const;
    AudioVoiceInfo inspect(retail::AudioBufferId) const;
    std::size_t live_buffers() const;
    retail::AudioResult health() const;
    retail::AudioResult status(retail::AudioBufferId, std::uint32_t &) override;
    retail::AudioResult duplicate(retail::AudioBufferId, retail::AudioBufferId &) override;
    retail::AudioResult query_spatial(retail::AudioBufferId, retail::AudioSpatialId &) override;
    std::uint32_t release(retail::AudioBufferId) override;
    std::uint32_t release_spatial(retail::AudioSpatialId) override;
    retail::AudioResult frequency(retail::AudioBufferId, std::uint32_t) override;
    retail::AudioResult volume(retail::AudioBufferId, std::int32_t) override;
    retail::AudioResult pan(retail::AudioBufferId, std::int32_t) override;
    retail::AudioResult play(retail::AudioBufferId, std::uint32_t, std::uint32_t, std::uint32_t) override;
    retail::AudioResult stop(retail::AudioBufferId) override;
    retail::AudioResult spatial_parameters(retail::AudioSpatialId, const retail::AudioSpatialParameters &,
                                           std::uint32_t) override;
    retail::AudioResult spatial_mode(retail::AudioSpatialId, std::uint32_t, std::uint32_t) override;
    retail::AudioResult spatial_position(retail::AudioSpatialId, Vec3, std::uint32_t) override;
    retail::AudioResult spatial_velocity(retail::AudioSpatialId, Vec3, std::uint32_t) override;
    retail::AudioResult listener_position(retail::AudioListenerId, Vec3, std::uint32_t) override;
    retail::AudioResult listener_velocity(retail::AudioListenerId, Vec3, std::uint32_t) override;
    retail::AudioResult listener_orientation(retail::AudioListenerId, Vec3, Vec3, std::uint32_t) override;
    retail::AudioResult enumerate(retail::AudioEnumerationReceiver &) override;
    retail::AudioResult create_default_device(retail::AudioDeviceId &) override;
    retail::AudioResult cooperative_level(retail::AudioDeviceId, std::uint64_t, std::uint32_t) override;
    std::uint32_t release_device(retail::AudioDeviceId) override;
    retail::AudioResult create_buffer(retail::AudioDeviceId, const retail::AudioBufferRequest &,
                                      retail::AudioBufferId &) override;
    retail::AudioResult create_music_buffer(retail::AudioDeviceId, const retail::AudioBufferRequest &,
                                            retail::AudioMusicBuffer &) override;
    retail::AudioResult query_spatial_buffer(retail::AudioBufferId, retail::AudioSpatialId &) override;
    retail::AudioResult lock_buffer(retail::AudioBufferId, std::uint32_t, std::uint32_t, retail::AudioBufferMemory &,
                                    std::uint32_t) override;
    retail::AudioResult unlock_buffer(retail::AudioBufferId, std::span<std::uint8_t>, std::span<std::uint8_t>) override;
    retail::AudioResult capabilities(retail::AudioDeviceId, std::span<std::uint32_t, 24>) override;
    retail::AudioResult primary_format(retail::AudioPrimaryId, retail::AudioWaveFormat &, std::uint32_t,
                                       std::uint32_t &) override;
    retail::AudioResult set_primary_format(retail::AudioPrimaryId, const retail::AudioWaveFormat &) override;
    retail::AudioResult query_listener(retail::AudioPrimaryId, retail::AudioListenerId &) override;
    retail::AudioResult listener_doppler(retail::AudioListenerId, float, std::uint32_t) override;
    retail::AudioResult listener_rolloff(retail::AudioListenerId, float, std::uint32_t) override;
    retail::AudioResult listener_distance(retail::AudioListenerId, float, std::uint32_t) override;
    retail::AudioResult set_play_cursor(retail::AudioBufferId, std::uint32_t);
    retail::AudioResult play_cursors(retail::AudioBufferId, std::uint32_t &, std::uint32_t &) override;
    retail::AudioResult query_audio_property(retail::AudioSpatialId, const retail::AudioPropertyGuid &,
                                             retail::AudioPropertyId &) override;
    retail::AudioResult audio_property_support(retail::AudioPropertyId, const retail::AudioPropertyGuid &,
                                               std::uint32_t, std::uint32_t &) override;
    retail::AudioResult set_audio_property(retail::AudioPropertyId, const retail::AudioPropertyGuid &, std::uint32_t,
                                           std::span<const std::uint8_t>, std::span<const std::uint8_t>) override;
};
// New Windows boundary: source file operations use owned MMIO identities;
// callbacks are dispatched on the constructing thread via a message-only
// window. A pending callback cannot outlive cancellation or source owners.
class WindowsAudioStreamPlatform final : public retail::AudioStreamPlaybackPlatform
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    explicit WindowsAudioStreamPlatform(WindowsAudioDevice &);
    WindowsAudioStreamPlatform(WindowsAudioDevice &, const std::filesystem::path &retail_root);
    ~WindowsAudioStreamPlatform() override;
    WindowsAudioStreamPlatform(const WindowsAudioStreamPlatform &) = delete;
    WindowsAudioStreamPlatform &operator=(const WindowsAudioStreamPlatform &) = delete;
    void cancel_pending_callbacks() noexcept;
    void check_health() const;
    std::size_t live_files() const noexcept;
    std::size_t live_timers() const noexcept;
    std::uint32_t kill_timer(std::uint32_t) override;
    std::uint32_t end_timer_period(std::uint32_t) override;
    retail::AudioResult set_play_cursor(retail::AudioBufferId, std::uint32_t) override;
    std::int32_t seek_wave(retail::AudioFileId, std::int32_t, std::int32_t) override;
    std::uint32_t close_wave(retail::AudioFileId, std::uint32_t) override;
    std::uint32_t resource_length(std::string_view) override;
    bool full_path(std::string_view, std::span<std::uint8_t, 260>) override;
    std::uint32_t timer_capabilities(std::uint32_t &, std::uint32_t &) override;
    std::uint32_t begin_timer_period(std::uint32_t) override;
    std::uint32_t drive_type(std::string_view) override;
    bool disk_sector_bytes(std::string_view, std::uint32_t &) override;
    retail::AudioFileId open_wave(std::span<const std::uint8_t, 260>, std::uint32_t, std::uint32_t) override;
    std::uint32_t descend_wave(retail::AudioFileId, retail::AudioWaveChunk &, const retail::AudioWaveChunk *,
                               std::uint32_t) override;
    std::uint32_t ascend_wave(retail::AudioFileId, retail::AudioWaveChunk &, std::uint32_t) override;
    std::int32_t read_wave(retail::AudioFileId, std::span<std::uint8_t>) override;
    std::uint32_t start_timer(std::uint32_t, std::uint32_t, std::function<void()>, std::uint32_t) override;
    void start_open_worker(std::function<void()>) override;
    void invoke_completion(std::uint32_t, std::uint32_t) override;
    std::uint32_t milliseconds() override;
};
} // namespace sgn
