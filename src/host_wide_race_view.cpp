// NEW HOST presentation (R20 wide_3d): Hor+ race 3D view, see
// include/sgn/host_wide_race_view.hpp. Not recovered behaviour.
#include "sgn/host_wide_race_view.hpp"
#include "sgn/race_session.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>

namespace sgn::host_wide
{
namespace
{
thread_local RaceViewScope *active_scope = nullptr;
// NEW HOST (R23) guard band angle (radians, 0 = off). Written by the presenter
// between source passes, read at each 402E90 publication.
std::atomic<double> &guard_word() noexcept
{
    static std::atomic<double> value{0.0};
    return value;
}
float f32(double value) noexcept
{
    return static_cast<float>(value);
}
bool same(float a, float b) noexcept
{
    return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}
bool same(const retail::CameraVector &a, const retail::CameraVector &b) noexcept
{
    return same(a.x, b.x) && same(a.y, b.y) && same(a.z, b.z) && same(a.w, b.w);
}
} // namespace

std::array<retail::CameraVector, 4> widened_camera_planes(const retail::CameraState &s, float factor_x, float factor_y)
{
    // Identical to 402ECC planes() except the half extents: inputs
    // {-lens,0,-hw},{lens,0,-hw},{0,lens,-hh},{0,-lens,-hh}, 44ED40 normalize,
    // then the camera-to-world rotation with the source operand order.
    const float half_width = f32(double(s.half_width) * factor_x), half_height = f32(double(s.half_height) * factor_y);
    const std::array<retail::CameraVector, 4> inputs{{{-s.lens, 0, -half_width, 0},
                                                      {s.lens, 0, -half_width, 0},
                                                      {0, s.lens, -half_height, 0},
                                                      {0, -s.lens, -half_height, 0}}};
    const auto &m = s.camera_to_world_rotation;
    std::array<retail::CameraVector, 4> out{};
    for (std::size_t i = 0; i < 4; ++i)
    {
        auto v = inputs[i];
        retail::camera_normalize(v);
        auto &o = out[i];
        o.x = f32((double(v.z) * m[8] + double(v.y) * m[4]) + double(v.x) * m[0]);
        o.y = f32((double(v.x) * m[1] + double(v.z) * m[9]) + double(v.y) * m[5]);
        o.z = f32((double(v.x) * m[2] + double(v.z) * m[10]) + double(v.y) * m[6]);
    }
    return out;
}

RaceViewScope::RaceViewScope(retail::RaceSessionState &state) noexcept : state_(&state), previous_(active_scope)
{
    view_ = widened_view(current(), state.viewport);
    active_scope = this;
}
RaceViewScope::~RaceViewScope()
{
    restore();
    active_scope = previous_;
}
void RaceViewScope::published(retail::RaceSessionState &s) noexcept
{
    if (ended_ || &s != state_)
        return;
    // The fresh 402E90 publication overwrote any earlier widening of this view;
    // its saved words are obsolete (the source wrote new ones).
    saved_ = {};
    original_ = {};
    // 42E870 may apply a requested selector (424C30 via 42E870..42E87F) before
    // its 402E90 call; always derive the view from the live selected viewport.
    // Hor+ factors exactly as widened_view(Layout,viewport) (layout scale, or 1).
    const auto layout = current();
    const bool wide = layout.wide();
    float factor_x = wide ? layout.scale_x() : 1.0f, factor_y = wide ? layout.scale_y() : 1.0f;
    // NEW HOST (R23): interpolation guard band on top of Hor+, see the header.
    // The words just published by 402E90 are the original P.x/P.y, so the
    // visible half-extent tangents are k/P.x and k_y/P.y.
    if (const double guard = race_guard_band(); guard > 0.0 && guard_band_applicable(s))
    {
        const double px = std::abs(double(s.frame_projection[0])), py = std::abs(double(s.frame_projection[5]));
        if (std::isfinite(px) && std::isfinite(py) && px > 0.0 && py > 0.0)
        {
            factor_x = f32(double(factor_x) * guard_band_factor(double(factor_x) / px, guard));
            factor_y = f32(double(factor_y) * guard_band_factor(double(factor_y) / py, guard));
        }
    }
    view_ = widened_view(factor_x, factor_y, s.viewport);
    if (!active())
        return;
    const auto i = s.controls.current;
    if (i >= s.controls.controllers.size() || i >= s.camera_presentation.size())
        return;
    const auto &camera = s.controls.controllers[i].camera;
    auto &planes = s.camera_presentation[i].planes;
    std::array<retail::CameraVector, 4> widened;
    try
    {
        widened = widened_camera_planes(camera, view_.factor_x, view_.factor_y);
    }
    catch (...)
    {
        return;
    }
    Saved next;
    next.valid = true;
    next.controller = i;
    next.projection_x = s.frame_projection[0];
    next.projection_y = s.frame_projection[5];
    next.planes = planes;
    // 44F4E0 stores x=lens2/(2*half_width): P.x/k equals the projection of
    // half_width*k (one binary32 rounding of the quotient).
    next.written_x = view_.factor_x == 1.0f ? next.projection_x : f32(double(next.projection_x) / view_.factor_x);
    next.written_y = view_.factor_y == 1.0f ? next.projection_y : f32(double(next.projection_y) / view_.factor_y);
    next.written_planes = next.planes;
    if (view_.factor_x != 1.0f)
    {
        next.written_planes[0] = widened[0];
        next.written_planes[1] = widened[1];
    }
    if (view_.factor_y != 1.0f)
    {
        next.written_planes[2] = widened[2];
        next.written_planes[3] = widened[3];
    }
    // Keep each source W lane exactly as 402ECC left it (planes() never writes W).
    for (unsigned p = 0; p < 4; ++p)
        next.written_planes[p].w = next.planes[p].w;
    s.frame_projection[0] = next.written_x;
    s.frame_projection[5] = next.written_y;
    planes = next.written_planes;
    saved_ = next;
    original_ = {&saved_.planes, saved_.projection_x, saved_.projection_y};
}
void RaceViewScope::restore() noexcept
{
    if (ended_)
        return;
    ended_ = true;
    if (!saved_.valid || !state_)
        return;
    auto &s = *state_;
    const auto i = saved_.controller;
    // Restore only words that still hold our widened value; a later source
    // writer (none is reached in 4425D0) would keep its own value.
    if (same(s.frame_projection[0], saved_.written_x))
        s.frame_projection[0] = saved_.projection_x;
    if (same(s.frame_projection[5], saved_.written_y))
        s.frame_projection[5] = saved_.projection_y;
    if (i < s.camera_presentation.size())
    {
        auto &planes = s.camera_presentation[i].planes;
        for (unsigned p = 0; p < 4; ++p)
            if (same(planes[p], saved_.written_planes[p]))
                planes[p] = saved_.planes[p];
    }
    saved_ = {};
    original_ = {};
}
void race_camera_published(retail::RaceSessionState &s) noexcept
{
    if (active_scope)
        active_scope->published(s);
}
const WideView *race_view() noexcept
{
    return active_scope && active_scope->active() ? &active_scope->view() : nullptr;
}
const OriginalRaceCamera *race_original_camera() noexcept
{
    return active_scope ? active_scope->original() : nullptr;
}

// NEW HOST (R23) guard band.
void set_race_guard_band(double radians) noexcept
{
    guard_word().store(std::isfinite(radians) && radians > 0.0 ? std::min(radians, max_guard_radians) : 0.0,
                       std::memory_order_release);
}
double race_guard_band() noexcept
{
    return guard_word().load(std::memory_order_acquire);
}
float guard_band_factor(double tangent, double radians) noexcept
{
    if (!std::isfinite(tangent) || !(tangent > 0.0) || !std::isfinite(radians) || !(radians > 0.0))
        return 1.0f;
    const double angle = std::atan(tangent) + std::min(radians, max_guard_radians);
    // Beyond ~86 degrees the tangent explodes; the cap applies long before.
    if (!(angle < 1.5))
        return max_guard_factor;
    const double factor = std::tan(angle) / tangent;
    if (!std::isfinite(factor))
        return max_guard_factor;
    return f32(std::clamp(factor, 1.0, double(max_guard_factor)));
}
bool guard_band_applicable(const retail::RaceSessionState &s) noexcept
{
    // A guard band outside a smaller view rectangle (split screen, cropped or
    // selector!=1 views) would draw into the visible canvas around it, and a
    // partial 2D clip would be scaled beyond its own rectangle.
    const auto &v = s.viewport;
    return s.split_setting == 0 && v.selector == 1 && v.cropped == 0 && v.clip_virtual[0] == 0.0f &&
           v.clip_virtual[1] == 640.0f && v.clip_virtual[2] == 0.0f && v.clip_virtual[3] == 480.0f;
}
} // namespace sgn::host_wide
