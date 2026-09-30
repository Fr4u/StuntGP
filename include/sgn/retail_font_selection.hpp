#pragma once
#include "sgn/retail_font_resources.hpp"

namespace sgn::retail
{
// Full428F70. Source57B310 is represented by the existing one-based font
// identity, not a host pointer. Unsigned clamp and 32-bit source address wrap
// precede the host extent check. Large indices can wrap to a real font owner.
inline void select_retail_font_428f70(RetailFontResources &fonts, std::uint32_t requested)
{
    const auto slot = requested >= fonts.count ? fonts.count - 1u : requested;
    const std::uint32_t address = 0x577bd0u + slot * 0x1ba0u;
    if (address == 0x577bd0u)
        fonts.current = 1;
    else if (address == 0x579770u)
        fonts.current = 2;
    else
        throw FormatError("font selector outside two physical source owners");
}
} // namespace sgn::retail
