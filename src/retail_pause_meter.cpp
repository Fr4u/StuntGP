#include "sgn/retail_pause_meter.hpp"
#include "sgn/bit_reinterpret.hpp"
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
        throw FormatError("pause meter arithmetic outside finite binary32 domain");
    return static_cast<float>(value);
}
using sgn::bit_reinterpret::as_word;
using sgn::bit_reinterpret::compile_time::as_float;
} // namespace
void submit_pause_meter_446d90(PauseMeterSubmission meter, std::uint32_t percent, float x, float y, float width,
                               float height, PolygonSubmissionSink &sink)
{
    for (auto value : {x, y, width, height})
        if (!std::isfinite(value))
            throw FormatError("pause meter nonfinite input");
    if (meter.alpha_vertices.size() < 4)
        throw FormatError("pause meter requires four shared alpha vertices");
    const float w = stored(double(width) * as_float(0x3f4ccccdu));  // 446DAC
    const float h = stored(double(height) * as_float(0x3f4ccccdu)); // 446DC8
    const std::uint32_t remaining_percent = 100u - percent;         // 446DA8,zero-extended FILD
    const float remaining = stored((double(remaining_percent) * w) * as_float(0x3c23d70au)); // 446DE4
    const float filled = stored((double(percent) * w) * as_float(0x3c23d70au));              // 446DF3/DF6, no reload
    const float label_y = stored(double(y) - 4.0);
    const float label_x = stored(double(x) - 16.0);
    submit_text_4291f0(meter.text, meter.labels.read_left_4851e0(), 0,
                       {label_x, label_y, as_float(0x3f7ae148u), 0xffffffffu, 1.0f, 6}, sink); // 446E1C
    const float right = stored(double(x) + w); // 446E38, after first text
    submit_text_4291f0(meter.text, meter.labels.read_right_4851e4(), 0,
                       {right, label_y, as_float(0x3f7ae148u), 0xffffffffu, 1.0f, 6}, sink); // 446E45
    auto d = meter.descriptor;
    const auto quad = [&] {
        submit_quad_432e00({d, meter.alpha_vertices.first<4>(), meter.text.selected_material, meter.text.materials,
                            meter.text.viewport},
                           sink);
    };
    if (remaining > 0.0f)
    {
        // 22 stores in literal instruction order; d4..9/d12..18 are not ours.
        d[23] = as_word(remaining);
        d[27] = as_word(stored(double(remaining) - 11.0));
        d[32] = as_word(h);
        d[0] = 4;
        d[19] = 0;
        d[20] = 0;
        d[1] = as_word(stored(double(filled) + x));
        d[21] = 0x447c0000;
        d[22] = 0x44480000;
        d[24] = 0;
        d[25] = 0x447c0000;
        d[26] = 0x44580000;
        d[28] = as_word(h);
        d[29] = 0x447c0000;
        d[30] = 0x44580000;
        d[31] = 0xc1300000;
        d[33] = 0x447c0000;
        d[34] = 0x44480000;
        d[2] = as_word(y);
        d[3] = 0x3f7851ec;
        d[10] = 11;
        d[11] = 32;
        quad(); // 446F35
    }
    if (filled > 0.0f)
    {
        const std::uint32_t a = percent + 4u * percent;
        d[0] = 4; // 446F51
        const std::uint32_t b = a + 4u * a, c = b << 2;
        const auto high = static_cast<std::uint32_t>((std::uint64_t(0x51eb851fu) * c) >> 32);
        const std::uint32_t n = (high >> 5) + 16u;
        const float u = stored(double(n) * 4.0);
        d[28] = as_word(h);
        d[23] = as_word(filled);
        d[1] = as_word(x);
        d[19] = 0;
        d[20] = 0;
        d[21] = 0x42800000;
        d[22] = 0x442b0000;
        d[25] = as_word(u);
        d[24] = 0;
        d[26] = 0x442b0000;
        d[30] = 0x443a0000;
        d[31] = 0xc1300000;
        d[27] = as_word(stored(double(filled) - 11.0));
        d[32] = as_word(h);
        d[33] = 0x42800000;
        d[29] = as_word(u);
        d[34] = 0x443a0000;
        d[2] = as_word(y);
        d[3] = 0x3f7851ec;
        d[10] = 11;
        d[11] = 32;
        quad(); // 447047
    }
    const UiLineSubmission line_view{meter.clip, meter.alpha_vertices, meter.text.viewport, meter.text.specular_enabled,
                                     meter.text.selected_material};
    const auto line = [&](float x1, float y1, float x2, float y2) {
        submit_ui_line_447840(line_view, {{x1, y1, as_float(0x3f7ae148u)}, {x2, y2, as_float(0x3f7ae148u)}},
                              0xffffffffu, sink);
    };
    const float bottom = stored((double(h) + 1.0) + y);     // 44706E
    const float lower_right = stored(double(right) - 11.0); // 44707A
    line(x, y, stored(double(right) + 1.0), y);             // 447090
    line(right, y, lower_right, bottom);                    // 4470AF
    const float lower_left = stored(double(x) - 11.0);      // 4470C9, after line2
    line(lower_right, bottom, lower_left, bottom);          // 4470D7
    line(lower_left, bottom, x, y);                         // 4470F0
}
} // namespace sgn::retail
