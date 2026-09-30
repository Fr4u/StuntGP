#include "sgn/host_wide_overlay.hpp"
#include "sgn/retail_viewport.hpp"
#include <cmath>

// NEW HOST presentation (wide_hud, R20). See host_wide_overlay.hpp.
namespace sgn::host_wide_overlay
{
namespace
{
struct State
{
    bool enabled{true};
    Anchor anchor{};
};
// Owner (draw) thread only, like host_presentation; thread_local keeps any
// foreign thread at the original mapping.
thread_local State state;

OverlayMapping identity(const retail::ViewportState &v) noexcept
{
    return {v.ui_scale_x, v.ui_scale_y, v.center_x, v.center_y, v.clip_virtual, false};
}
bool finite(double value) noexcept
{
    return std::isfinite(value);
}
bool usable(const retail::ViewportState &v) noexcept
{
    // Nonfinite/degenerate inputs stay with the original arithmetic and its
    // existing source checks; the host mapping never hides them.
    if (!finite(v.ui_scale_x) || !finite(v.ui_scale_y) || !finite(v.center_x) || !finite(v.center_y))
        return false;
    for (const auto c : v.clip_virtual)
        if (!finite(c))
            return false;
    return v.ui_scale_x > 0.0f && v.ui_scale_y > 0.0f;
}
// Active widened view of v with finite, nonnegative per-side growth.
bool widened(const retail::ViewportState &v, host_wide::WideView &out) noexcept
{
    if (!state.enabled || !usable(v))
        return false;
    out = host_wide::widened_view(v);
    if (!out.active)
        return false;
    for (const double e :
         {double(out.extend_left), double(out.extend_right), double(out.extend_top), double(out.extend_bottom)})
        if (!finite(e) || e < 0.0)
            return false;
    return finite(out.original.half_width) && finite(out.original.half_height) && out.original.half_width > 0.0f &&
           out.original.half_height > 0.0f;
}
double offset(Horizontal a, const host_wide::WideView &w) noexcept
{
    return a == Horizontal::left ? -double(w.extend_left) : a == Horizontal::right ? double(w.extend_right) : 0.0;
}
double offset(Vertical a, const host_wide::WideView &w) noexcept
{
    return a == Vertical::top ? -double(w.extend_top) : a == Vertical::bottom ? double(w.extend_bottom) : 0.0;
}
} // namespace

void set_enabled(bool value) noexcept
{
    state.enabled = value;
}
bool enabled() noexcept
{
    return state.enabled;
}
Anchor current_anchor() noexcept
{
    return state.anchor;
}

AnchorScope::AnchorScope(Anchor anchor) noexcept : previous_(state.anchor)
{
    state.anchor = anchor;
}
AnchorScope::~AnchorScope()
{
    state.anchor = previous_;
}

OverlayMapping overlay_mapping(const retail::ViewportState &v) noexcept
{
    auto m = identity(v);
    const auto anchor = state.anchor;
    if (anchor.mode == Anchor::Mode::original)
        return m;
    host_wide::WideView w;
    if (!widened(v, w))
        return m;
    const double sx = v.ui_scale_x, sy = v.ui_scale_y, cx = v.center_x, cy = v.center_y;
    const double el = w.extend_left, er = w.extend_right, et = w.extend_top, eb = w.extend_bottom;
    if (anchor.mode == Anchor::Mode::fill)
    {
        // Logical clip frame unchanged; it now spans the widened view.
        const double hw = w.original.half_width, hh = w.original.half_height;
        m.scale_x = static_cast<float>(sx * (hw + (el + er) * 0.5) / hw);
        m.scale_y = static_cast<float>(sy * (hh + (et + eb) * 0.5) / hh);
        m.center_x = static_cast<float>(cx + (er - el) * 0.5);
        m.center_y = static_cast<float>(cy + (eb - et) * 0.5);
        m.anchored = true;
        return m;
    }
    // Edge anchor: same ui_scale (no stretch), center shifted with the edge.
    const double ax = cx + offset(anchor.horizontal, w), ay = cy + offset(anchor.vertical, w);
    m.center_x = static_cast<float>(ax);
    m.center_y = static_cast<float>(ay);
    // Original logical clip edges on screen, grown by the view's per-side
    // extension, mapped back through the anchored mapping.
    const auto &c = v.clip_virtual;
    const double x0 = (double(c[0]) - 320.0) * sx + cx - el, x1 = (double(c[1]) - 320.0) * sx + cx + er;
    const double y0 = (double(c[2]) - 240.0) * sy + cy - et, y1 = (double(c[3]) - 240.0) * sy + cy + eb;
    m.clip = {static_cast<float>((x0 - ax) / sx + 320.0), static_cast<float>((x1 - ax) / sx + 320.0),
              static_cast<float>((y0 - ay) / sy + 240.0), static_cast<float>((y1 - ay) / sy + 240.0)};
    m.anchored = true;
    return m;
}

double side_extension(const retail::ViewportState &v, Horizontal side) noexcept
{
    // NEW HOST (widescreen R37), see the header.
    if (side == Horizontal::center)
        return 0.0;
    host_wide::WideView w;
    if (!widened(v, w))
        return 0.0;
    const double grow = side == Horizontal::left ? double(w.extend_left) : double(w.extend_right);
    const double scale = v.ui_scale_x;
    const double units = grow / scale;
    return finite(units) && units > 0.0 ? units : 0.0;
}

ViewFrame view_frame(const retail::ViewportState &v) noexcept
{
    ViewFrame f{double(v.width), double(v.height), v.center_x, v.center_y};
    host_wide::WideView w;
    if (!widened(v, w))
        return f;
    const auto &r = w.widened;
    if (!finite(r.half_width) || !finite(r.half_height) || !finite(r.center_x) || !finite(r.center_y) ||
        !(r.half_width > 0.0f) || !(r.half_height > 0.0f))
        return f;
    return {double(r.half_width) * 2.0, double(r.half_height) * 2.0, r.center_x, r.center_y};
}
} // namespace sgn::host_wide_overlay
