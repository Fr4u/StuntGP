#include "sgn/menu_keyboard_acquisition.hpp"
#include <bit>
#include <type_traits>

namespace sgn::retail
{
namespace
{
static_assert(std::endian::native == std::endian::little);
static_assert(std::is_same_v<std::uint8_t, unsigned char>);
template <class T> T read(const T &value) noexcept
{
    return static_cast<const volatile T &>(value);
}
template <class T> void store(T &where, T value) noexcept
{
    static_cast<volatile T &>(where) = value;
}
AcquiredPhysicalInputView upstream(const MenuKeyboardRawState &state, unsigned i) noexcept
{
    const auto &words = state.upstream[i].words;
    return {words[0], words[1], words[2], std::span<const std::uint32_t, 31>{words.data() + 3, 31}};
}
PhysicalInputPublicationView publication(MenuKeyboardRawState &raw, SessionMenuInputState &menu, unsigned i) noexcept
{
    auto &p = raw.physical[i];
    return {menu.raw_words[i].word_0,
            menu.raw_words[i].word_4,
            p.word_0c,
            p.word_24,
            p.word_28,
            std::span<std::uint32_t, 31>{p.words_from_2c.data(), 31}};
}
MenuPhysicalInputView producer(MenuKeyboardRawState &raw, SessionMenuInputState &menu, unsigned i) noexcept
{
    auto &p = raw.physical[i];
    const auto *bytes = reinterpret_cast<const unsigned char *>(p.words_from_2c.data());
    return {menu.raw_words[i].word_0,
            p.word_8,
            p.word_0c,
            p.words_from_2c[2],
            p.words_from_2c[3],
            p.words_from_2c[4],
            std::span<const std::uint8_t, 256>{bytes + 0x3c, 256},
            borrow_menu_input_bank(menu.edges[i], p.extra_edges)};
}
} // namespace

std::span<std::uint8_t, 256> menu_keyboard_bytes(RaceInitializationState &state) noexcept
{
    return std::span<std::uint8_t, 256>{reinterpret_cast<unsigned char *>(state.fields_62d480.data()), 256};
}
std::span<const std::uint8_t, 256> menu_keyboard_bytes(const RaceInitializationState &state) noexcept
{
    return std::span<const std::uint8_t, 256>{reinterpret_cast<const unsigned char *>(state.fields_62d480.data()), 256};
}
std::array<AcquiredPhysicalInputView, 4> menu_upstream_publication_views(const MenuKeyboardRawState &state) noexcept
{
    return {upstream(state, 0), upstream(state, 1), upstream(state, 2), upstream(state, 3)};
}
std::array<PhysicalInputPublicationView, 4> menu_physical_publication_views(MenuKeyboardRawState &raw,
                                                                            SessionMenuInputState &menu) noexcept
{
    return {publication(raw, menu, 0), publication(raw, menu, 1), publication(raw, menu, 2), publication(raw, menu, 3)};
}
std::array<MenuPhysicalInputView, 4> menu_physical_producer_views(MenuKeyboardRawState &raw,
                                                                  SessionMenuInputState &menu) noexcept
{
    return {producer(raw, menu, 0), producer(raw, menu, 1), producer(raw, menu, 2), producer(raw, menu, 3)};
}

void initialize_keyboard_upstream_projection_4249b0(MenuKeyboardRawState &raw) noexcept
{
    for (auto &record : raw.upstream)
        for (auto &word : record.words)
            store(word, std::uint32_t{0});                              // 4249C9
    store(raw.upstream[0].words[0], std::uint32_t{1});                  // 4249EE
    store(raw.upstream[0].words[1], std::uint32_t{1});                  // 4249F4
    store(raw.source_device_pointer_576b6c, std::uint32_t{0x005766ac}); // 4249FA
}

MenuKeyboardDispatchResult acquire_keyboard_domain_4243f0(MenuKeyboardRawState &raw, RaceInitializationState &keyboard,
                                                          MenuKeyboardAcquisition &platform)
{
    for (unsigned slot = 0; slot < 10; ++slot)
    {
        const auto &words = raw.upstream[slot].words;
        if (read(words[0]) != 1u)
            continue;                                     // 4243FD..424404
        const auto kind = read(words[1]);                 // 424406
        const auto table_index = kind - std::uint32_t{1}; // 42440C DEC, wrapping32 bits
        if (table_index > 5u)
            continue; // 42440D..424410 unsigned JA
        if (kind == 1u)
        {
            if (slot == 0)
                platform.acquire_45ec00(menu_keyboard_bytes(keyboard)); // 424419..42441D
        }
        else
        {
            // 424424..424426 would call the complete DirectInput child here.
            // Unimplemented domain: stop explicitly; never report completion.
            return {MenuKeyboardDispatchStatus::direct_input_424520_required, slot, kind};
        }
    }
    return {};
}

MenuKeyboardDispatchResult acquire_publish_keyboard_prefix_45f210(MenuKeyboardRawState &raw,
                                                                  SessionMenuInputState &menu,
                                                                  RaceInitializationState &keyboard,
                                                                  std::uint32_t &second_pause_612958,
                                                                  MenuKeyboardAcquisition &platform)
{
    store(second_pause_612958, std::uint32_t{0}); // 45F221
    for (unsigned i = 0; i < 64; ++i)             // 45F22B, forward64-DWORD copy
        store(keyboard.fields_62dcf0[i], read(keyboard.fields_62d480[i]));
    const auto result = acquire_keyboard_domain_4243f0(raw, keyboard, platform); // 45F22D
    if (result.status != MenuKeyboardDispatchStatus::completed)
        return result;
    const auto from = menu_upstream_publication_views(raw);
    const auto to = menu_physical_publication_views(raw, menu);
    publish_physical_input_4244c0(from, to); // 45F232, existing full leaf, no duplicate implementation
    return result;
}
} // namespace sgn::retail
