#pragma once
#include "sgn/retail_race_hud.hpp"
#include "sgn/retail_game_text.hpp"
#include "sgn/retail_quad_submission.hpp"
#include "sgn/retail_text_submission.hpp"

namespace sgn::retail
{
struct RaceTextureReplacement;
// Borrow the persistent words written by40DD80/40DDD0. These are NOT612990,
// a monitor clock, or level.overlay_demo. This view supplies no initial values.
struct DemoTailWords
{
    const std::uint32_t &age;   // 473A24; source40DE80 compares SIGNED against300
    const float &offset;        // 4D5B18; source binary32 offset, updated outside drawing
    std::uint32_t &initialized; // 4D5B1C; set before both actual uploads
};
struct DemoTailDrawingSources
{
    virtual ~DemoTailDrawingSources() = default;
    // Pure fresh borrows from the same Session/material/scratch owners.
    // No allocation, source writes, upload, font selection or scene dispatch.
    virtual TextSubmission borrow_text() = 0;
    virtual QuadSubmission borrow_quad() = 0;
};
struct DemoTailWorld
{
    const std::uint32_t &hud_block;  // 61299C; exact equality1, not nonzero
    const std::uint32_t &demo_ticks; // 612990; unsigned DIV60 color input only
    DemoTailWords words;
    RaceHudView hud; // existing selected HUD, countdown/layouts/notices/margins
    const GameTextState &game_text;
    RaceTextureReplacement &textures;        // actual source28 ->slot11,29 ->slot14
    std::span<std::uint32_t, 35> descriptor; // same retained576C38 as other UI
    DemoTailDrawingSources &drawing;
    PolygonSubmissionSink &sink;
};
// Recovered source bodies. Supported arithmetic domain: finite
// binary32 inputs and finite stored outputs, PC53/nearest source convention.
// No source tick, inferred resource fallback, upload suppression or font reset.
void submit_demo_graphics_40de80(DemoTailWorld &);
void submit_demo_text_40df90(DemoTailWorld &);
void submit_demo_tail_40e030(DemoTailWorld &);
} // namespace sgn::retail
