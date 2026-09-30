#pragma once
#include "sgn/retail_audio_driver.hpp"
#include <cstdint>

namespace sgn::retail
{
class RaceSessionPlatform;
// NEW host adapter (R18 WP-D, DESIGN A5): the retail AudioDevice contract used by
// AudioDriver (460D60/460D90/460E70/460F70/461150/4614A0/461850 and their 461030/
// 461300/461730 children) forwarded onto the SAME retained source DirectSound
// owner, RaceSessionPlatform::source_audio_owner(), through its source_sample_*
// operations. One audio owner: no preview XAudio2 registry, no fallback device.
// Moved unchanged from source_vehicle_replay_audio_reset.cpp (R17
// SourceVehicleAudioDevice). The borrowed platform must outlive the adapter.
class SourceAudioDeviceAdapter final : public AudioDevice
{
    RaceSessionPlatform &platform_;

  public:
    explicit SourceAudioDeviceAdapter(RaceSessionPlatform &platform) noexcept : platform_(platform) {}
    // clang-format off: keep each address comment on one line
    AudioResult status(AudioBufferId,std::uint32_t& flags) override;             //460E87/460F87 GetStatus
    AudioResult duplicate(AudioBufferId,AudioBufferId& output) override;         //460EDC/460FD5 [62E0B0]->DuplicateSoundBuffer
    AudioResult query_spatial(AudioBufferId,AudioSpatialId& output) override;    //460EFD QI IID_IDirectSound3DBuffer
    std::uint32_t release(AudioBufferId) override;                              //460F09 (and 460E39) Release
    std::uint32_t release_spatial(AudioSpatialId) override;                     //460E33 Release
    AudioResult frequency(AudioBufferId,std::uint32_t) override;                //4611E8 SetFrequency
    AudioResult volume(AudioBufferId,std::int32_t) override;                    //461214 SetVolume
    AudioResult pan(AudioBufferId,std::int32_t) override;                       //461241 SetPan
    AudioResult play(AudioBufferId,std::uint32_t reserved1,std::uint32_t reserved2,std::uint32_t flags) override; //461258 Play(0,0,flags)
    AudioResult stop(AudioBufferId) override;                                   //460D79/460DB2 Stop
    AudioResult spatial_parameters(AudioSpatialId,const AudioSpatialParameters&,std::uint32_t apply) override; //46142B SetAllParameters
    AudioResult spatial_mode(AudioSpatialId,std::uint32_t mode,std::uint32_t apply) override;                   //4610A5 SetMode
    AudioResult spatial_position(AudioSpatialId,Vec3,std::uint32_t apply) override;                            //4617F0 SetPosition
    AudioResult spatial_velocity(AudioSpatialId,Vec3,std::uint32_t apply) override;                            //46180A SetVelocity
    // clang-format on
    // Not reached by AudioDriver (retail_audio_driver.cpp has no listener call);
    // the only listener writer is 462280 maintenance, which the menu world runs
    // on the source owner itself (source_menu_audio_maintenance.cpp). Explicit
    // SourceAllocationError at the source listener sites, never a silent success.
    AudioResult listener_position(AudioListenerId, Vec3, std::uint32_t apply) override;
    AudioResult listener_velocity(AudioListenerId, Vec3, std::uint32_t apply) override;
    AudioResult listener_orientation(AudioListenerId, Vec3 forward, Vec3 up, std::uint32_t apply) override;
};
} // namespace sgn::retail
