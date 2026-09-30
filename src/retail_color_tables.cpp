#include "sgn/retail_color_tables.hpp"
#include <algorithm>
#include <bit>
namespace sgn::retail
{
void initialize_color_tables(ColorTables &tables)
{
    if (std::bit_cast<std::uint32_t>(tables.exponent_4754c4) != 0x3f800000u ||
        std::bit_cast<std::uint32_t>(tables.exponent_4754c8) != 0x3f800000u)
        throw FormatError("color table exponents outside pinned startup domain");
    // 422380: pow(n,2) * binary32(1/pow(256,2)) *65280, clamp2048..65280,
    // then truncate. Every intermediate is exact for n=1..256, so integer
    // n*n*255/256 reproduces all256 original words without platform pow drift.
    for (std::uint32_t n = 1; n <= 256; ++n)
        tables.raster_gamma[n - 1] = std::clamp(n * n * 255u / 256u, 2048u, 65280u);
    // 422420: each exponent is exactly1, reciprocal128 is exactly representable.
    // Original writes5717D4 first, then5715D4. Neither loop accumulates floats.
    for (std::uint32_t n = 1; n <= 128; ++n)
        tables.curve_5717d4[n - 1] = static_cast<float>(n) / 128.0f;
    for (std::uint32_t n = 1; n <= 128; ++n)
        tables.curve_5715d4[n - 1] = static_cast<float>(n) / 128.0f;
}
} // namespace sgn::retail
