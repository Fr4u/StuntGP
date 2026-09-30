#pragma once
#include "sgn/retail_frontend_language.hpp"
#include "sgn/retail_frontend_font_metrics.hpp"

namespace sgn::retail
{
struct FrontendLanguageSelectionViews
{
    FrontendLanguageCatalogViews catalog;
    std::span<std::uint8_t, 0x404> scratch;                       // 5FF8C4, retained between calls
    std::span<std::uint8_t, frontend_language_arena_bytes> arena; // 5FFCC8
    // 606860,1000 descriptor pairs. Labels encode pinned ordinal+1;0=null.
    std::span<std::uint32_t, 2000> descriptors;
    std::uint32_t &remaining; // 47B1A0
};
struct FrontendLanguageSelectionResult
{
    std::int32_t column;      // original EAX; -1 means no catalog flag matched
    std::uint32_t cursor;     // new diagnostic of the original local cursor
    std::uint32_t unresolved; // new diagnostic of remaining descriptor count
};
// Complete440C40, including all441060/441110 assignments and4409C0 wraps.
// Uses the same source arena/table and actual font metrics. Undefined resource,
// buffer, missing wrap string and arena-overflow domains are rejected by a new
// read-only preflight. A matching flag requires descriptor731's sentinel=0,
// the defined cold/retained retail context; all other tail bytes are retained.
FrontendLanguageSelectionResult select_frontend_language_440c40(FrontendLanguageSelectionViews, FrontendLanguageInput,
                                                                FrontendFontMetricsViews, std::uint32_t flag);
// Full4409C0. The text must be a bounded mutable NUL span disjoint from scratch.
std::uint32_t wrap_frontend_text_4409c0(std::span<std::uint8_t> nul_text, std::span<std::uint8_t, 0x404> scratch,
                                        FrontendFontMetricsViews, std::int32_t width, float scale,
                                        std::int32_t maximum_lines);
} // namespace sgn::retail
