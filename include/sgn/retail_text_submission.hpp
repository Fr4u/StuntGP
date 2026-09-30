#pragma once
#include "sgn/retail_font_resources.hpp"
#include "sgn/retail_polygon_submission.hpp"

namespace sgn::retail
{
struct TextSubmission
{
    RetailFontResources &fonts;       // sole full font/metric/map/current/count owners
    std::uint32_t &selected_material; // shared61C5E4
    std::uint32_t &specular_enabled;  // shared592050
    std::span<const PolygonSubmissionMaterial> materials;
    const std::uint32_t &quality;                   // live624984
    const std::uint32_t &legacy_driver_restriction; // live589E08
    const ViewportState &viewport;
};
struct TextGlyphParameters
{
    float x{}, y{}, depth{};
    std::uint32_t color{};
    float scale{};
    std::uint32_t flags{};
};
// These helpers use a zero-based physical font record, not fonts.current.
// The caller4291F0 reloads current at its literal source boundaries.
double measure_text_4290d0(RetailFontResources &, std::span<const std::uint8_t>, float scale);
void begin_text_432300(TextSubmission, unsigned font, std::uint32_t flags, PolygonSubmissionSink &);
double submit_text_glyph_4323f0(TextSubmission, unsigned font, std::uint8_t character, TextGlyphParameters,
                                PolygonSubmissionSink &);
void end_text_432380(TextSubmission, PolygonSubmissionSink &);
// Full4291F0. alignment1=center,2=right,other=left. Text ends at NUL/newline
// or source clipping; an unaccessed suffix need not contain a terminator.
// Glyph vertices are LOCAL4x40B. They do not mutate global58D818/source35quad.
// Unwritten UV1 is zero initialized by explicit host policy, never retail data.
void submit_text_4291f0(TextSubmission, std::span<const std::uint8_t> text, std::uint32_t alignment,
                        TextGlyphParameters, PolygonSubmissionSink &);
} // namespace sgn::retail
