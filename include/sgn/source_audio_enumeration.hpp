#pragma once
#include "sgn/retail_audio_initialize.hpp"
#include <variant>

namespace sgn::retail
{
class RaceSessionPlatform;
struct SourceAudioEnumerationFailure
{
    AudioResult result; // Actual nonzero HRESULT.462090 returns source0 here.
};
struct SourceAudioDefaultDeviceBoundary
{
    static constexpr std::uint32_t source_site = 0x462146u;
    // Enumeration completed and its count is already published. The subsequent
    // DirectSoundCreate(NULL,&62E0B0,NULL) contract is not implemented by this
    // stage. Preserve all owners and reject an attempted cold continuation.
    [[noreturn]] void resume() const;
};
using SourceAudioEnumerationOutcome = std::variant<SourceAudioEnumerationFailure, SourceAudioDefaultDeviceBoundary>;

// Only46210E..462140. The caller must have executed the real462090 prefix and
// input_b gate. This function does not reset/initialize any source owner.
SourceAudioEnumerationOutcome enumerate_source_audio_stage_46210e(AudioEnumerationState &, AudioListenerId &,
                                                                  AudioEnumerationPlatform &);

// Borrows the actual persistent world; usable before Session construction and
// before preparing assets/XAudio2/stream objects. Owner-thread check comes first.
SourceAudioEnumerationOutcome enumerate_session_source_audio_46210e(RaceSessionPlatform &);
} // namespace sgn::retail
