#include "sgn/retail_pause_input.hpp"

namespace sgn::retail
{
void update_pause_input_45ee60(PauseInputWorld world)
{
    if (static_cast<const volatile std::uint32_t &>(world.scene) != 1)
        return;                                                        // 45EE60..6D
    static_cast<volatile std::uint32_t &>(world.lifecycle.status) = 0; // 45EE73
    for (unsigned slot = 0; slot < 4; ++slot)
    {
        if (static_cast<const volatile std::uint32_t &>(world.physical_bindings[slot].vehicle) == 0xffffu)
            continue; // 45EE84..8B; no read of either raw input word
        const auto first = static_cast<const volatile std::uint32_t &>(world.physical_words[slot].word_0); // 45EE8D
        if (first == 0)
        {
            static_cast<volatile std::uint32_t &>(world.lifecycle.status) = 1; // 45EE93, every eligible zero
            // 45EE99 retains flags from TEST ECX,ECX: this JNZ cannot be taken
            // on this path. It does not re-read status or the physical word.
            if (static_cast<const volatile std::uint32_t &>(world.physical_words[slot].word_4) == 1)
            {                                                                                                 // 45EE9B
                const auto paused = static_cast<const volatile std::uint32_t &>(world.lifecycle.frame_pause); // 45EEB8
                static_cast<volatile std::uint32_t &>(world.previous_pause) = paused; // 45EEBF, even if nonzero
                if (paused != 0)
                    return;                          // 45EEC4, not an exact1 test
                enter_pause_4422c0(world.lifecycle); // 45EEC6, full real child
                static_cast<volatile std::uint32_t &>(world.lifecycle.physical_input.token) = slot + 1; // 45EEE5
                return; // source pointer62D580+slot*1DC normalized to token1..4
            }
        }
        else if (first == 1 && static_cast<const volatile std::uint32_t &>(world.physical_words[slot].word_4) == 0)
        {                                                                               // 45EEA0..A9
            if (static_cast<const volatile std::uint32_t &>(world.previous_pause) != 1) // 45EEEC, exact1 suppresses
                resume_pause_442350(world.lifecycle); // 45EEF4, unconditional real child once reached
            return;
        }
    }
}
} // namespace sgn::retail
