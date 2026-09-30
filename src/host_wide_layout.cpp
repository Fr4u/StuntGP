// NEW HOST presentation (R20 widescreen, wide_layout/wide_3d). Not recovered
// game behaviour; see include/sgn/host_wide_layout.hpp for the shared model.
#include "sgn/host_wide_layout.hpp"
#include "sgn/retail_viewport.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cwctype>
#include <string>

namespace sgn::host_wide
{
namespace
{
constexpr float max_canvas = 8192.0f;
// Packed output size (width | height<<32); 0 = not yet published (640x480).
std::atomic<std::uint64_t> &output_word() noexcept
{
    static std::atomic<std::uint64_t> value{0};
    return value;
}
std::atomic<bool> &original_word() noexcept
{
    static std::atomic<bool> value{false};
    return value;
}
float f32(double value) noexcept
{
    return static_cast<float>(value);
}
bool finite(float value) noexcept
{
    return std::isfinite(value);
}
std::wstring lower(std::wstring_view text)
{
    std::wstring result(text);
    for (auto &c : result)
        c = static_cast<wchar_t>(std::towlower(static_cast<wint_t>(c)));
    return result;
}
bool token_present(std::wstring_view command_line, std::wstring_view token)
{
    const auto text = lower(command_line);
    for (std::size_t at = text.find(token); at != std::wstring::npos; at = text.find(token, at + 1))
    {
        const bool start = at == 0 || std::iswspace(static_cast<wint_t>(text[at - 1])) || text[at - 1] == L'"';
        const auto end = at + token.size();
        const bool finish = end == text.size() || std::iswspace(static_cast<wint_t>(text[end])) || text[end] == L'"';
        if (start && finish)
            return true;
    }
    return false;
}
bool environment_requests_4x3() noexcept
{
    wchar_t *value = nullptr;
    std::size_t length = 0;
    // _wdupenv_s keeps /W4 /WX clean (getenv is deprecated under MSVC).
    if (_wdupenv_s(&value, &length, L"SGN_ASPECT") != 0 || !value)
        return false;
    bool result = false;
    try
    {
        const auto text = lower(value);
        result = text == L"4x3" || text == L"4:3" || text == L"original";
    }
    catch (...)
    {
        result = false;
    }
    std::free(value);
    return result;
}
} // namespace

Layout compute_layout(std::uint32_t output_width, std::uint32_t output_height, bool original_4x3) noexcept
{
    Layout l;
    if (output_width == 0 || output_height == 0)
    {
        output_width = 640;
        output_height = 480;
    }
    l.output_width = output_width;
    l.output_height = output_height;
    l.original_4x3 = original_4x3;
    const double w = output_width, h = output_height;
    // Exact 4:3 outputs (integer test, no rounding) and the original mode keep
    // the 640x480 canvas and today's centred aspect fit.
    const auto wide_x = std::uint64_t(output_width) * 3u, tall_y = std::uint64_t(output_height) * 4u;
    if (!original_4x3 && wide_x != tall_y)
    {
        if (wide_x > tall_y)
            l.canvas_width = std::min(max_canvas, f32(480.0 * w / h));
        else
            l.canvas_height = std::min(max_canvas, f32(640.0 * h / w));
        l.extend_x = f32((double(l.canvas_width) - source_width) * 0.5);
        l.extend_y = f32((double(l.canvas_height) - source_height) * 0.5);
    }
    l.pixels_per_unit = f32(std::min(w / l.canvas_width, h / l.canvas_height));
    l.output_origin_x = f32((w - double(l.canvas_width) * l.pixels_per_unit) * 0.5);
    l.output_origin_y = f32((h - double(l.canvas_height) * l.pixels_per_unit) * 0.5);
    return l;
}
Layout current() noexcept
{
    const auto packed = output_word().load(std::memory_order_acquire);
    return compute_layout(static_cast<std::uint32_t>(packed), static_cast<std::uint32_t>(packed >> 32),
                          original_word().load(std::memory_order_acquire));
}
void set_output_size(std::uint32_t width, std::uint32_t height) noexcept
{
    if (!width || !height)
        return;
    output_word().store(std::uint64_t(width) | (std::uint64_t(height) << 32), std::memory_order_release);
}
bool original_4x3() noexcept
{
    return original_word().load(std::memory_order_acquire);
}
void set_original_4x3(bool value) noexcept
{
    original_word().store(value, std::memory_order_release);
}
bool configure(std::wstring_view command_line) noexcept
{
    bool requested = environment_requests_4x3();
    try
    {
        if (token_present(command_line, L"--aspect-4x3"))
            requested = true;
    }
    catch (...)
    {
    }
    if (requested)
        set_original_4x3(true);
    return original_4x3();
}

WideView widened_view(const Layout &layout, const retail::ViewportState &v) noexcept
{
    if (!layout.wide())
        return widened_view(1.0f, 1.0f, v);
    return widened_view(layout.scale_x(), layout.scale_y(), v);
}
WideView widened_view(float factor_x, float factor_y, const retail::ViewportState &v) noexcept
{
    WideView r;
    r.original = {v.center_x, v.center_y, v.scale_x, v.scale_y};
    r.widened = r.original;
    for (unsigned i = 0; i < 4; ++i)
    {
        r.clip_screen[i] = v.clip_screen[i];
        r.clip_expanded[i] = v.clip_expanded[i];
    }
    if ((factor_x == 1.0f && factor_y == 1.0f) || !finite(factor_x) || !finite(factor_y) || !(factor_x > 0.0f) ||
        !(factor_y > 0.0f) || v.full_width <= 0 || v.full_height <= 0)
        return r;
    if (!finite(v.center_x) || !finite(v.center_y) || !finite(v.scale_x) || !finite(v.scale_y) || !finite(v.origin_x) ||
        !finite(v.origin_y) || !(v.scale_x > 0.0f) || !(v.scale_y > 0.0f))
        return r;
    const double kx = factor_x, ky = factor_y;
    // Canvas centre = centre of the full 4:3 source frame (424C30 origin + full/2).
    const double cx = double(v.origin_x) + double(v.full_width) * 0.5,
                 cy = double(v.origin_y) + double(v.full_height) * 0.5;
    r.active = true;
    r.factor_x = f32(kx);
    r.factor_y = f32(ky);
    r.widened.center_x = f32((double(v.center_x) - cx) * kx + cx);
    r.widened.center_y = f32((double(v.center_y) - cy) * ky + cy);
    r.widened.half_width = f32(double(v.scale_x) * kx);
    r.widened.half_height = f32(double(v.scale_y) * ky);
    r.extend_left = std::max(0.0f, r.original.left() - r.widened.left());
    r.extend_right = std::max(0.0f, r.widened.right() - r.original.right());
    r.extend_top = std::max(0.0f, r.original.top() - r.widened.top());
    r.extend_bottom = std::max(0.0f, r.widened.bottom() - r.original.bottom());
    // Same arithmetic as 4250A0 (retail_viewport.cpp set_viewport_clip) with
    // the widened view scale/centre, applied to the live virtual clip.
    const auto x = [&](float value) {
        return f32(((double(value) - 320) * r.widened.half_width) * std::bit_cast<float>(0x3b4ccccdu) +
                   r.widened.center_x);
    };
    const auto y = [&](float value) {
        return f32(((double(value) - 240) * r.widened.half_height) * std::bit_cast<float>(0x3b888889u) +
                   r.widened.center_y);
    };
    r.clip_screen[0] = x(v.clip_virtual[0]);
    r.clip_screen[1] = x(v.clip_virtual[1]);
    r.clip_screen[2] = y(v.clip_virtual[2]);
    r.clip_screen[3] = y(v.clip_virtual[3]);
    r.clip_expanded[0] = f32(double(r.clip_screen[0]) - 100);
    r.clip_expanded[1] = f32(double(r.clip_screen[1]) + 100);
    r.clip_expanded[2] = f32(double(r.clip_screen[2]) - 100);
    r.clip_expanded[3] = f32(double(r.clip_screen[3]) + 100);
    return r;
}
} // namespace sgn::host_wide
