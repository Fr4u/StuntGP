#include "sgn/race_session_music.hpp"
#include "sgn/source_music_calls.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
namespace
{
struct RequiredSourceAudioProperty
{
    std::uint32_t address;
    AudioSpatialId spatial;
};
// Invocation-only adapter. No new device or spatial/property owner is created.
// Inactive460AF0 branches never reach the source owner lookup.
class SourceEnvironmentPropertyCalls final : public AudioEnvironmentDevice
{
    RaceSessionPlatform &platform_;

  public:
    explicit SourceEnvironmentPropertyCalls(RaceSessionPlatform &platform) : platform_(platform) {}
    AudioResult query_audio_property(AudioSpatialId spatial, const AudioPropertyGuid &iid,
                                     AudioPropertyId &output) override
    {
        platform_.require_owner_thread();
        WindowsAudioDevice *actual = nullptr;
        try
        {
            actual = &platform_.source_audio_owner();
        }
        catch (const FormatError &)
        {
            throw RequiredSourceAudioProperty{0x460a9c, spatial};
        }
        // Do not catch a native exception or replace a real HRESULT.
        return actual->source_query_audio_property(spatial, iid, output);
    }
    AudioResult audio_property_support(AudioPropertyId property, const AudioPropertyGuid &set, std::uint32_t identifier,
                                       std::uint32_t &support) override
    {
        return platform_.source_audio_owner().source_audio_property_support(property, set, identifier, support);
    }
    AudioResult set_audio_property(AudioPropertyId property, const AudioPropertyGuid &set, std::uint32_t identifier,
                                   std::span<const std::uint8_t> instance, std::span<const std::uint8_t> data) override
    {
        return platform_.source_audio_owner().source_set_audio_property(property, set, identifier, instance, data);
    }
};
} // namespace
SourceAudioEnvironmentSelection update_source_audio_environment_446110(RaceSessionPlatform &platform,
                                                                       SourceAudioEnvironmentRegionCalls &regions_call)
{
    platform.require_owner_thread();
    auto &state = platform.state();
    if (!platform.source_construction_only_entry())
        throw FormatError("source446110 requires its source platform owner");
    SourceEnvironmentPropertyCalls property(platform);
    AudioEnvironmentWorld world{state.audio_environment, state.audio_driver, state.audio_initialization.spatial_enabled,
                                property};
    // 446110 captures region1's type before any current-camera read.
    // 446149 reads ONE following type even after a match;44612C then exits
    // without reading its camera or any further region. Do not scan to the
    // sentinel after selection, or skip that reached next-type load.
    auto &regions = state.audio_environment.regions;
    std::size_t index = 1;
    std::uint32_t selected = 0;
    auto type = regions[index].type;
    bool searching = true;
    while (type != 0xffffffffu)
    {
        if (!searching)
            break; // 44612C/44612F, after the preceding type read
        if (state.controls.current >= state.controls.controllers.size())
            throw SourceAllocationError(0x446131, "446110 current controller leaves its owner");
        const auto &position = state.controls.controllers[state.controls.current].camera.position;
        if (regions_call.contains_4278a0(regions[index], position) == 1u)
        {
            selected = static_cast<std::uint32_t>(index);
            searching = false;
        }
        if (index + 1u >= regions.size())
            throw SourceAllocationError(0x446149, "446110 next-type load leaves the represented source region bank");
        type = regions[index + 1u].type; // 446149, including after a match
        ++index;                         // 44614C
    }
    if (selected == state.audio_environment.current_region)
        return {SourceAudioEnvironmentStatus::completed, 0, {}};
    const auto selected_type = regions[selected].type;
    // Unsigned>6 covers the source signed JL followed by signed JG6.
    if (selected_type > 6)
        return {SourceAudioEnvironmentStatus::completed, 0, {}};
    try
    {
        (void)set_audio_environment(world, selected_type);
    }
    catch (const RequiredSourceAudioProperty &required)
    {
        return {SourceAudioEnvironmentStatus::property_interface_required, required.address, required.spatial};
    }
    state.audio_environment.current_region = selected; // 44616C, even when actual460AF0 returned false
    return {SourceAudioEnvironmentStatus::completed, 0, {}};
}
SourceAudioEnvironmentSelection select_source_audio_environment_460af0(RaceSessionPlatform &platform,
                                                                       std::uint32_t type)
{
    platform.require_owner_thread();
    auto &state = platform.state();
    SourceEnvironmentPropertyCalls property(platform);
    AudioEnvironmentWorld world{state.audio_environment, state.audio_driver, state.audio_initialization.spatial_enabled,
                                property};
    try
    {
        (void)set_audio_environment(world, type);
    }
    catch (const RequiredSourceAudioProperty &required)
    {
        return {SourceAudioEnvironmentStatus::property_interface_required, required.address, required.spatial};
    }
    return {SourceAudioEnvironmentStatus::completed, 0, {}};
}
namespace
{
struct RequiredSourceMusicStop
{
    SourceMusicStopStatus status;
    std::uint32_t address;
    std::uint64_t identity;
};
// Invocation-only view of the existing platform dependency. Source stream
// timer/buffer ownership has not been constructed by the reached cold path.
// Existing Modern/WM_TIMER identities cannot stand in for those source calls.
class SourceMusicStopBoundary final : public AudioDevice, public AudioStreamPlatform
{
    RaceSessionPlatform &platform_;
    [[noreturn]] static void unexpected()
    {
        throw FormatError("operation outside reached source45F9D0/460330 stop prefix");
    }

