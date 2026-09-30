#include "sgn/retail_race_status_text.hpp"
#include "sgn/retail_font_selection.hpp"
#include "sgn/host_wide_race_anchors.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
// Pinned immutable bytes at4826CC,4826D8,4826FC, including their NULs.
constexpr char finished[] = "GAMEOVER!";
constexpr char wait[] = "PLEASE WAIT FOR RACE TO FINISH...";
constexpr char unavailable[] = "NO PLAYER AVAILABLE...";
template <std::size_t N> std::span<const std::uint8_t> text(const char (&value)[N])
{
    return {reinterpret_cast<const std::uint8_t *>(value), N};
}
TextGlyphParameters parameters(std::uint32_t y)
{
    return {std::bit_cast<float>(0x43a00000u), std::bit_cast<float>(y),
            std::bit_cast<float>(0x3f7ae148u), 0x00ffffffu,
            std::bit_cast<float>(0x40000000u), 0};
}
} // namespace
void draw_race_finished_status_442520(TextSubmission world, PolygonSubmissionSink &sink)
{
    host_wide_overlay::AnchorScope anchor(host_wide_overlay::race::center); // NEW HOST (wide_hud R20), x=320
    select_retail_font_428f70(world.fonts, 1);
    submit_text_4291f0(world, text(finished), 1, parameters(0x43500000u), sink);
    submit_text_4291f0(world, text(wait), 1, parameters(0x43880000u), sink);
    select_retail_font_428f70(world.fonts, 0);
}
void draw_no_player_status_442590(TextSubmission world, PolygonSubmissionSink &sink)
{
    host_wide_overlay::AnchorScope anchor(host_wide_overlay::race::center); // NEW HOST (wide_hud R20), x=320
    select_retail_font_428f70(world.fonts, 1);
    submit_text_4291f0(world, text(unavailable), 1, parameters(0x43700000u), sink);
    select_retail_font_428f70(world.fonts, 0);
}
} // namespace sgn::retail
