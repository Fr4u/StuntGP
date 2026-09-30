#include "sgn/retail_ui_line.hpp"
#include "sgn/host_wide_overlay.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/format_require.hpp"
#include <bit>
#include <cmath>
#include <limits>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_word;
using sgn::format_check::require;
void finite(float value)
{
    require(std::isfinite(value), "UI line nonfinite accessed arithmetic");
}
float stored(double value)
{
    require(std::isfinite(value) && value <= std::numeric_limits<float>::max() &&
                value >= -std::numeric_limits<float>::max(),
            "UI line result outside finite binary32 domain");
    return static_cast<float>(value);
}
void endpoints(const UiLineEndpoints &line)
{
    for (auto x : line.first)
        finite(x);
    for (auto x : line.second)
        finite(x);
}
void advance(UiLineClipScratch &s, float fraction, bool first)
{
    finite(fraction);
    for (unsigned i = 3; i < 9; ++i)
        finite(as_float(s.words[i]));
    for (unsigned i = 0; i < 3; ++i)
        s.words[i] = as_word(stored(double(as_float(s.words[3 + i])) - as_float(s.words[6 + i])));
    // 431ADC/431B7C X is kept on x87 while Y/Z products are stored.
    const double x = double(as_float(s.words[0])) * fraction;
    const float y = stored(double(as_float(s.words[1])) * fraction),
                z = stored(double(as_float(s.words[2])) * fraction);
    const unsigned base = first ? 6u : 3u;
    s.words[base] = as_word(stored(first ? double(as_float(s.words[base])) + x : double(as_float(s.words[base])) - x));
    s.words[base + 1] =
        as_word(stored(first ? double(as_float(s.words[base + 1])) + y : double(as_float(s.words[base + 1])) - y));
    s.words[base + 2] =
        as_word(stored(first ? double(as_float(s.words[base + 2])) + z : double(as_float(s.words[base + 2])) - z));
}
bool clip(UiLineEndpoints &line, UiLineClipScratch &s, const ViewportState &v)
{
    endpoints(line);
    finite(v.right_edge);
    finite(v.bottom_edge);
    const float right = stored(double(v.right_edge) - 1.0), bottom = stored(double(v.bottom_edge) - 1.0);
    for (unsigned i = 0; i < 3; ++i)
    {
        s.words[3 + i] = as_word(line.second[i]);
        s.words[6 + i] = as_word(line.first[i]);
    }
    const auto first = [&](unsigned axis) { return as_float(s.words[6 + axis]); };
    const auto second = [&](unsigned axis) { return as_float(s.words[3 + axis]); };
    // 431C31..431CFD: four ordered trivial rejections, strictly outside.
    if (first(0) < 1.0f && second(0) < 1.0f)
        return false;
    if (first(0) > right && second(0) > right)
        return false;
    if (first(1) < 1.0f && second(1) < 1.0f)
        return false;
    if (first(1) > bottom && second(1) > bottom)
        return false;
    const auto adjust = [&](unsigned axis, float boundary, bool lower, bool target_first) {
        const float value = target_first ? first(axis) : second(axis);
        if (lower ? value < boundary : value > boundary)
        {
            const double numerator = lower ? double(boundary) - value : double(value) - boundary;
            const double denominator = std::abs(double(second(axis)) - first(axis));
            require(denominator != 0.0, "UI line zero clipping divisor");
            advance(s, stored(numerator / denominator), target_first);
        }
    };
    // Source changes endpoints after every helper. Do not cache deltas or use
    // a replacement clip algorithm; later decisions consume the rounded writes.
    adjust(0, 1.0f, true, true);
    adjust(0, 1.0f, true, false);
    adjust(0, right, false, true);
    adjust(0, right, false, false);
    adjust(1, 1.0f, true, true);
    adjust(1, 1.0f, true, false);
    adjust(1, bottom, false, true);
    adjust(1, bottom, false, false);
    for (unsigned i = 0; i < 3; ++i)
    {
        line.first[i] = as_float(s.words[6 + i]);
        line.second[i] = as_float(s.words[3 + i]);
    }
    return true;
}
struct Region
{
    std::uintptr_t at;
    std::size_t size;
};
void disjoint(std::span<const Region> regions)
{
    for (const auto &r : regions)
        require(r.at <= std::numeric_limits<std::uintptr_t>::max() - r.size, "UI line owner extent overflow");
    for (std::size_t a = 0; a < regions.size(); ++a)
        for (std::size_t b = 0; b < a; ++b)
            require(regions[a].at + regions[a].size <= regions[b].at ||
                        regions[b].at + regions[b].size <= regions[a].at,
                    "UI line overlapping source owners");
}
void submit(UiLineSubmission frame, UiLineEndpoints line, std::uint32_t color, PolygonSubmissionSink &sink)
{
    UiLineClipScratch next = frame.clip;
    const bool visible = clip(line, next, frame.viewport);
    const std::array<Region, 4> owners = {{{reinterpret_cast<std::uintptr_t>(&frame.clip), sizeof frame.clip},
                                           {reinterpret_cast<std::uintptr_t>(&frame.viewport), sizeof frame.viewport},
                                           {reinterpret_cast<std::uintptr_t>(&frame.specular_enabled), 4},
                                           {reinterpret_cast<std::uintptr_t>(&frame.selected_material), 4}}};
    disjoint(owners);
    if (visible)
    {
        require(frame.alpha_vertices.size() >= 2, "UI line draw needs two shared alpha vertices");
        std::array<Region, 5> all{};
        for (unsigned i = 0; i < 4; ++i)
            all[i] = owners[i];
        all[4] = {reinterpret_cast<std::uintptr_t>(frame.alpha_vertices.data()), 2 * sizeof(PolygonSubmissionVertex)};
        disjoint(all);
    }
    frame.clip = next;
    if (!visible)
        return;
    for (unsigned i = 0; i < 2; ++i)
    {
        auto &vertex = frame.alpha_vertices[i].words;
        const auto &p = i ? line.second : line.first;
        vertex[0] = as_word(p[0]);
        vertex[1] = as_word(p[1]);
        vertex[2] = vertex[3] = as_word(p[2]);
        vertex[4] = color;
        vertex[5] = 0xff000000u;
    }
    sink.set_render_state(24, 1);
    sink.set_texture(0, 0);
    sink.draw(2, 0x2c4, frame.alpha_vertices.first(2), 0x14, frame.viewport);
    sink.set_render_state(24, 0xa0);
    // 431FE7 return: deliberately no cache invalidation or specular-owner write.
}
} // namespace
void advance_ui_line_first_431aa0(UiLineClipScratch &s, float fraction)
{
    auto next = s;
    advance(next, fraction, true);
    s = next;
}
void advance_ui_line_second_431b40(UiLineClipScratch &s, float fraction)
{
    auto next = s;
    advance(next, fraction, false);
    s = next;
}
bool clip_ui_line_431be0(UiLineEndpoints &line, UiLineClipScratch &s, const ViewportState &v)
{
    const std::array<Region, 3> owners = {{{reinterpret_cast<std::uintptr_t>(&line), sizeof line},
                                           {reinterpret_cast<std::uintptr_t>(&s), sizeof s},
                                           {reinterpret_cast<std::uintptr_t>(&v), sizeof v}}};
    disjoint(owners);
    auto next = s;
    auto next_line = line;
    const bool visible = clip(next_line, next, v);
    s = next;
    if (visible)
        line = next_line;
    return visible;
}
void submit_ui_line_431f10(UiLineSubmission f, UiLineEndpoints line, std::uint32_t color, PolygonSubmissionSink &sink)
{
    submit(f, line, color, sink);
}
void submit_ui_line_447840(UiLineSubmission f, UiLineEndpoints line, std::uint32_t color, PolygonSubmissionSink &sink)
{
    endpoints(line);
    finite(f.viewport.ui_scale_x);
    finite(f.viewport.ui_scale_y);
    finite(f.viewport.center_x);
    finite(f.viewport.center_y);
    // NEW HOST (wide_hud R20): anchored mapping; identity outside a scope or on 4:3.
    const auto map = host_wide_overlay::overlay_mapping(f.viewport);
    // 447843..4478AC stores transformed second Y/X, then first Y/X. Z unchanged.
    line.second[1] = stored((double(line.second[1]) - 240.0) * map.scale_y + map.center_y);
    line.second[0] = stored((double(line.second[0]) - 320.0) * map.scale_x + map.center_x);
    line.first[1] = stored((double(line.first[1]) - 240.0) * map.scale_y + map.center_y);
    line.first[0] = stored((double(line.first[0]) - 320.0) * map.scale_x + map.center_x);
    submit(f, line, color, sink);
}
} // namespace sgn::retail