  public:
    explicit SourceMusicStopBoundary(RaceSessionPlatform &platform) : platform_(platform) {}
    std::uint32_t kill_timer(std::uint32_t timer) override
    {
        platform_.require_owner_thread();
        throw RequiredSourceMusicStop{SourceMusicStopStatus::timer_required, 0x460365, timer};
    }
    AudioResult stop(AudioBufferId buffer) override
    {
        platform_.require_owner_thread();
        throw RequiredSourceMusicStop{SourceMusicStopStatus::buffer_stop_required, 0x460379, buffer.value};
    }
    // These mandatory interface members cannot be reached before the two
    // unresolved calls above. Refuse instead of manufacturing a source result.
    AudioResult status(AudioBufferId, std::uint32_t &) override
    {
        unexpected();
    }
    AudioResult duplicate(AudioBufferId, AudioBufferId &) override
    {
        unexpected();
    }
    AudioResult query_spatial(AudioBufferId, AudioSpatialId &) override
    {
        unexpected();
    }
    std::uint32_t release(AudioBufferId) override
    {
        unexpected();
    }
    std::uint32_t release_spatial(AudioSpatialId) override
    {
        unexpected();
    }
    AudioResult frequency(AudioBufferId, std::uint32_t) override
    {
        unexpected();
    }
    AudioResult volume(AudioBufferId, std::int32_t) override
    {
        unexpected();
    }
    AudioResult pan(AudioBufferId, std::int32_t) override
    {
        unexpected();
    }
    AudioResult play(AudioBufferId, std::uint32_t, std::uint32_t, std::uint32_t) override
    {
        unexpected();
    }
    AudioResult spatial_parameters(AudioSpatialId, const AudioSpatialParameters &, std::uint32_t) override
    {
        unexpected();
    }
    AudioResult spatial_mode(AudioSpatialId, std::uint32_t, std::uint32_t) override
    {
        unexpected();
    }
    AudioResult spatial_position(AudioSpatialId, Vec3, std::uint32_t) override
    {
        unexpected();
    }
    AudioResult spatial_velocity(AudioSpatialId, Vec3, std::uint32_t) override
    {
        unexpected();
    }
    AudioResult listener_position(AudioListenerId, Vec3, std::uint32_t) override
    {
        unexpected();
    }
    AudioResult listener_velocity(AudioListenerId, Vec3, std::uint32_t) override
    {
        unexpected();
    }
    AudioResult listener_orientation(AudioListenerId, Vec3, Vec3, std::uint32_t) override
    {
        unexpected();
    }
    std::uint32_t end_timer_period(std::uint32_t) override
    {
        unexpected();
    }
    AudioResult set_play_cursor(AudioBufferId, std::uint32_t) override
    {
        unexpected();
    }
    std::int32_t seek_wave(AudioFileId, std::int32_t, std::int32_t) override
    {
        unexpected();
    }
    std::uint32_t close_wave(AudioFileId, std::uint32_t) override
    {
        unexpected();
    }
};
} // namespace
SourceMusicStopResult stop_source_music_45f9d0(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    if (const auto binding = platform.source_music_calls())
    {
        binding->stop();
        return {SourceMusicStopStatus::completed, 0, 0};
    } // Unbound cold paths retain the existing explicit source boundary below.
    platform.require_owner_thread();
    auto &state = platform.state();
    SourceMusicStopBoundary boundary(platform);
    try
    {
        stop_audio_music(AudioMusicStopWorld{state.audio_stream, state.audio_maintenance.music, state.audio_level,
                                             boundary, boundary});
    }
    catch (const RequiredSourceMusicStop &required)
    {
        return {required.status, required.address, required.identity};
    }
    return {SourceMusicStopStatus::completed, 0, 0};
}
namespace
{
struct RequiredSourceSampleStop
{
    AudioBufferId buffer;
};
// One call-local dependency borrowing the retained platform, not an owner or
// a native backend. Source sample tokens must not enter the Modern registry.
class SourceSampleStopBoundary final : public AudioDevice
{
    RaceSessionPlatform &platform_;
    [[noreturn]] static void unexpected()
    {
        throw FormatError("operation outside reached source460DD0/460D60 stop prefix");
    }

