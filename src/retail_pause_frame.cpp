#include "sgn/retail_pause_frame.hpp"
#include <bit>
#include <cmath>
#include <limits>

namespace sgn::retail
{
namespace
{
float stored(double value)
{
    if (!std::isfinite(value) || value > std::numeric_limits<float>::max() ||
        value < -std::numeric_limits<float>::max())
        throw FormatError("pause frame arithmetic outside finite binary32 domain");
    return static_cast<float>(value);
}
} // namespace
void submit_pause_frame_447280(UiLineSubmission frame, float x, float y, float width, float height,
                               PolygonSubmissionSink &sink)
{
    for (const auto value : {x, y, width, height})
        if (!std::isfinite(value))
            throw FormatError("pause frame nonfinite input");
    constexpr float depth = std::bit_cast<float>(0x3f7d70a4u);
    const auto line = [&](float x1, float y1, float x2, float y2, std::uint32_t color) {
        submit_ui_line_447840(frame, {{x1, y1, depth}, {x2, y2, depth}}, color, sink);
    };
    const float right = stored(double(x) + width);              // 447298
    const float bottom = stored((double(y) + height) + 1.0);    // 4472A9
    const float left = stored(double(x) - 1.0);                 // 4472B5
    line(left, y, stored(double(right) + 1.0), y, 0xffffffffu); // 4472CE
    line(right, y, right, bottom, 0xffffffffu);                 // 4472EA
    line(right, bottom, x, bottom, 0xffffffffu);                // 447303
    line(x, bottom, x, y, 0xffffffffu);                         // 44731C
    const float top = stored(double(y) - 1.0);                  // 44733A
    const float right2 = stored((double(right) - 1.0) + 1.0);   // 447350
    const float bottom2 = stored((double(bottom) - 1.0) + 1.0); // 447362
    const float inner_right = stored(double(right2) - 1.0);     // 44736E
    line(left, top, inner_right, top, 0xffc0c0c0u);             // 44737E
    const float inner_bottom = stored(double(bottom2) - 1.0);   // 447399
    line(right2, top, right2, inner_bottom, 0xffc0c0c0u);       // 4473A8
    line(inner_right, bottom2, left, bottom2, 0xffc0c0c0u);     // 4473C3
    line(left, inner_bottom, left, top, 0xffc0c0c0u);           // 4473DE
}
} // namespace sgn::retail
