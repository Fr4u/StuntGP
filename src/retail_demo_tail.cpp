#include "sgn/retail_demo_tail.hpp"
#include "sgn/race_session_lifecycle.hpp"
#include "sgn/retail_font_selection.hpp"
#include "sgn/host_wide_race_anchors.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>
#include <limits>

namespace sgn::retail
{
namespace
{
// Pinned PE4a1d234c...:46E470=42400000,46E46C=42A00000,
// 46E474=43E00000,46E1D4=43A00000. Verified in instructions.csv/input-pins.json.
using sgn::bit_reinterpret::as_float;
double finite(float value)
{
    if (!std::isfinite(value))
        throw FormatError("demo nonfinite source float outside candidate domain");
    return double(value);
}
float store(double value)
{
    // Guard BEFORE narrowing: out-of-range C++ floating conversion is not an
    // implementation of original FSTP overflow. No source rollback on failure.
    if (!std::isfinite(value) || std::abs(value) > double(std::numeric_limits<float>::max()))
        throw FormatError("demo float store outside candidate domain");
    return static_cast<float>(value);
}
std::uint32_t word(double value)
{
    return std::bit_cast<std::uint32_t>(store(value));
}
std::span<const std::uint8_t> text_bytes(const GameTextState &text, unsigned id)
{
    if (id >= text.offsets.size() || !text.offsets[id] || *text.offsets[id] >= text.arena.size())
        throw FormatError("demo game text pointer outside source arena");
    const auto offset = *text.offsets[id];
    // Preserve source4291F0's own NUL/newline/clipping traversal. Do not scan
    // an unaccessed suffix or substitute a translated/local string.
    return {reinterpret_cast<const std::uint8_t *>(text.arena.data() + offset), text.arena.size() - offset};
}
void quad(DemoTailWorld &w)
{
    auto drawing = w.drawing.borrow_quad();
    if (drawing.descriptor.data() != w.descriptor.data())
        throw FormatError("demo quad descriptor alias differs");
    submit_quad_432e00(drawing, w.sink);
}
} // namespace

void submit_demo_graphics_40de80(DemoTailWorld &w)
{
    // NEW HOST (wide_hud R20): logo quads follow HUD element0 (gauge corner).
    host_wide_overlay::AnchorScope anchor(host_wide_overlay::race::gauge);
    if (std::bit_cast<std::int32_t>(w.words.age) > 300)
        return; // 40DE80..40DE8A signed JG
    if (w.words.initialized == 0)
    {                                              // 40DE90..40DEC1
        w.words.initialized = 1;                   // 40DEA3 BEFORE upload; retain on width rejection/failure
        w.textures.replace_source_texture(11, 28); // 40DEAD, full430D40
        w.textures.replace_source_texture(14, 29); // 40DEBC, full430D40
        initialize_race_hud(w.hud);                // 40DEC1 tail448110, existing WHOLE child
        return;                                    // No two-quads fallthrough on the initialization invocation.
    }
    // 40DEC6 caches61B740. +2C/+30 alias element0's computed words, not its x/y.
    auto &position = hud_element(w.hud, w.hud.selected_hud, 0).layout;
    auto &d = w.descriptor;
    d[0] = 2;
    d[3] = 0x3f7851ec;                                                                  // 40DED0..40DEDA
    const double x = finite(as_float(position.word_1c)) - double(as_float(0x42400000)); // 40DEE4..40DEE7
    d[6] = 0;
    d[7] = 0;
    d[8] = 0x44800000;
    d[9] = 0x44800000; // 40DEED..40DF0B
    d[4] = 0x43480000;
    d[5] = 0x43480000; // 40DF15..40DF1F
    d[1] = word(x);    // 40DF29 FSTP, only here narrows to binary32
    const double y = finite(as_float(position.word_20)) - double(as_float(0x42a00000)); // 40DF2F..40DF32
    d[10] = 14;
    d[11] = 0x70;
    d[12] = 0xff;                            // 40DF38..40DF4C
    d[2] = word(y - finite(w.words.offset)); // 40DF56..40DF5C, no intermediate float store
    quad(w);                                 // 40DF62 ->432E00
    d[10] = 11;
    d[11] = 0x58; // 40DF67..40DF71, other descriptor words RETAINED
    quad(w);      // 40DF80 tail432E00
}

void submit_demo_text_40df90(DemoTailWorld &w)
{
    {
        auto drawing = w.drawing.borrow_text();
        select_retail_font_428f70(drawing.fonts, 1);
    } // 40DF91..40DF96
    const std::uint32_t remainder = w.demo_ticks % 60u; // 40DF9B..40DFA9, unsigned DIV
    const float bottom_y = store(double(as_float(0x43e00000)) - double(w.hud.margins.y)); // signed FILD then FSUBR/FSTP
    // 40DFBC..40DFCC, SBB/AND/ADD modulo32. Cache ESI across BOTH text calls.
    const std::uint32_t color = ((0u - std::uint32_t(remainder < 20u)) & 0xff404100u) + 0xffffff00u;
    const std::int32_t bottom_margin_x = w.hud.margins.x;                                 // 40DFDC FILD
    const auto bottom = text_bytes(w.game_text, 95);                                      // 40DFE3:61291C =6127A0+95*4
    const float bottom_x = store(double(bottom_margin_x) + double(as_float(0x43a00000))); // 40DFE9..40DFEF
    {
        host_wide_overlay::AnchorScope anchor(host_wide_overlay::race::bottom_center); // NEW HOST (wide_hud R20)
        auto drawing = w.drawing.borrow_text();
        submit_text_4291f0(drawing, bottom, 1,
                           {bottom_x, bottom_y, as_float(0x3f7d70a4), color, as_float(0x40000000), 6}, w.sink);
    } // 40DFF2
    // Read margins and text104 AFTER the first complete4291F0. Do not cache both
    // calls' parameters before it or restore font0 after either text call.
    const float top_y = store(double(w.hud.margins.y));                             // 40DFF7..40E010 signed FILD/FSTP
    const std::int32_t top_margin_x = w.hud.margins.x;                              // 40E013
    const auto top = text_bytes(w.game_text, 104);                                  // 40E01A:612940 =6127A0+104*4
    const float top_x = store(double(top_margin_x) + double(as_float(0x43a00000))); // 40E020..40E026
    {
        host_wide_overlay::AnchorScope anchor(host_wide_overlay::race::top_center); // NEW HOST (wide_hud R20)
        auto drawing = w.drawing.borrow_text();
        submit_text_4291f0(drawing, top, 1, {top_x, top_y, as_float(0x3f7d70a4), color, as_float(0x40000000), 6},
                           w.sink);
    } // 40E029;40E02E..40E02F only POP/RET, font selection retained
}

void submit_demo_tail_40e030(DemoTailWorld &w)
{
    if (w.hud_block != 1)
        return;                     // 40E030..40E037, also returns for2/FFFFFFFF
    submit_demo_graphics_40de80(w); // 40E039, its age/init return does NOT skip text
    submit_demo_text_40df90(w);     // 40E03E tail, no additional hud_block check
}
} // namespace sgn::retail
