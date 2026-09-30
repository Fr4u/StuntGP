#include "sgn/playable_menu_input.hpp"

namespace sgn::retail
{
void bind_playable_menu_keyboard(PlayableMenuKeyboardWorld w)
{
    for (unsigned i = 1; i < 4; ++i)
        if (w.menu.raw_words[i].word_0 == 1)
            throw FormatError("playable menu host requires one keyboard device");
    const auto &first = w.raw.physical[0];
    if (w.menu.raw_words[0].word_0 == 1 && (first.word_0c != 1 || first.word_8 != 0))
        throw FormatError("playable menu host cannot replace an active non-keyboard device");
    // New host device binding only; no inferred source startup/reset.
    w.menu.raw_words[0].word_0 = 1;
    w.raw.physical[0].word_8 = 0;
    w.raw.physical[0].word_0c = 1;
}
void publish_playable_menu_keyboard(PlayableMenuKeyboardWorld w, std::span<const bool, 256> keys)
{
    if (w.menu.raw_words[0].word_0 != 1 || w.raw.physical[0].word_8 != 0 || w.raw.physical[0].word_0c != 1)
        throw FormatError("playable menu keyboard binding changed");
    for (unsigned i = 1; i < 4; ++i)
        if (w.menu.raw_words[i].word_0 == 1)
            throw FormatError("playable menu host acquired another active input device");
    auto keyboard = menu_keyboard_bytes(w.keyboard);
    for (unsigned i = 0; i < 256; ++i)
        keyboard[i] = keys[i] ? std::uint8_t{0x80} : std::uint8_t{0};
    const auto physical = menu_physical_producer_views(w.raw, w.menu);
    produce_menu_input_40d860({w.scene, keyboard, physical, w.selectors, w.raw.source_device_pointer_576b6c});
}
bool PlayablePauseKey::consume(std::uint32_t scene, std::uint32_t frame_pause, std::span<const bool, 256> keys) noexcept
{
    const bool down = keys[0x1b];
    const bool rising = down && !escape_down_;
    escape_down_ = down;
    return rising && scene == 1 && frame_pause == 0;
}
} // namespace sgn::retail
