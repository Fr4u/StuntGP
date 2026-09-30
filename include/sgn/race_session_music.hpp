#pragma once
#include "sgn/audio_stream_playback.hpp"
#include "sgn/audio_environment.hpp"
namespace sgn::retail
{
class RaceSession;
class RaceSessionPlatform;
// Source transition outcome, not a synthesized HRESULT. The caller of
// 4420E0 ignores460AF0's EAX. An unresolved native call is not a source return.
enum class SourceAudioEnvironmentStatus
{
    completed,
    property_interface_required
};
struct SourceAudioEnvironmentSelection
{
    SourceAudioEnvironmentStatus status;
    std::uint32_t blocked_address{};
    AudioSpatialId required_spatial{}; // borrowed source samples[1] identity
};
// Actual460AF0 on the same platform world. Inactive gates do no owner lookup.
// Active calls borrow the retained source DirectSound spatial/property owner;
// only a missing owner remains an explicit boundary, never preview fallback.
SourceAudioEnvironmentSelection select_source_audio_environment_460af0(RaceSessionPlatform &, std::uint32_t);
// Actual446110 region selection.4278A0 consumes a source stack lane whose
// producer is still unresolved. The existing preview predicate zeroes it and
// must not silently supply a result here. This mandatory synchronous receiver
// must implement the reached child or throw; it has no success default.
struct SourceAudioEnvironmentRegionCalls
{
    virtual ~SourceAudioEnvironmentRegionCalls() = default;
    virtual std::uint32_t contains_4278a0(const AudioEnvironmentRegion &, const CameraVector &) = 0;
};
// The camera/region references are borrowed at the original call, not copied
// during outer entry. Inactive property gates retain lazy native owner lookup.
SourceAudioEnvironmentSelection update_source_audio_environment_446110(RaceSessionPlatform &,
                                                                       SourceAudioEnvironmentRegionCalls &);
enum class SourceMusicStopStatus
{
    completed,
    timer_required,
    buffer_stop_required
};
struct SourceMusicStopResult
{
    SourceMusicStopStatus status;
    std::uint32_t blocked_address{};
    std::uint64_t required_identity{}; // timer or same source music buffer token
};
// Actual45F9D0/460330 on retained world; no owner construction or eager lookup.
SourceMusicStopResult stop_source_music_45f9d0(RaceSessionPlatform &);
enum class SourceSampleStopStatus
{
    completed,
    buffer_stop_required
};
struct SourceSampleStopResult
{
    SourceSampleStopStatus status;
    std::uint32_t blocked_address{};
    AudioBufferId required_buffer{}; // same full-width retained sample token
};
// Actual460DD0/460D60 on the retained driver; no native owner construction.
SourceSampleStopResult stop_source_samples_460dd0(RaceSessionPlatform &);
// Exact460C90 stores; callers must first complete the preceding460DD0.
void reset_source_sound_output_460c90(RaceSessionPlatform &);
// The frontend can borrow these same live owners while no level is attached.
AudioStreamPlaybackWorld audio_stream_world(RaceSessionPlatform &);
AudioEnvironmentWorld audio_environment_world(RaceSessionPlatform &);
AudioStreamPlaybackWorld audio_stream_world(RaceSession &);
AudioEnvironmentWorld audio_environment_world(RaceSession &); // borrows the sole Session owners
void maintain_session_audio(RaceSession &);                   // 446110 then462280, once per source outer frame
void close_session_music(RaceSession &);                      // must run while Session state is alive
} // namespace sgn::retail
