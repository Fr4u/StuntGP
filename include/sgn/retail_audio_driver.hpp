#pragma once
#include "sgn/assets.hpp"
#include "sgn/contact_player.hpp"
#include "sgn/retail_level_overrides.hpp"
#include <array>
#include <cstdint>
#include "sgn/source_listener_value.hpp"

namespace sgn::retail
{
// New host identities, not guest COM pointers or sound-slot indices. Zero is null.
struct AudioBufferId
{
    std::uint64_t value{};
    bool operator==(const AudioBufferId &) const = default;
};
struct AudioSpatialId
{
    std::uint64_t value{};
    bool operator==(const AudioSpatialId &) const = default;
};
struct AudioListenerId
{
    SourceListenerValue value{};
    bool operator==(const AudioListenerId &) const = default;
};
using AudioResult = std::int32_t;
// Literal 64-byte DS3DBUFFER value contract at 004897B8. XYZ only.
struct AudioSpatialParameters
{
    std::uint32_t size{};
    Vec3 position{}, velocity{};
    std::uint32_t inside_cone_angle{}, outside_cone_angle{};
    Vec3 cone_orientation{};
    std::int32_t outside_cone_volume{};
    float minimum_distance{}, maximum_distance{};
    std::uint32_t mode{};
};
// Mandatory OS boundary. Implementations must execute the requested operation and
// return its real result. Output references preserve COM writes even on failure.
// The recovered caller tests result == 0 (including positive nonzero results).
struct AudioDevice
{
    virtual ~AudioDevice() = default;
    virtual AudioResult status(AudioBufferId, std::uint32_t &flags) = 0;
    virtual AudioResult duplicate(AudioBufferId, AudioBufferId &output) = 0;
    virtual AudioResult query_spatial(AudioBufferId, AudioSpatialId &output) = 0;
    virtual std::uint32_t release(AudioBufferId) = 0;
    virtual std::uint32_t release_spatial(AudioSpatialId) = 0;
    virtual AudioResult frequency(AudioBufferId, std::uint32_t) = 0;
    virtual AudioResult volume(AudioBufferId, std::int32_t) = 0;
    virtual AudioResult pan(AudioBufferId, std::int32_t) = 0;
    virtual AudioResult play(AudioBufferId, std::uint32_t reserved1, std::uint32_t reserved2, std::uint32_t flags) = 0;
    virtual AudioResult stop(AudioBufferId) = 0;
    virtual AudioResult spatial_parameters(AudioSpatialId, const AudioSpatialParameters &, std::uint32_t apply) = 0;
    virtual AudioResult spatial_mode(AudioSpatialId, std::uint32_t mode, std::uint32_t apply) = 0;
    virtual AudioResult spatial_position(AudioSpatialId, Vec3, std::uint32_t apply) = 0;
    virtual AudioResult spatial_velocity(AudioSpatialId, Vec3, std::uint32_t apply) = 0;
    virtual AudioResult listener_position(AudioListenerId, Vec3, std::uint32_t apply) = 0;
    virtual AudioResult listener_velocity(AudioListenerId, Vec3, std::uint32_t apply) = 0;
    virtual AudioResult listener_orientation(AudioListenerId, Vec3 forward, Vec3 up, std::uint32_t apply) = 0;
};
struct AudioSample
{
    AudioBufferId buffer{};    // record +0
    AudioSpatialId spatial{};  // record +4
    std::uint32_t type{};      // record +8
    std::uint32_t frequency{}; // record +14
};
struct AudioDriverState
{
    std::array<AudioSample, 128> samples{};           // 0062E118, stride20, duplicates72..103
    std::uint32_t initialized{}, spatial_mode{};      // 62E0AC,4897B4
    std::uint32_t range{}, range_squared{};           // 4897A0,4897A4; wrapping square
    float inverse_range{};                            // 4897A8
    std::uint32_t duplicate_count{}, playing_count{}; // 62F5B0,62F5B4
    AudioSpatialParameters spatial_template{};        // 4897B8, copied before a 3D start
};
// Optional new diagnostic observer of the source461150 entry arguments. It
// observes requests even when audio is disabled; no ownership or guest-pointer
// conversion belongs here. Implementations must not mutate game state.
struct AudioRequestObserver
{
    virtual ~AudioRequestObserver() = default;
    virtual void observe(std::uint32_t sample, std::uint32_t flags, std::uint32_t frequency, std::uint32_t volume,
                         std::int32_t pan, std::uint32_t force, const std::uint32_t *handle) noexcept = 0;
};
struct AudioDriver
{
    AudioDriverState &state;
    AudioDevice &device;
    const LevelAudioState &level; // borrows output_b at489798
    const contact::player::Controls &controls;
    const std::uint32_t &split_setting; // 576BE0
    AudioRequestObserver *request_observer{};
};
// Literal source helpers; indices must address samples and the 129-entry volume
// table. Invalid source memory accesses are rejected, never clamped to success.
AudioSpatialParameters audio_default_spatial_parameters();
void audio_set_range(AudioDriverState &, std::uint32_t);                                                   // 460D20
void audio_stop_slot(AudioDriver &, std::uint32_t);                                                        // 460D60
void audio_stop_handle(AudioDriver &, std::uint32_t &);                                                    // 460D90
void audio_stop_all(AudioDriver &);                                                                        // 460DD0
bool audio_duplicate_2d(AudioDriver &, AudioBufferId &, std::uint32_t, std::uint32_t *);                   // 460F70
bool audio_duplicate_3d(AudioDriver &, AudioBufferId &, AudioSpatialId &, std::uint32_t, std::uint32_t *); // 460E70
unsigned audio_listener(const AudioDriver &, Vec3);                                                        // 461270
void audio_start(AudioDriver &, std::uint32_t sample, std::uint32_t flags, std::uint32_t frequency,
                 std::uint32_t volume, std::int32_t pan, std::uint32_t force, std::uint32_t *handle); // 461150/461030
void audio_start_positioned(AudioDriver &, std::uint32_t sample, std::uint32_t flags, std::uint32_t frequency,
                            std::uint32_t volume, Vec3 position, const Vec3 *velocity, std::uint32_t force,
                            std::uint32_t *handle); // 4614A0/461300
void audio_update_positioned(AudioDriver &, std::uint32_t &handle, std::uint32_t frequency, std::uint32_t volume,
                             Vec3 position, const Vec3 *velocity); // 461850/461730
} // namespace sgn::retail
