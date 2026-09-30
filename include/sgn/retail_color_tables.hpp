#pragma once
#include "sgn/assets.hpp"
#include <array>
namespace sgn::retail
{
struct ColorTables
{
    std::array<std::uint32_t, 256> raster_gamma{}; // 62457C..62497B
    std::array<float, 128> curve_5715d4{}, curve_5717d4{};
    float exponent_4754c4{1}, exponent_4754c8{1}; // pinned PE values
};
// Complete422420/422380 output for the pinned cold-start parameter domain.
// Both mutable source exponents must still be1. Other values are not silently
// approximated with a different platform's pow implementation.
void initialize_color_tables(ColorTables &);
} // namespace sgn::retail
