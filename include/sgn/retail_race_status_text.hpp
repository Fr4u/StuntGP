#pragma once
#include "sgn/retail_text_submission.hpp"

namespace sgn::retail
{
// Full small transition-screen parents called by4425D0's non-world branches.
// They select font1, submit the original fixed strings, then select font0.
// Text glyphs are local packets; these parents do not reset the shared cache
// or draw a world, frame border, loading overlay or score popup.
void draw_race_finished_status_442520(TextSubmission, PolygonSubmissionSink &);
void draw_no_player_status_442590(TextSubmission, PolygonSubmissionSink &);
} // namespace sgn::retail
