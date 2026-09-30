#pragma once
#include <array>
#include <cstdint>

namespace sgn::retail
{
// Sole owners of the still-uninterpreted bytes in one source 0x7724 record.
// These are real bytes cleared at 444DC9 and copied at 413738, not host padding.
// Recovered named fields must replace their range here, never duplicate it.
// No native pointer or independent vehicle/world image is stored here.
struct SourceVehicleUninterpretedStorage final
{
    std::array<std::uint8_t, 4> offset_0458{};   // [0458,045C)
    std::array<std::uint8_t, 4> offset_1b88{};   // [1B88,1B8C)
    std::array<std::uint8_t, 4> offset_1cb8{};   // [1CB8,1CBC)
    std::array<std::uint8_t, 4> offset_1f18{};   // [1F18,1F1C)
    std::array<std::uint8_t, 16> offset_2000{};  // [2000,2010)
    std::array<std::uint8_t, 20> offset_203c{};  // [203C,2050)
    std::array<std::uint8_t, 16> offset_2098{};  // [2098,20A8)
    std::array<std::uint8_t, 20> offset_20d4{};  // [20D4,20E8)
    std::array<std::uint8_t, 16> offset_2130{};  // [2130,2140)
    std::array<std::uint8_t, 20> offset_216c{};  // [216C,2180)
    std::array<std::uint8_t, 16> offset_21c8{};  // [21C8,21D8)
    std::array<std::uint8_t, 20> offset_2204{};  // [2204,2218)
    std::array<std::uint8_t, 7> offset_2291{};   // [2291,2298)
    std::array<std::uint8_t, 4> offset_263c{};   // [263C,2640)
    std::array<std::uint8_t, 4> offset_28ac{};   // [28AC,28B0)
    std::array<std::uint8_t, 16> offset_350c{};  // [350C,351C)
    std::array<std::uint8_t, 16> offset_7290{};  // [7290,72A0)
    std::array<std::uint8_t, 4> offset_7378{};   // [7378,737C)
    std::array<std::uint8_t, 152> offset_757c{}; // [757C,7614)
    std::array<std::uint8_t, 4> offset_7624{};   // [7624,7628)
    std::array<std::uint8_t, 4> offset_7670{};   // [7670,7674)
};
static_assert(sizeof(SourceVehicleUninterpretedStorage) == 371);
} // namespace sgn::retail
