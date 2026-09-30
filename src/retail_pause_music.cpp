#include "sgn/retail_pause_music.hpp"

namespace sgn::retail
{
void set_music_words_45f8e0(std::uint32_t &field_e100, std::uint32_t &gain_step, std::uint32_t first,
                            std::uint32_t second) noexcept
{
    // Preserve both ordered source stores even when a value is unchanged.
    // Volatile does not supply cross-thread synchronization.
    static_cast<volatile std::uint32_t &>(field_e100) = first; // 45F8E0
    static_cast<volatile std::uint32_t &>(gain_step) = second; // 45F8E6
}
void pause_music_45fa00(PauseMusicWorld world) noexcept
{
    if (world.playback_active != 1u)
        return; // 45FA00/07, before output read
    if (world.output_a == 0u)
        return; // 45FA09..10
    set_music_words_45f8e0(world.field_e100, world.gain_step, 0u, 0xfffffff6u);
    static_cast<volatile std::uint32_t &>(world.playback_active) = 2u; // 45FA1E
}
void resume_music_45fa30(PauseMusicWorld world) noexcept
{
    if (world.playback_active != 2u)
        return; // 45FA30/37, before output read
    if (world.output_a == 0u)
        return; // 45FA39..40
    set_music_words_45f8e0(world.field_e100, world.gain_step, 255u, 10u);
    static_cast<volatile std::uint32_t &>(world.playback_active) = 1u; // 45FA51
}
} // namespace sgn::retail