  public:
    explicit SourceSampleStopBoundary(RaceSessionPlatform &platform) : platform_(platform) {}
    AudioResult stop(AudioBufferId buffer) override
    {
        platform_.require_owner_thread();
        WindowsAudioDevice *owner = nullptr;
        // Only this lookup's absent-owner error is a named source boundary.
        // Do not catch native Stop exceptions or manufacture a returned HRESULT.
        try
        {
            owner = &platform_.source_audio_owner();
        }
        catch (const FormatError &)
        {
            throw RequiredSourceSampleStop{buffer};
        }
        if (!owner->source_sample_stop_admitted(buffer))
            throw RequiredSourceSampleStop{buffer};
        return owner->stop(buffer); // 460D79, before the existing current DEC.
    }
    AudioResult status(AudioBufferId, std::uint32_t &) override
    {
        unexpected();
    }
    AudioResult duplicate(AudioBufferId, AudioBufferId &) override
    {
        unexpected();
    }
    AudioResult query_spatial(AudioBufferId, AudioSpatialId &) override
    {
        unexpected();
    }
    std::uint32_t release(AudioBufferId) override
    {
        unexpected();
    }
    std::uint32_t release_spatial(AudioSpatialId) override
    {
        unexpected();
    }
    AudioResult frequency(AudioBufferId, std::uint32_t) override
    {
        unexpected();
    }
    AudioResult volume(AudioBufferId, std::int32_t) override
    {
        unexpected();
    }
    AudioResult pan(AudioBufferId, std::int32_t) override
    {
        unexpected();
    }
    AudioResult play(AudioBufferId, std::uint32_t, std::uint32_t, std::uint32_t) override
    {
        unexpected();
    }
    AudioResult spatial_parameters(AudioSpatialId, const AudioSpatialParameters &, std::uint32_t) override
    {
        unexpected();
    }
    AudioResult spatial_mode(AudioSpatialId, std::uint32_t, std::uint32_t) override
    {
        unexpected();
    }
    AudioResult spatial_position(AudioSpatialId, Vec3, std::uint32_t) override
    {
        unexpected();
    }
    AudioResult spatial_velocity(AudioSpatialId, Vec3, std::uint32_t) override
    {
        unexpected();
    }
    AudioResult listener_position(AudioListenerId, Vec3, std::uint32_t) override
    {
        unexpected();
    }
    AudioResult listener_velocity(AudioListenerId, Vec3, std::uint32_t) override
    {
        unexpected();
    }
    AudioResult listener_orientation(AudioListenerId, Vec3, Vec3, std::uint32_t) override
    {
        unexpected();
    }
};
} // namespace
SourceSampleStopResult stop_source_samples_460dd0(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    auto &state = platform.state();
    SourceSampleStopBoundary boundary(platform);
    AudioDriver driver{state.audio_driver, boundary, state.audio_level, state.controls, state.split_setting};
    try
    {
        audio_stop_all(driver);
    }
    catch (const RequiredSourceSampleStop &required)
    {
        return {SourceSampleStopStatus::buffer_stop_required, 0x460d79, required.buffer};
    }
    return {SourceSampleStopStatus::completed, 0, {}};
}
void reset_source_sound_output_460c90(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    auto &level = platform.state().audio_level;
    level.output_b = 255;      // 460C95,489798
    level.output_b_copy = 255; // 460C9A,48979C
}
AudioEnvironmentWorld audio_environment_world(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    auto &s = platform.state();
    return {s.audio_environment, s.audio_driver, s.audio_initialization.spatial_enabled, platform.audio_device()};
}
AudioStreamPlaybackWorld audio_stream_world(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    auto &s = platform.state();
    auto &device = platform.audio_device();
    return {{s.audio_driver, device, s.audio_level, s.controls, s.split_setting},
            s.audio_maintenance.music,
            s.audio_stream,
            s.audio_initialization.device,
            device,
            device,
            platform.audio_stream_platform(),
            device};
}
AudioEnvironmentWorld audio_environment_world(RaceSession &session)
{
    return audio_environment_world(*session.shared_platform());
}
AudioStreamPlaybackWorld audio_stream_world(RaceSession &session)
{
    return audio_stream_world(*session.shared_platform());
}
void maintain_session_audio(RaceSession &session)
{
    session.shared_platform()->require_owner_thread();
    auto &s = session.state();
    const auto retained = session.shared_platform();
    std::shared_ptr<SourceMusicCallBinding> source_music;
    if (retained->source_construction_only_entry())
    {
        source_music = retained->source_music_calls();
        if (source_music)
            source_music->check_health();
        else if (s.audio_driver.initialized != 0 && s.audio_level.output_a != 0)
            throw SourceMusicWaveUnknown("source music maintenance binding is absent");
    }
    else
        session.audio_stream_platform().check_health();
    auto environment = audio_environment_world(session);
    const auto &p = s.controls.controllers.at(s.controls.current).camera.position;
    update_audio_environment(environment, {p.x, p.y, p.z}); // 44288C, actual4722D0 controller camera
    auto driver = session.audio_view();
    audio_maintain({driver, s.audio_level, s.audio_maintenance, s.camera_presentation[0], source_music.get()});
}
void close_session_music(RaceSession &session)
{
    // This check must precede source writes as well as native map/window calls.
    // A retained frontend can prevent the final Platform destructor from
    // running, so checking only that destructor would be too late.
    try
    {
        session.shared_platform()->require_owner_thread();
    }
    catch (...)
    {
        std::terminate();
    }
    const auto retained = session.shared_platform();
    if (retained->source_construction_only_entry())
    {
        if (const auto binding = retained->source_music_calls())
            binding->close();
        else if (session.state().audio_stream.allocated != 0)
            throw SourceMusicWaveUnknown("allocated source music lacks its close binding");
        return; // Never resolve the preview registry for a source music buffer.
    }
    auto &s = session.state();
    auto &platform = session.audio_stream_platform();
    auto &device = session.audio_device(); // reject absent backend before source writes
    try
    {
        close_audio_stream(s.audio_stream, s.audio_maintenance.music, device, platform);
    }
    catch (...)
    { /* Native owners still release files/voices on resource destruction. */
    }
    platform.cancel_pending_callbacks();
}
} // namespace sgn::retail
