#pragma once
#include <cstdint>
#include <span>

namespace sgn::retail
{
inline constexpr unsigned frontend_font_glyphs = 160;
// Minimal projection consumed by4290D0, not a texture or renderer record.
// Per font: source +8 followed by160 source +28+44*i advance words.
struct FrontendFontMetricsViews
{
    std::span<std::uint32_t, 2 * (1 + frontend_font_glyphs)> records;
    std::span<std::uint8_t, 160> index;     // 577AD0
    std::span<std::uint8_t, 256> glyph_map; // 57B318
    std::uint32_t &current;                 // identity1..2;0 is no current record
    std::uint32_t &count;                   // 57B314
    std::uint32_t &digit_advance_bits;      // 57B418
};
struct FrontendFontInputs
{
    std::span<const std::uint8_t> index;
    std::span<const std::uint8_t> i32;
    std::span<const std::uint8_t> i32_b05;
    std::span<const std::uint8_t> i32_b025;
};
// 428FA0 input selection,4622C0 pixels,428AB0 bounds,428C00 advances,
// 428A80 map and428F70 current font, projected from full original CPU init.
// Borrows actual fonts.wad resource bytes. No GPU/texture/registry replacement.
void initialize_frontend_font_metrics(FrontendFontMetricsViews, FrontendFontInputs, std::int32_t source_width);
// Full arithmetic and font-selection effects of438910→428F70/4290D0 for
// finite scale and validated metrics. Binary64 preserves source x87 PC=53;
// the result must not be rounded to float before4409C0's final addition.
double measure_frontend_text_438910(FrontendFontMetricsViews, std::span<const std::uint8_t> nul_text, float scale);
} // namespace sgn::retail
