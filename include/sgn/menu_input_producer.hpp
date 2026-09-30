#pragma once
#include "sgn/retail_pause_menu.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <span>

namespace sgn::retail
{
// Private proposal, not attached or execution-qualified. These are the six
// bytes missing from each existing eight-byte PauseMenuInputEdges owner.
// Source PE seed is zero. Construction is not a replacement for 45EC30.
struct MenuInputExtraEdges
{
    std::uint8_t current_1a8{}, current_1a9{}, current_1ad{};
    std::uint8_t previous_1af{}, previous_1b0{}, previous_1b4{};
};
static_assert(sizeof(MenuInputExtraEdges) == 6);

// Seven logical addresses in SOURCE order. References borrow the existing
// eight owner bytes plus six extra bytes; no contiguous cast or shadow bank.
struct MenuInputBankView
{
    std::array<std::reference_wrapper<std::uint8_t>, 7> current;  // +1A8..1AE
    std::array<std::reference_wrapper<std::uint8_t>, 7> previous; // +1AF..1B5
};
MenuInputBankView borrow_menu_input_bank(PauseMenuInputEdges &, MenuInputExtraEdges &) noexcept;

struct MenuPhysicalInputView
{
    const std::uint32_t &word_0;  // existing Session raw_words[slot].word_0
    const std::uint32_t &word_8;  // physical+8, exact zero enables this path
    const std::uint32_t &word_0c; // raw device kind; NOT frontend binding+1C
    const std::uint32_t &word_34; // raw signed axis range; SAR, not float division
    const std::uint32_t &word_38; // raw signed horizontal sample
    const std::uint32_t &word_3c; // raw signed vertical sample
    // Source addresses physical+68+uint8 selector. Borrow real record bytes,
    // including retained bytes outside the 124-byte 4244C0 copy destination.
    std::span<const std::uint8_t, 256> buttons_from_68;
    MenuInputBankView edges;
};
struct MenuDeviceSelectorsView
{
    const std::uint8_t &byte_473982;
    const std::uint8_t &byte_473983;
    const std::uint32_t &word_473984; // full source read, low byte is the index
};
struct MenuInputProducerWorld
{
    const std::uint32_t &scene; // 612BE8, affects keyboard back only
    std::span<const std::uint8_t, 256> keyboard_62d480;
    std::span<const MenuPhysicalInputView, 4> physical;
    MenuDeviceSelectorsView selectors;
    // Raw source identity, written only for admitted device kinds2..6.
    // Retain its prior value for keyboard/invalid/skipped records. This is
    // 576B6C, NOT61B744 physical menu selection and NOT current vehicle.
    std::uint32_t &source_device_pointer_576b6c;
};
// Full40D860..40DA5A,133 instructions/507 bytes. No platform call or pause gate
// is hidden here. Caller decides the source cadence and supplies acquired raw
// state. No callbacks or concurrent mutation; all distinct source addresses
// must borrow distinct valid storage. Seven-byte copies are scattered only
// because the accepted owner layout is paired, not source-contiguous.
void produce_menu_input_40d860(MenuInputProducerWorld) noexcept;

// Full4244C0 raw-device publication, after platform acquisition. Upstream
// arrays start576618 with stride88; destination records start62D580/stride1DC.
// Device polling/keyboard acquisition are explicitly outside this leaf.
struct AcquiredPhysicalInputView
{
    const std::uint32_t &word_0; // upstream active/status, no Boolean conversion
    const std::uint32_t &word_4; // upstream raw device kind
    const std::uint32_t &word_8;
    std::span<const std::uint32_t, 31> words_from_0c; // 124 source bytes
};
struct PhysicalInputPublicationView
{
    std::uint32_t &word_0; // reuse Session raw_words[slot].word_0
    std::uint32_t &word_4; // reuse Session raw_words[slot].word_4
    std::uint32_t &word_0c;
    std::uint32_t &word_24;
    std::uint32_t &word_28;
    std::span<std::uint32_t, 31> words_from_2c; // actual retained source storage
};
// Source forward REP MOVSD domain; source/destination blocks must be disjoint.
// Invalid/non-exact active/kind keeps payload,+24,+28 unchanged, but ALWAYS
// publishes old+0 to+4 and the two new raw words to+0/+0C.
void publish_physical_input_4244c0(std::span<const AcquiredPhysicalInputView, 4>,
                                   std::span<const PhysicalInputPublicationView, 4>) noexcept;
} // namespace sgn::retail
