#pragma once
#include <array>
#include <cstdint>
namespace sgn::retail
{
// Sole still-uninterpreted bytes in source strides6/7 cleared by40A305.
// Existing configuration/presentation/model/wheel lanes remain in their owners.
// Not extra physical vehicles and not a duplicate complete vehicle image.
struct SourceVehicleColdTail final
{
    std::array<std::uint8_t, 0x1ffc> offset_0020{}; //[0020,201C)
    std::array<std::uint8_t, 0x44> offset_202c{};   //[202C,2070)
    std::array<std::uint8_t, 0x3c> offset_2078{};   //[2078,20B4)
    std::array<std::uint8_t, 0x44> offset_20c4{};   //[20C4,2108)
    std::array<std::uint8_t, 0x3c> offset_2110{};   //[2110,214C)
    std::array<std::uint8_t, 0x44> offset_215c{};   //[215C,21A0)
    std::array<std::uint8_t, 0x3c> offset_21a8{};   //[21A8,21E4)
    std::array<std::uint8_t, 0x44> offset_21f4{};   //[21F4,2238)
    std::array<std::uint8_t, 0x470> offset_2240{};  //[2240,26B0)
    std::array<std::uint8_t, 0x8e8> offset_284c{};  //[284C,3134)
    std::array<std::uint8_t, 0x290> offset_3138{};  //[3138,33C8)
    std::array<std::uint8_t, 0x13c> offset_33cc{};  //[33CC,3508)
    std::array<std::uint8_t, 0x418> offset_350c{};  //[350C,3924)
    std::array<std::uint8_t, 0x3ce8> offset_392c{}; //[392C,7614)
    std::array<std::uint8_t, 0x108> offset_761c{};  //[761C,7724)
};
static_assert(sizeof(SourceVehicleColdTail) == 29932);
} // namespace sgn::retail
