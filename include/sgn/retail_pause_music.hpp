#pragma once
#include <cstdint>

namespace sgn::retail
{
struct PauseMusicWorld
{
    const std::uint32_t &output_a;  // 62E0F8, existing LevelAudioState owner
    std::uint32_t &playback_active; // 62F5AC, existing AudioStreamState owner
    std::uint32_t &field_e100;      // 62E100, same AudioStreamState owner
    std::uint32_t &gain_step;       // 62E108, existing AudioMusicState owner
};
// Full45F8E0..45F8EC: raw ECX and EDX words, in source store order. The two
// destinations must be distinct valid uint32 objects, without concurrent mutation.
void set_music_words_45f8e0(std::uint32_t &field_e100, std::uint32_t &gain_step, std::uint32_t first,
                            std::uint32_t second) noexcept;
// Full45FA00..45FA28 /45FA30..45FA5B, including the real setter. All four
// borrowed words must be distinct, valid and stable against concurrent mutation.
// Typed void: original EAX residue is not a semantic status return.
void pause_music_45fa00(PauseMusicWorld) noexcept;
void resume_music_45fa30(PauseMusicWorld) noexcept;
} // namespace sgn::retail
