#pragma once
#include "sgn/retail_ui_line.hpp"

namespace sgn::retail
{
// Full447280. Four source stack values x/y/width/height, eight ordered447840
// calls, no additional persistent owner. Each call borrows the SAME live line
// scratch/alpha/viewport/cache/specular/sink. Shared descriptors/fonts untouched.
// Finite arithmetic and the borrowed line owner's existing domain are required.
// Host guards do not roll back earlier completed source calls.
void submit_pause_frame_447280(UiLineSubmission, float x, float y, float width, float height, PolygonSubmissionSink &);
} // namespace sgn::retail
