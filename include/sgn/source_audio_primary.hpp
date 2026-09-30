#pragma once
#include "sgn/source_audio_cooperative.hpp"
#include <variant>

namespace sgn::retail
{
struct SourceAudioPrimaryFailure
{
    AudioResult result; // Actual nonzero HRESULT; source 461CF0 returns false.
};
// Reached device GetCaps, before its invocation. Only dwSize has been written
// to the source stack descriptor. The remaining 23 words are not fabricated.
struct SourceAudioDeviceCapsBoundary
{
    static constexpr std::uint32_t source_site = 0x461d4cu;
    const AudioDeviceId device;
    AudioPrimaryId &primary; // Same output slot written by 461D25.
    const std::uint32_t requested_size = 96;
    [[noreturn]] void resume() const;
};
using SourceAudioPrimaryOutcome = std::variant<SourceAudioPrimaryFailure, SourceAudioDeviceCapsBoundary>;
SourceAudioPrimaryOutcome create_source_audio_primary_461d25(const SourceAudioPrimaryBufferBoundary &,
                                                             WindowsAudioDevice &);
SourceAudioPrimaryOutcome create_platform_source_audio_primary_461d25(const SourceAudioPrimaryBufferBoundary &,
                                                                      RaceSessionPlatform &);
} // namespace sgn::retail
