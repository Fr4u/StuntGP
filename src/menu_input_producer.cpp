#include "sgn/menu_input_producer.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
template <class T> T read(const T &value) noexcept
{
    return static_cast<const volatile T &>(value);
}
template <class T> void store(T &target, T value) noexcept
{
    static_cast<volatile T &>(target) = value;
}
void put(MenuInputBankView bank, unsigned index, std::uint8_t value) noexcept
{
    store(bank.current[index].get(), value);
}
// The source deliberately creates0/1 edge bytes. Raw words and copied old
// bytes are never routed through this conversion.
std::uint8_t edge(bool value) noexcept
{
    return value ? std::uint8_t{1} : std::uint8_t{0};
}

void previous_then_clear(MenuInputBankView bank) noexcept
{
    // 40D893..40D8A8: copy4, then2, then1; preserve arbitrary byte values.
    const std::array<std::uint8_t, 4> first{read(bank.current[0].get()), read(bank.current[1].get()),
                                            read(bank.current[2].get()), read(bank.current[3].get())};
    for (unsigned i = 0; i < 4; ++i)
        store(bank.previous[i].get(), first[i]);
    const std::array<std::uint8_t, 2> next{read(bank.current[4].get()), read(bank.current[5].get())};
    for (unsigned i = 0; i < 2; ++i)
        store(bank.previous[i + 4].get(), next[i]);
    store(bank.previous[6].get(), read(bank.current[6].get()));
    // 40D8AF..40D8B5: clear current4+2+1 ONLY after copying all seven.
    for (unsigned i = 0; i < 7; ++i)
        put(bank, i, std::uint8_t{0});
}
} // namespace

MenuInputBankView borrow_menu_input_bank(PauseMenuInputEdges &e, MenuInputExtraEdges &x) noexcept
{
    return {{x.current_1a8, x.current_1a9, e.up, e.down, e.activate, x.current_1ad, e.back},
            {x.previous_1af, x.previous_1b0, e.up_previous, e.down_previous, e.activate_previous, x.previous_1b4,
             e.back_previous}};
}

void produce_menu_input_40d860(MenuInputProducerWorld w) noexcept
{
    for (unsigned slot = 0; slot < 4; ++slot)
    {
        const auto &p = w.physical[slot];
        if (read(p.word_0) != 1u || read(p.word_8) != 0u)
            continue; // 40D871..40D87F
        previous_then_clear(p.edges);
        const auto kind = std::bit_cast<std::int32_t>(read(p.word_0c)); // 40D8B8
        if (kind == 1)
        { // 40D99F..40DA3D; exact source key-state byte indices
            if (read(w.scene) == 1u)
                put(p.edges, 6, edge((read(w.keyboard_62d480[0x1b]) & 0x80u) != 0));
            put(p.edges, 4, edge((read(w.keyboard_62d480[0x0d]) & 0x80u) != 0));
            put(p.edges, 5, edge((read(w.keyboard_62d480[0x1b]) & 0x80u) != 0));
            put(p.edges, 0, static_cast<std::uint8_t>(read(w.keyboard_62d480[0x25]) >> 7));
            put(p.edges, 1, edge((read(w.keyboard_62d480[0x27]) & 0x80u) != 0));
            put(p.edges, 2, edge((read(w.keyboard_62d480[0x26]) & 0x80u) != 0));
            put(p.edges, 3, edge((read(w.keyboard_62d480[0x28]) & 0x80u) != 0));
        }
        else if (kind > 1 && kind <= 6)
        {                                                                  // signed40D8C3/40D8CC
            const auto back_index = read(w.selectors.word_473984) & 0xffu; // 40D8D2/D8DE
            const auto range_bits = read(p.word_34);                       // 40D8D8
            // Exact32-bit SAR1 for every input bit pattern, including negative odd values.
            const auto half = std::bit_cast<std::int32_t>((range_bits >> 1) | (range_bits & 0x80000000u));
            const auto back = read(p.buttons_from_68[back_index]);              // 40D8E6
            store(w.source_device_pointer_576b6c, 0x0062d5acu + slot * 0x1dcu); // 40D8EB
            put(p.edges, 6, edge(back != 0));
            const auto activate_index = read(w.selectors.byte_473982); // 40D904
            put(p.edges, 4, edge(read(p.buttons_from_68[activate_index]) != 0));
            const auto other_index = read(w.selectors.byte_473983); // 40D924
            put(p.edges, 5, edge(read(p.buttons_from_68[other_index]) != 0));
            put(p.edges, 1, edge(std::bit_cast<std::int32_t>(read(p.word_38)) > half)); // 40D943
            // half is in[-2^30,2^30-1], so NEG cannot overflow int32.
            const auto negative_half = -half;
            put(p.edges, 0, edge(std::bit_cast<std::int32_t>(read(p.word_38)) < negative_half)); // 40D956
            put(p.edges, 3, edge(std::bit_cast<std::int32_t>(read(p.word_3c)) > half));          // 40D963
            put(p.edges, 2, edge(std::bit_cast<std::int32_t>(read(p.word_3c)) < negative_half)); // 40D983
        }
        // Other kinds keep cleared current bytes and newly copied previous bytes.
        // No implicit pause/input identity publication, activity update or reset.
    }
}

void publish_physical_input_4244c0(std::span<const AcquiredPhysicalInputView, 4> upstream,
                                   std::span<const PhysicalInputPublicationView, 4> physical) noexcept
{
    for (unsigned slot = 0; slot < 4; ++slot)
    {
        const auto &u = upstream[slot];
        const auto &p = physical[slot];
        const auto previous = read(p.word_0); // 4244CC
        const auto kind = read(u.word_4);     // 4244CE, before any output store
        store(p.word_4, previous);            // 4244D0
        const auto active = read(u.word_0);   // 4244D3
        store(p.word_0, active);              // 4244D6
        store(p.word_0c, kind);               // 4244DB
        const auto signed_kind = std::bit_cast<std::int32_t>(kind);
        if (active != 1u || signed_kind < 2 || signed_kind > 6)
            continue;
        for (unsigned i = 0; i < 31; ++i) // 4244F5 REP MOVSD:31 raw DWORDs
            store(p.words_from_2c[i], read(u.words_from_0c[i]));
        store(p.word_28, read(p.word_24)); // 4244F7..4244FA
        store(p.word_24, read(u.word_8));  // 4244FD..424500
    }
}
} // namespace sgn::retail
