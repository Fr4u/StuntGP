#pragma once
#include "sgn/source_audio_default_device.hpp"
#include "sgn/window_input_owner.hpp"

namespace sgn::retail
{
// Reached461D25, before CreateSoundBuffer. Invocation-only references; the
// caller must retain the actual world/device. This is not a source-ready state.
struct SourceAudioPrimaryBufferBoundary
{
    static constexpr std::uint32_t source_site = 0x461d25u;
    const AudioResult cooperative_result; // Host observation; source ignores it.
    const AudioDeviceId device;
    AudioPrimaryId &output;           // same62E114; no write by the reached local prefix
    const AudioBufferRequest request; // source20-byte descriptor projection
    [[noreturn]] void resume() const;
};
// Requires the A2 reached point and the source HWND61C398 read by the caller.
// Source level is exactly3 (DSSCL_EXCLUSIVE). Any returned HRESULT proceeds into
// the local461CF0 descriptor prefix; a native exception remains an exception.
SourceAudioPrimaryBufferBoundary set_source_audio_cooperative_462165(const SourceAudioCooperativeLevelBoundary &,
                                                                     AudioDeviceId, AudioPrimaryId &,
                                                                     std::uintptr_t source_window,
                                                                     WindowsAudioDevice &);
// Borrow the SAME already attached HWND/HINSTANCE and existing audio owner;
// neither input initialization nor device/window/world construction is invoked.
SourceAudioPrimaryBufferBoundary set_platform_source_audio_cooperative_462165(
    const SourceAudioCooperativeLevelBoundary &, RaceSessionPlatform &, InputWindowHandles);
} // namespace sgn::retail
