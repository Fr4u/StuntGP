#include "sgn/retail_viewport.hpp"
#include "sgn/assets.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>
#include <limits>

namespace sgn::retail
{
namespace
{
float f32(double value)
{
    return static_cast<float>(value);
}
using sgn::bit_reinterpret::as_float;
void dimensions(std::int32_t width, std::int32_t height)
{
    if (width <= 0 || height <= 0)
        throw FormatError("viewport dimensions must be positive");
}
std::int32_t integer(double value)
{ // 462860 low DWORD of FISTP qword
    if (!std::isfinite(value) || value < -9223372036854775808.0 || value >= 9223372036854775808.0)
        throw FormatError("viewport integer outside source conversion range");
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int64_t>(value)));
}
} // namespace
void set_viewport_clip(ViewportState &s, float x0, float y0, float x1, float y1)
{
    s.clip_virtual = {x0, x1, y0, y1};
    const auto x = [&](float value) {
        return f32(((double(value) - 320) * s.scale_x) * as_float(0x3b4ccccd) + s.center_x);
    };
    const auto y = [&](float value) {
        return f32(((double(value) - 240) * s.scale_y) * as_float(0x3b888889) + s.center_y);
    };
    s.clip_screen = {x(x0), x(x1), y(y0), y(y1)};
    s.clip_expanded = {f32(double(s.clip_screen[0]) - 100), f32(double(s.clip_screen[1]) + 100),
                       f32(double(s.clip_screen[2]) - 100), f32(double(s.clip_screen[3]) + 100)};
}
void initialize_viewport(ViewportState &s, std::uint32_t &split_setting)
{
    dimensions(s.width, s.height);
    const auto hx = s.width / 2, hy = s.height / 2;
    s.scale_x = f32(double(hx) + 0.5);
    s.scale_y = f32(double(hy) + 0.5);
    s.center_pixel_x = hx;
    s.center_pixel_y = hy;
    s.min_x = -hx;
    s.min_y = -hy;
    s.max_x = hx;
    s.max_y = hy;
    s.right_edge = f32(double(s.width) - 0.5);
    s.bottom_edge = f32(double(s.height) - 0.5);
    s.cropped = 0;
    s.selector = 1;
    split_setting = 0;
    s.full_width = s.width;
    s.full_height = s.height;
    s.bytes_per_pixel = s.depth_bits / 8;
    s.applied_selector = 0;
    s.requested_selector = 1;
    s.center_x = f32(double(s.scale_x) - 0.5);
    s.center_y = f32(double(s.scale_y) - 0.5);
    set_viewport_clip(s, 0, 0, 640, 480);
}
void update_viewport_ui_scale(ViewportState &s)
{
    s.ui_scale_x = f32(double(s.width) * as_float(0x3acccccd));
    s.ui_scale_y = f32(double(s.height) * as_float(0x3b088889));
}
void select_viewport(ViewportState &s, std::int32_t selector)
{
    // Nonpositive selectors read uninitialized stack locals in the executable;
    // reject that undefined path before any mutation.17..INT_MAX selects1.
    if (selector <= 0)
        throw FormatError("viewport selector has undefined original locals");
    dimensions(s.full_width, s.full_height);
    if (!std::isfinite(s.origin_x) || !std::isfinite(s.origin_y))
        throw FormatError("nonfinite viewport origin");
    if (selector > 16)
        selector = 1;
    float width{}, height{}, offset_x{}, offset_y{};
    if (selector <= 5)
    {
        const double scale = selector == 1 || selector == 2 ? 1 : selector == 3 ? 0.75 : selector == 4 ? 0.5 : 0.25;
        width = f32(double(s.full_width) * scale);
        const float stored_height = f32(double(s.full_height));
        const double scaled_height = double(s.full_height) * (selector == 2 ? 0.5625 : scale);
        height = f32(scaled_height);
        if (selector != 1)
        {
            // Source keeps FILD width, but subtracts the stored scaled height.
            offset_x = f32((double(s.full_width) - width) * 0.5);
            offset_y = f32((double(stored_height) - height) * 0.5);
        }
    }
    else
    {
        width = f32(double(s.full_width / 2));
        height = f32(double(s.full_height / 2));
        const double fw = s.full_width, fh = s.full_height;
        if (selector == 6 || selector == 7 || selector == 10)
            offset_x = f32((fw - width) * 0.5);
        if (selector == 7)
            offset_y = height;
        if (selector == 8 || selector == 9)
            offset_y = f32((fh - height) * 0.5);
        if (selector == 9 || selector == 12 || selector == 14 || selector == 16)
            offset_x = width;
        if (selector == 11 || selector == 12 || selector == 15 || selector == 16)
            offset_y = f32(fh - height);
    }
    s.cropped = selector == 1 ? 0 : 1;
    s.width = integer(width);
    s.height = integer(height);
    update_viewport_ui_scale(s);
    const double half_width = double(width) * 0.5, half_height = double(height) * 0.5;
    const float stored_half_height = f32(half_height);
    s.scale_x = f32(half_width);
    s.scale_y = f32(half_height);
    const double cx = (double(s.origin_x) + offset_x) + half_width;
    const float cy = f32((double(s.origin_y) + offset_y) + stored_half_height);
    s.center_pixel_x = integer(cx);
    s.center_pixel_y = integer(cy);
    s.center_x = f32(cx);
    s.center_y = cy;
    s.selector = static_cast<std::uint32_t>(selector);
    s.applied_selector = s.selector;
    s.requested_selector = s.selector;
    set_viewport_clip(s, 0, 0, 640, 480);
}
void apply_requested_viewport(ViewportState &s)
{
    if (s.applied_selector != s.requested_selector)
        select_viewport(s, std::bit_cast<std::int32_t>(s.requested_selector));
}
} // namespace sgn::retail
