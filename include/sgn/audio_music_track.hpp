#pragma once
#include <cstdint>
namespace sgn::retail
{
// Sole world music selection at62F5A8. Separate from stream playback_active
// at62F5AC and the transient level configuration's requested track.
struct AudioMusicTrack
{
    std::uint32_t selected{};
};
inline void select_audio_music_track(AudioMusicTrack &state, std::uint32_t track) noexcept
{
    state.selected = track;
} // 45F7C0
} // namespace sgn::retail
