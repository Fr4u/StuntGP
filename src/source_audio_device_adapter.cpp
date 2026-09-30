#include "sgn/source_audio_device_adapter.hpp"
#include "sgn/platform_audio.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/source_allocation_bank.hpp"

// NEW host adapter (R18 WP-D / DESIGN A5), moved unchanged from the R17
// SourceVehicleAudioDevice in source_vehicle_replay_audio_reset.cpp:72-107.
// Every operation forwards to the SAME retained source DirectSound owner; the
// source_sample_* children own the COM retention ledger and the exact SDK call
// (platform_audio.cpp Impl::sample_*). No result is fabricated here.
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
WindowsAudioDevice &owner(RaceSessionPlatform &p)
{
    p.require_owner_thread();
    return p.source_audio_owner();
}
[[noreturn]] void unreached(W site)
{
    throw SourceAllocationError(site, "audio listener operation is outside the reached AudioDriver domain");
}
} // namespace
// 460E87 (460E70) / 460F87 (460F70): [buffer]->GetStatus(&local).
AudioResult SourceAudioDeviceAdapter::status(AudioBufferId id, std::uint32_t &flags)
{
    return owner(platform_).source_sample_status(id, flags);
}
// 460ECF..460EDC / 460FCB..460FD5: MOV EAX,[62E0B0]; [device]->DuplicateSoundBuffer(buffer,out).
// 62E0B0 is the retained source device of this world: state().audio_initialization.device.
AudioResult SourceAudioDeviceAdapter::duplicate(AudioBufferId id, AudioBufferId &output)
{
    auto &o = owner(platform_);
    return o.source_sample_duplicate(platform_.state().audio_initialization.device, id, output);
}
// 460EEE..460EFD: [duplicate]->QueryInterface(46FE90 IID_IDirectSound3DBuffer,&local).
AudioResult SourceAudioDeviceAdapter::query_spatial(AudioBufferId id, AudioSpatialId &output)
{
    return owner(platform_).source_sample_query_spatial(id, output);
}
// 460F03..460F09 (QI failure arm of 460E70): [duplicate]->Release().
std::uint32_t SourceAudioDeviceAdapter::release(AudioBufferId id)
{
    return owner(platform_).source_sample_release(id);
}
// 460E30..460E33: [spatial]->Release().
std::uint32_t SourceAudioDeviceAdapter::release_spatial(AudioSpatialId id)
{
    return owner(platform_).source_sample_release_spatial(id);
}
AudioResult SourceAudioDeviceAdapter::frequency(AudioBufferId id, std::uint32_t value)
{
    return owner(platform_).source_sample_frequency(id, value); // 4611E8 SetFrequency
}
AudioResult SourceAudioDeviceAdapter::volume(AudioBufferId id, std::int32_t value)
{
    return owner(platform_).source_sample_volume(id, value); // 461214 SetVolume
}
AudioResult SourceAudioDeviceAdapter::pan(AudioBufferId id, std::int32_t value)
{
    return owner(platform_).source_sample_pan(id, value); // 461241 SetPan
}
// 461258: [buffer]->Play(0,0,flags). The two reserved DWORDs are literal PUSH 0.
AudioResult SourceAudioDeviceAdapter::play(AudioBufferId id, std::uint32_t reserved1, std::uint32_t reserved2,
                                           std::uint32_t flags)
{
    if (reserved1 != 0 || reserved2 != 0)
        throw SourceAllocationError(0x461258, "source Play reserved arguments are literal zero");
    return owner(platform_).source_sample_play(id, flags);
}
// 460D76..460D79 (460D60) / 460DAF..460DB2 (460D90): [buffer]->Stop().
AudioResult SourceAudioDeviceAdapter::stop(AudioBufferId id)
{
    auto &o = owner(platform_);
    if (!o.source_sample_stop_admitted(id))
        throw SourceAllocationError(0x460d79, "source Stop requires this owner's retained buffer");
    return o.stop(id); // source path of WindowsAudioDevice::stop: Impl::stop_source_sample
}
AudioResult SourceAudioDeviceAdapter::spatial_parameters(AudioSpatialId id, const AudioSpatialParameters &value,
                                                         std::uint32_t apply)
{
    return owner(platform_).source_sample_spatial_parameters(id, value, apply); // 46142B SetAllParameters(4897B8 image)
}
AudioResult SourceAudioDeviceAdapter::spatial_mode(AudioSpatialId id, std::uint32_t mode, std::uint32_t apply)
{
    return owner(platform_).source_sample_spatial_mode(id, mode, apply); // 4610A5 SetMode
}
AudioResult SourceAudioDeviceAdapter::spatial_position(AudioSpatialId id, Vec3 value, std::uint32_t apply)
{
    return owner(platform_).source_sample_spatial_position(id, value, apply); // 4617F0 SetPosition
}
AudioResult SourceAudioDeviceAdapter::spatial_velocity(AudioSpatialId id, Vec3 value, std::uint32_t apply)
{
    return owner(platform_).source_sample_spatial_velocity(id, value, apply); // 46180A SetVelocity
}
AudioResult SourceAudioDeviceAdapter::listener_position(AudioListenerId, Vec3, std::uint32_t)
{
    unreached(0x461b00);
}
AudioResult SourceAudioDeviceAdapter::listener_velocity(AudioListenerId, Vec3, std::uint32_t)
{
    unreached(0x461b1d);
}
AudioResult SourceAudioDeviceAdapter::listener_orientation(AudioListenerId, Vec3, Vec3, std::uint32_t)
{
    unreached(0x461b5e);
}
} // namespace sgn::retail
