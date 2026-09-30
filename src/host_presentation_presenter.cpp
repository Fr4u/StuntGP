#include "sgn/host_presentation_presenter.hpp"
#include "sgn/host_frame_phase.hpp"
#include "sgn/host_frame_trace.hpp"
#include "sgn/host_graphics_settings.hpp"
#include "sgn/host_profiler.hpp"
#include "sgn/polygon_submission.hpp"
#include "sgn/polygon_device_context.hpp"
#include "sgn/polygon_command_capture.hpp"
#include "sgn/polygon_reprojection.hpp"
#include "sgn/renderer.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session.hpp"
#include "sgn/host_wide_layout.hpp"
#include "sgn/host_wide_race_view.hpp"
#include "sgn/source_host_counter.hpp"
#include "sgn/source_menu_presentation_backend.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <windows.h>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string_view>

// NEW HOST high-refresh presentation (interp, R19); see the header. Plain math on
// snapshots and on copies of the canonical capture. Nothing here is a source owner.
namespace sgn::host_presentation
{
namespace
{
using V3 = std::array<double, 3>;
struct M3
{
    std::array<double, 9> m{1, 0, 0, 0, 1, 0, 0, 0, 1};
}; // row-major, row-vector convention v' = v*M
V3 add(V3 a, V3 b)
{
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}
V3 sub(V3 a, V3 b)
{
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
V3 scale(V3 a, double s)
{
    return {a[0] * s, a[1] * s, a[2] * s};
}
V3 lerp(V3 a, V3 b, double t)
{
    return {a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t};
}
double dot(V3 a, V3 b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
V3 cross(V3 a, V3 b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double length(V3 a)
{
    return std::sqrt(dot(a, a));
}
bool normalize(V3 &a)
{
    const double l = length(a);
    if (!(l > 1e-12) || !std::isfinite(l))
        return false;
    a = scale(a, 1.0 / l);
    return true;
}
V3 mul(V3 v, const M3 &m)
{ // v*M
    const auto &x = m.m;
    return {v[0] * x[0] + v[1] * x[3] + v[2] * x[6], v[0] * x[1] + v[1] * x[4] + v[2] * x[7],
            v[0] * x[2] + v[1] * x[5] + v[2] * x[8]};
}
M3 mul(const M3 &a, const M3 &b)
{
    M3 r;
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j)
            r.m[i * 3 + j] = a.m[i * 3] * b.m[j] + a.m[i * 3 + 1] * b.m[3 + j] + a.m[i * 3 + 2] * b.m[6 + j];
    return r;
}
M3 transpose(const M3 &a)
{
    M3 r;
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j)
            r.m[i * 3 + j] = a.m[j * 3 + i];
    return r;
}
double determinant(const M3 &a)
{
    const auto &m = a.m;
    return m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) + m[2] * (m[3] * m[7] - m[4] * m[6]);
}
std::optional<M3> inverse(const M3 &a)
{
    const auto &m = a.m;
    const double d = determinant(a);
    if (!(std::abs(d) > 1e-12) || !std::isfinite(d))
        return std::nullopt;
    M3 r;
    const double k = 1.0 / d;
    r.m = {(m[4] * m[8] - m[5] * m[7]) * k, (m[2] * m[7] - m[1] * m[8]) * k, (m[1] * m[5] - m[2] * m[4]) * k,
           (m[5] * m[6] - m[3] * m[8]) * k, (m[0] * m[8] - m[2] * m[6]) * k, (m[2] * m[3] - m[0] * m[5]) * k,
           (m[3] * m[7] - m[4] * m[6]) * k, (m[1] * m[6] - m[0] * m[7]) * k, (m[0] * m[4] - m[1] * m[3]) * k};
    return r;
}
// Proper rotation check (orthonormal, det +1) within float tolerance.
bool rotation(const M3 &a)
{
    const auto p = mul(a, transpose(a));
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j)
            if (std::abs(p.m[i * 3 + j] - (i == j ? 1.0 : 0.0)) > 1e-3)
                return false;
    return determinant(a) > 0;
}
struct Q
{
    double w{1}, x{}, y{}, z{};
};
// Row-vector matrix M equals the column-vector rotation R^T; the conversion
// pair below is mutually inverse, so the convention cancels.
Q quaternion(const M3 &a)
{
    const auto &m = a.m;
    Q q;
    const double trace = m[0] + m[4] + m[8];
    if (trace > 0)
    {
        const double s = std::sqrt(trace + 1.0) * 2;
        q = {0.25 * s, (m[5] - m[7]) / s, (m[6] - m[2]) / s, (m[1] - m[3]) / s};
    }
    else if (m[0] > m[4] && m[0] > m[8])
    {
        const double s = std::sqrt(1.0 + m[0] - m[4] - m[8]) * 2;
        q = {(m[5] - m[7]) / s, 0.25 * s, (m[3] + m[1]) / s, (m[6] + m[2]) / s};
    }
    else if (m[4] > m[8])
    {
        const double s = std::sqrt(1.0 + m[4] - m[0] - m[8]) * 2;
        q = {(m[6] - m[2]) / s, (m[3] + m[1]) / s, 0.25 * s, (m[7] + m[5]) / s};
    }
    else
    {
        const double s = std::sqrt(1.0 + m[8] - m[0] - m[4]) * 2;
        q = {(m[1] - m[3]) / s, (m[6] + m[2]) / s, (m[7] + m[5]) / s, 0.25 * s};
    }
    return q;
}
M3 matrix(Q q)
{
    const double n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    q = {q.w / n, q.x / n, q.y / n, q.z / n};
    const double xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z, xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z,
                 wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    M3 r;
    r.m = {1 - 2 * (yy + zz), 2 * (xy + wz), 2 * (xz - wy), 2 * (xy - wz),    1 - 2 * (xx + zz),
           2 * (yz + wx),     2 * (xz + wy), 2 * (yz - wx), 1 - 2 * (xx + yy)};
    return r;
}
double angle_between(const M3 &a, const M3 &b)
{ // rotation angle of a^T*b
    const auto d = mul(transpose(a), b);
    const double c = std::clamp((d.m[0] + d.m[4] + d.m[8] - 1.0) * 0.5, -1.0, 1.0);
    return std::acos(c);
}
// Shortest-arc normalized lerp; outside proper rotations an element-wise blend.
M3 blend(const M3 &a, const M3 &b, double t)
{
    if (rotation(a) && rotation(b))
    {
        const auto qa = quaternion(a);
        auto qb = quaternion(b);
        if (qa.w * qb.w + qa.x * qb.x + qa.y * qb.y + qa.z * qb.z < 0)
            qb = {-qb.w, -qb.x, -qb.y, -qb.z};
        return matrix(
            {qa.w + (qb.w - qa.w) * t, qa.x + (qb.x - qa.x) * t, qa.y + (qb.y - qa.y) * t, qa.z + (qb.z - qa.z) * t});
    }
    M3 r;
    for (unsigned i = 0; i < 9; ++i)
        r.m[i] = a.m[i] + (b.m[i] - a.m[i]) * t;
    return r;
}
// Camera rotation, possibly left-handed: interpolate its proper part.
M3 blend_camera(const M3 &a, const M3 &b, double t)
{
    if (determinant(a) < 0 && determinant(b) < 0)
    {
        auto fa = a, fb = b;
        for (unsigned r = 0; r < 3; ++r)
        {
            fa.m[r * 3 + 2] = -fa.m[r * 3 + 2];
            fb.m[r * 3 + 2] = -fb.m[r * 3 + 2];
        }
        auto x = blend(fa, fb, t);
        for (unsigned r = 0; r < 3; ++r)
            x.m[r * 3 + 2] = -x.m[r * 3 + 2];
        return x;
    }
    return blend(a, b, t);
}
M3 camera_rotation(const ViewSnapshot &v)
{
    M3 r;
    r.m = v.world_to_camera;
    return r;
}
std::optional<M3> body_basis(const VehicleSnapshot &v)
{ // rows x,y(up),z(forward)
    V3 z = v.forward, up = v.up;
    if (!normalize(z) || !normalize(up))
        return std::nullopt;
    V3 x = cross(up, z);
    if (!normalize(x))
        return std::nullopt;
    const V3 y = cross(z, x);
    M3 r;
    r.m = {x[0], x[1], x[2], y[0], y[1], y[2], z[0], z[1], z[2]};
    return r;
}
struct Affine
{
    M3 a;
    V3 b{};
    bool identity{true};
}; // c' = c*a + b (camera space)
// NEW HOST (R28 smooth effects): the motion of one vertex group. A blend (the
// newest quad of a trail) moves a vertex by `affine` + weight * (`target` -
// `affine`), weight 1 at the newest ring point and 0 at the older one; the
// weight comes from the vertex's world position in the base frame.
struct Motion
{
    Affine affine;
    bool blend{};
    Affine target;
    V3 older{}, axis{};
    double inverse_length2{};
};
struct Motions
{
    std::array<Motion, 2 + 2 * vehicle_slots> groups; // static, sky, vehicles 1..6, shadows 7..12
    std::vector<Motion> effects;                      // FrameTags::effects of the base frame
    Affine to_world;                                  // base camera space -> world
    const Motion *of(std::uint16_t group) const noexcept
    {
        if (group == group_overlay)
            return nullptr;
        if (group == group_sky)
            return &groups[1];
        if (group >= group_first_vehicle && group < group_first_vehicle + vehicle_slots)
            return &groups[2 + (group - group_first_vehicle)];
        if (group >= group_first_shadow && group < group_first_shadow + vehicle_slots)
            return &groups[2 + vehicle_slots + (group - group_first_shadow)];
        if (group >= group_first_effect && std::size_t(group - group_first_effect) < effects.size())
            return &effects[group - group_first_effect];
        return &groups[0];
    }
};
bool moves(const Motion &m) noexcept
{
    return !m.affine.identity || (m.blend && !m.target.identity);
}
struct Frame
{
    ImmutablePolygonSubmission submission;
    SdrGammaRamp gamma;
    ImmutableFrameTags tags;
    std::uint64_t serial{};
    double time{};        // due time of the tick that produced it, source counter units
    std::uint32_t tick{}; // 612CFC simulation_tick at the draw
    // NEW HOST (R23): host_wide canvas when the frame was observed. A different canvas
    // (original_4x3 switched, window resized) never forms a pair with it.
    float canvas_width{}, canvas_height{};
    // NEW HOST (R33): GPU re-projection description of this capture as a base, built on first use.
    mutable std::shared_ptr<const PolygonReprojectionBase> gpu_base;
    mutable bool gpu_unusable{};
};
bool finite_positive(float x)
{
    return std::isfinite(x) && x > 0.0f;
}
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_word;
// NEW HOST (R28): moved camera-space position of a vertex under its group motion.
V3 apply(const Motion &m, const V3 &c, const Affine &to_world)
{
    const auto moved = add(mul(c, m.affine.a), m.affine.b);
    if (!m.blend)
        return moved;
    double weight = 1.0;
    if (m.inverse_length2 > 0)
    {
        const auto p = add(mul(c, to_world.a), to_world.b);
        weight = std::clamp(dot(sub(p, m.older), m.axis) * m.inverse_length2, 0.0, 1.0);
    }
    const auto target = add(mul(c, m.target.a), m.target.b);
    return add(moved, scale(sub(target, moved), weight));
}
// A world translation d added to the camera motion `fixed` (W = interpolated world_to_camera).
Affine translated(const Affine &fixed, const V3 &d, const M3 &w)
{
    if (d == V3{})
        return fixed;
    Affine r = fixed;
    r.b = add(fixed.b, mul(d, w));
    r.identity = false;
    return r;
}

// NEW HOST (R23): guard band control (host_wide_race_view.hpp). The capture of the
// next tick is culled/clipped `angle` beyond each canvas edge. The angle
// follows the camera motion measured between canonical frames (rotation plus
// the parallax of its translation at a reference depth), with headroom, so
// straight driving pays a small margin and fast turns a larger one.
constexpr double degree = 3.14159265358979323846 / 180.0;
constexpr double guard_default = 3.0 * degree; // no motion measured yet (180 deg/s)
constexpr double guard_floor = 1.5 * degree;   // also hides the retail z-vs-z+1 edge sliver
constexpr double guard_ceiling = 6.0 * degree; // bounds the extra world work (about 1.8x frustum area at worst)
constexpr double guard_headroom = 1.25;
constexpr std::size_t guard_history = 8; // canonical transitions (~133 ms)
// NEW HOST (R28): a particle may move this much more than twice its velocity per tick.
constexpr double particle_step_slack = 64.0;
constexpr double parallax_near_planes = 4.0; // reference depth = 4 near-plane distances
// NEW HOST (R23): re-projected triangles are clipped at the source near plane
// (w = clip_near+1, 4277D0 keeps z > clip_near) instead of clamping vertices.
constexpr double near_tolerance = 1e-5;          // relative; binary32 round trip of a vertex ON the plane
constexpr std::size_t max_draw_vertices = 65536; // WORD indices (polygon_d3d12 record bound)

// Mapping of one draw: X = kx*c.x/w + cx (429A40/41E410 + 4335E8), Z/RHW = 32/w.
struct Mapping
{
    double kx{}, ky{}, cx{}, cy{};
    bool valid{};
};
Mapping mapping_of(const retail::ViewportState &view, double px, double py)
{
    Mapping m{double(view.scale_x) * px, double(view.scale_y) * py, double(view.center_x), double(view.center_y)};
    m.valid = std::abs(m.kx) > 0 && std::abs(m.ky) > 0;
    return m;
}
// Camera-space point of a captured vertex (exact inverse of the mapping).
std::optional<V3> unproject(const retail::PolygonSubmissionVertex &vertex, const Mapping &m)
{
    const float rhw = as_float(vertex.words[3]);
    if (!finite_positive(rhw) || !m.valid)
        return std::nullopt;
    const double w = 32.0 / double(rhw);
    return V3{(double(as_float(vertex.words[0])) - m.cx) * w / m.kx,
              (double(as_float(vertex.words[1])) - m.cy) * w / m.ky, w - 1.0};
}
// The same mapping with a transformed point; Z keeps its ratio to RHW (or
// stays equal to RHW when the capture stored Z==RHW). w_min only guards a
// vertex that no triangle references after near clipping, or a draw that
// cannot be clipped (lines, invalid/overlay vertices).
void project(retail::PolygonSubmissionVertex &vertex, const V3 &moved, double w_min, const Mapping &m)
{
    auto &words = vertex.words;
    const float rhw = as_float(words[3]);
    const double w2 = std::max(moved[2] + 1.0, w_min);
    const float rhw2 = static_cast<float>(32.0 / w2);
    words[0] = as_word(static_cast<float>(m.kx * moved[0] / w2 + m.cx));
    words[1] = as_word(static_cast<float>(m.ky * moved[1] / w2 + m.cy));
    words[2] = words[2] == words[3] ? as_word(rhw2)
                                    : as_word(static_cast<float>(double(as_float(words[2])) * (double(rhw2) / rhw)));
    words[3] = as_word(rhw2);
}
std::uint32_t lerp_color(std::uint32_t a, std::uint32_t b, double s)
{
    std::uint32_t r = 0;
    for (unsigned shift = 0; shift < 32; shift += 8)
    {
        const double x = double((a >> shift) & 0xffu), y = double((b >> shift) & 0xffu);
        const auto c = static_cast<std::uint32_t>(std::clamp(std::lround(x + (y - x) * s), 0l, 255l));
        r |= c << shift;
    }
    return r;
}
bool has_gpu_insertions(const SourceCompletedFrame &frame)
{
    if (!frame.submission)
        return false;
    for (const auto &segment : frame.submission->segments)
        if (segment.device_frame && segment.device_frame->polygon_commands &&
            host_frame_phase::has_foreign_insertions(*segment.device_frame->polygon_commands)) // NEW HOST (R28)
            return true;
    return false;
}
} // namespace

PresenterOptions presenter_options(bool command_line_enabled)
{
    PresenterOptions o;
    o.enabled = command_line_enabled;
    // NEW HOST (R30): the session log of the game root the executable runs from.
    {
        std::wstring module(MAX_PATH, L'\0');
        const auto length = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
        if (length > 0 && length < module.size())
        {
            module.resize(length);
            const auto root = std::filesystem::path(module).parent_path();
            std::error_code error;
            if (std::filesystem::is_directory(root / "data/retail", error))
                o.report_log = root / "reports/source-menu-session.log";
        }
    }
    char buffer[16]{};
    std::size_t size = 0;
    if (getenv_s(&size, buffer, sizeof buffer, "SGN_INTERP") == 0 && size > 1)
    {
        if (buffer[0] == '0')
            o.enabled = false;
        else if (buffer[0] == '1')
            o.enabled = true;
    }
    return o;
}

struct InterpolatingPresenter::Impl
{
    retail::FrontendState &frontend;
    SourceMenuPresentationBackend &backend;
    const std::unique_ptr<Renderer> &renderer;
    retail::FrontendPerformanceCounter &counter;
    const retail::SourceHostRebasedCounter *precise;
    PresenterOptions options;
    // NEW HOST (R22): three canonical frames (N-2, N-1, N). The display samples the
    // state half a tick further in the past than R19 so that a tick whose pass
    // has not finished yet never freezes the pose (measured 15-25% frames held
    // at N at 240 Hz); the older pair covers that half tick.
    std::optional<Frame> older, previous, current;
    static constexpr double presentation_margin_ticks = 0.5;
    std::uint64_t observed_serial{};
    bool active{}, waiting{};
    // NEW HOST (R23): `enabled` follows the options/graphics setting; `failed` is a
    // host presentation failure (reset when the user enables it again).
    bool enabled{}, failed{};
    // NEW HOST (R23): live host_graphics::Settings::interpolation. The value present
    // when the presenter was created is the baseline (the command line and
    // SGN_INTERP decided `options.enabled`); later changes are applied.
    std::uint64_t settings_revision{};
    bool settings_interpolation{};
    bool smooth_effects{};  // NEW HOST (R28): host_graphics::Settings::smooth_effects, live
    Motions effect_motions; // NEW HOST (R28): scratch of present()
    // NEW HOST (R33) GPU re-projection path (polygon_reprojection.hpp). The base
    // capture's slot stream and draw mappings are built once per base frame; per
    // present only the slot motions (the Motions of transforms(), as floats) are
    // passed and the GPU re-projects the moving draws. SGN_INTERP_GPU=0 keeps the
    // R30 CPU build for every frame. A base the GPU path cannot record falls back
    // to the CPU build (interp.cpu_fallback_frames).
    bool gpu_path{gpu_path_option()};
    std::vector<PolygonReprojectionMotion> gpu_motions;
    static bool gpu_path_option() noexcept
    {
        char buffer[8]{};
        std::size_t size = 0;
        return !(getenv_s(&size, buffer, sizeof buffer, "SGN_INTERP_GPU") == 0 && size > 1 && buffer[0] == '0');
    }
    // Motion slot of a vertex group: the Motions::of() mapping (overlay: never moved).
    static std::uint16_t gpu_slot(std::uint16_t group, std::size_t effects) noexcept
    {
        if (group == group_overlay)
            return polygon_reprojection_passthrough;
        if (group == group_sky)
            return 1;
        if (group >= group_first_vehicle && group < group_first_vehicle + vehicle_slots)
            return static_cast<std::uint16_t>(2 + (group - group_first_vehicle));
        if (group >= group_first_shadow && group < group_first_shadow + vehicle_slots)
            return static_cast<std::uint16_t>(2 + vehicle_slots + (group - group_first_shadow));
        if (group >= group_first_effect && std::size_t(group - group_first_effect) < effects)
            return static_cast<std::uint16_t>(polygon_reprojection_fixed_slots + (group - group_first_effect));
        return 0;
    }
    std::shared_ptr<const PolygonReprojectionBase> gpu_base_of(const Frame &base) noexcept
    {
        if (base.gpu_base || base.gpu_unusable)
            return base.gpu_base;
        try
        {
            SGN_PROFILE_SCOPE("host.interp_gpu_base");
            const auto &tags = *base.tags;
            const auto &source = *base.submission;
            auto out = std::make_shared<PolygonReprojectionBase>();
            out->submission = base.submission;
            const double px = tags.view.projection_x, py = tags.view.projection_y;
            const double w_min = std::max(1e-3, 0.5 * (tags.view.clip_near + 1.0));
            const double w_near =
                std::isfinite(tags.view.clip_near) && tags.view.clip_near > 0 ? tags.view.clip_near + 1.0 : w_min;
            out->w_near = static_cast<float>(w_near);
            const std::size_t effects = tags.effects.size();
            if (effects > std::size_t{polygon_reprojection_passthrough} - polygon_reprojection_fixed_slots)
                throw std::length_error("too many effect groups for the GPU slot stream");
            std::size_t ordinal = 0;
            for (const auto &segment : source.segments)
            {
                out->segment_first_draw.push_back(out->draws.size());
                for (const auto &command : segment.device_frame->polygon_commands->commands)
                {
                    const auto *draw = std::get_if<CapturedPolygonDraw>(&command);
                    if (!draw)
                        continue;
                    const auto &tag = tags.draws.at(ordinal++);
                    // The mapping build() uses (R20 wide_3d / R23 guard band factors).
                    const auto wide =
                        host_wide::widened_view(tags.view.wide_factor_x, tags.view.wide_factor_y, draw->viewport);
                    auto view = draw->viewport;
                    if (wide.active)
                    {
                        view.scale_x = wide.widened.half_width;
                        view.scale_y = wide.widened.half_height;
                        view.center_x = wide.widened.center_x;
                        view.center_y = wide.widened.center_y;
                    }
                    const auto m = mapping_of(view, px, py);
                    PolygonReprojectionDraw entry;
                    entry.first_slot = out->slots.size();
                    entry.mapping = {static_cast<float>(m.kx), static_cast<float>(m.ky), static_cast<float>(m.cx),
                                     static_cast<float>(m.cy), m.valid ? 1u : 0u};
                    const std::size_t n = draw->vertices.size();
                    for (std::size_t i = 0; i < n; ++i)
                        out->slots.push_back(
                            gpu_slot(tag.per_vertex ? tags.groups.at(tag.first + i) : tag.uniform, effects));
                    out->draw_index.emplace(draw, static_cast<std::uint32_t>(out->draws.size()));
                    out->draws.push_back(entry);
                }
            }
            base.gpu_base = std::move(out);
        }
        catch (const std::exception &error)
        {
            base.gpu_unusable = true;
            report(std::string("host interp: GPU re-projection unavailable for a frame: ") + error.what());
        }
        return base.gpu_base;
    }
    static void gpu_columns(const Affine &a, float *out) noexcept
    {
        for (unsigned j = 0; j < 3; ++j)
        {
            out[4 * j + 0] = static_cast<float>(a.a.m[j]);
            out[4 * j + 1] = static_cast<float>(a.a.m[3 + j]);
            out[4 * j + 2] = static_cast<float>(a.a.m[6 + j]);
            out[4 * j + 3] = static_cast<float>(a.b[j]);
        }
    }
    static void gpu_pack(const Motion &m, PolygonReprojectionMotion &out) noexcept
    {
        auto *w = out.words.data();
        gpu_columns(m.affine, w);
        gpu_columns(m.target, w + 12);
        for (unsigned i = 0; i < 3; ++i)
        {
            w[24 + i] = static_cast<float>(m.older[i]);
            w[28 + i] = static_cast<float>(m.axis[i]);
        }
        w[27] = static_cast<float>(m.inverse_length2);
        const std::uint32_t flags = (moves(m) ? 1u : 0u) | (m.blend ? 2u : 0u);
        w[31] = std::bit_cast<float>(flags);
    }
    // One GPU re-projected present of `base` with `g`; false when this base cannot
    // take the GPU path (the caller then builds on the CPU). Graphics errors propagate.
    bool present_gpu(Renderer &r, const Frame &base, const Motions &g)
    {
        const auto gpu_base = gpu_base_of(base);
        if (!gpu_base)
            return false;
        SGN_PROFILE_SCOPE("host.interp_present_gpu");
        const std::size_t slots = polygon_reprojection_fixed_slots + g.effects.size();
        gpu_motions.resize(slots);
        std::uint64_t moving = 0;
        for (std::size_t s = 0; s < slots; ++s)
        {
            const auto &m =
                s < polygon_reprojection_fixed_slots ? g.groups[s] : g.effects[s - polygon_reprojection_fixed_slots];
            gpu_pack(m, gpu_motions[s]);
            if (moves(m))
                moving |= polygon_reprojection_bit(static_cast<std::uint32_t>(s));
        }
        PolygonReprojection reprojection;
        reprojection.base = gpu_base;
        reprojection.motions = gpu_motions;
        std::array<float, 12> world{};
        gpu_columns(g.to_world, world.data());
        reprojection.to_world = world;
        reprojection.moving = moving;
        try
        {
            r.present_polygon_reprojected(base.submission, reprojection, base.gamma, true);
        }
        catch (const GraphicsError &)
        {
            throw;
        }
        catch (const std::exception &error)
        {
            // Nothing was presented (the recorder validates before Execute/Present).
            base.gpu_unusable = true;
            report(std::string("host interp: GPU re-projection rejected a frame, CPU build used: ") + error.what());
            return false;
        }
        return true;
    }
    // NEW HOST (R23): guard band inputs.
    std::array<double, guard_history> motion{};
    std::size_t motion_count{}, motion_next{};
    bool gpu_world_frames{};
    std::vector<unsigned char> behind; // scratch per draw (near clipping)
    std::vector<V3> moved;
    std::vector<unsigned char> known, transformed;
    // NEW HOST (R30): persistent output of build(), reused by every present. The
    // structure of the base capture (device, snapshots, events, textures, GPU
    // insertions) is mirrored once per new base frame; per present only the
    // command list is refreshed and the draws are rewritten in place.
    struct ScratchSegment
    {
        std::shared_ptr<CapturedPolygonDeviceFrame> device;
        std::shared_ptr<CapturedPolygonFrame> commands;
        std::vector<unsigned char> pristine; // per command: this draw holds the base draw's exact bits
        bool events_retyped{};               // a near-clipped draw retyped its device event (restored next build)
    };
    ImmutablePolygonSubmission scratch_source; // base mirrored by the scratch (held, so its address stays unique)
    CapturedPolygonSubmission scratch;
    std::vector<ScratchSegment> scratch_segments;
    std::vector<std::vector<retail::PolygonSubmissionVertex>> spare_vertices; // draw buffers across layout shifts
    std::vector<std::vector<std::uint16_t>> spare_indices;
    std::vector<retail::PolygonSubmissionVertex> clip_extra; // clip_near scratch
    std::vector<std::uint16_t> clip_indices;
    const char *last_rejection{};       // NEW HOST (R30 perf): rejection() of the last observed frame, if any
    std::optional<double> last_pose_at; // NEW HOST (R30 perf) diagnostics only (SGN_PROFILE)
    struct BuildCounters                // NEW HOST (R30) diagnostics (SGN_PROFILE quantities)
    {
        std::int64_t allocations{}, bytes{}, vertices_copied{}, vertices_projected{}, draws_kept{};
        std::int64_t legacy_allocations{}, legacy_bytes{}; // what the R29 full copy did for the same present
    } counters;
    std::optional<std::chrono::steady_clock::time_point> last_present; // NEW HOST (R22) diagnostics only (SGN_PROFILE)
    std::string failure;
    // NEW HOST (R30) diagnostics: each distinct line is written once per session.
    std::vector<std::string> reported;
    void report(std::string line) noexcept
    {
        try
        {
            if (std::find(reported.begin(), reported.end(), line) != reported.end())
                return;
            reported.push_back(line);
            OutputDebugStringA(("SGN " + line + "\n").c_str());
            if (options.report_log.empty())
                return;
            std::error_code error;
            std::filesystem::create_directories(options.report_log.parent_path(), error);
            // A diagnostic line never replaces the outcome it reports: an
            // unwritable log only loses the line (the stream reports no error).
            std::ofstream(options.report_log, std::ios::app) << line << '\n';
        }
        catch (const std::bad_alloc &)
        {
            OutputDebugStringA("SGN host interp: diagnostic line dropped (out of memory)\n");
        }
    }
    // NEW HOST (R30): the first vertex of a captured draw that no GPU path can
    // rasterize (nonfinite XYZRHW or RHW <= 0; the 432F60 screen fans, type 6
    // flags 14h, may carry rejected ring points and are exempt exactly as in the
    // polygon recorder). Such a canonical frame is a SOURCE-side producer fault.
    std::optional<std::string> unrasterizable(const SourceCompletedFrame &frame) const
    {
        if (!frame.submission)
            return std::nullopt;
        std::size_t segment_index = 0, ordinal = 0;
        for (const auto &segment : frame.submission->segments)
        {
            if (segment.device_frame && segment.device_frame->polygon_commands)
            {
                const auto &commands = segment.device_frame->polygon_commands->commands;
                for (std::size_t index = 0; index < commands.size(); ++index)
                {
                    const auto *draw = std::get_if<CapturedPolygonDraw>(&commands[index]);
                    if (!draw)
                        continue;
                    const bool screen_fan = draw->type == 6 && !draw->indexed && draw->flags == 0x14;
                    for (std::size_t v = 0; !screen_fan && v < draw->vertices.size(); ++v)
                    {
                        const auto &w = draw->vertices[v].words;
                        const bool finite = std::isfinite(as_float(w[0])) && std::isfinite(as_float(w[1])) &&
                                            std::isfinite(as_float(w[2])) && std::isfinite(as_float(w[3]));
                        if (finite && as_float(w[3]) > 0.0f)
                            continue;
                        const char *group = "untagged";
                        if (frame.tags && frame.tags->consistent && ordinal < frame.tags->draws.size())
                        {
                            const auto &tag = frame.tags->draws[ordinal];
                            const auto g = tag.per_vertex && tag.first + v < frame.tags->groups.size()
                                               ? frame.tags->groups[tag.first + v]
                                               : tag.uniform;
                            group = g == group_overlay        ? "overlay2d"
                                    : g == group_sky          ? "world sky"
                                    : g >= group_first_effect ? "world effect sprite"
                                    : g >= group_first_shadow && g < group_first_shadow + vehicle_slots ? "world shadow"
                                    : g >= group_first_vehicle && g < group_first_vehicle + vehicle_slots
                                        ? "world vehicle"
                                        : "world static";
                        }
                        char text[320];
                        std::snprintf(text, sizeof text,
                                      "source frame tick %u segment %zu command %zu draw %zu (type %u, flags %02X, %s, "
                                      "%s) vertex %zu/%zu XYZRHW %08X %08X %08X %08X",
                                      frame.tags ? unsigned(frame.tags->simulation_tick) : 0u, segment_index, index,
                                      ordinal, unsigned(draw->type), unsigned(draw->flags),
                                      draw->indexed ? "indexed" : "not indexed", group, v, draw->vertices.size(),
                                      unsigned(w[0]), unsigned(w[1]), unsigned(w[2]), unsigned(w[3]));
                        return std::string(text);
                    }
                    ++ordinal;
                }
            }
            ++segment_index;
        }
        return std::nullopt;
    }

    Impl(retail::FrontendState &f, SourceMenuPresentationBackend &b, const std::unique_ptr<Renderer> &r,
         retail::FrontendPerformanceCounter &c, PresenterOptions o)
        : frontend(f), backend(b), renderer(r), counter(c),
          precise(dynamic_cast<const retail::SourceHostRebasedCounter *>(&c)), options(o)
    {
        enabled = options.enabled;
        set_collection_enabled(enabled);
        settings_revision = host_graphics::revision();
        settings_interpolation = host_graphics::current().interpolation;
        smooth_effects = host_graphics::current().smooth_effects;
        update_guard();
    }
    bool usable() const noexcept
    {
        return enabled && !failed;
    }
    double now()
    { // source counter units (the unchanged 442490 counter), fractional when available
        return precise ? precise->precise_counter() : double(counter.counter());
    }
    void deactivate() noexcept
    {
        active = false;
        waiting = false;
        last_present.reset(); // NEW HOST (R28b): interp.present_interval never spans an inactive period
        older.reset();
        previous.reset();
        current.reset();
        motion_count = 0;
        motion_next = 0;
        set_race_present_suppressed(false);
    }
    void fail(const char *what) noexcept
    {
        failure = what;
        OutputDebugStringA("SGN interp: high-refresh presentation disabled: ");
        OutputDebugStringA(what);
        OutputDebugStringA("\n");
        sgn::host_profile::add("interp.reject_failed", 0); // NEW HOST (R30)
        report(std::string("host interp: disabled: ") + what);
        failed = true;
        deactivate();
        // NEW HOST (R31): a present made inside the source pass (pacing yield) can
        // fail while the source capture is open. Tag collection and the guard band
        // never change inside a capture (as in apply_settings): observe() applies
        // them after the pass; this frame's tags are then simply not used.
        if (backend.source_frame_active())
        {
            collection_off_pending = true;
            return;
        }
        set_collection_enabled(false);
        update_guard();
    }
    bool collection_off_pending{}; // NEW HOST (R31)
    // NEW HOST (R23): guard band for the next capture (0 = off).
    double guard_angle() const noexcept
    {
        if (!motion_count)
            return guard_default;
        double largest = 0;
        for (std::size_t i = 0; i < motion_count; ++i)
            largest = std::max(largest, motion[i]);
        return std::clamp(largest * guard_headroom, guard_floor, guard_ceiling);
    }
    void update_guard() noexcept
    {
        // gpu_world hybrid frames are never re-projected: no margin to pay for.
        host_wide::set_race_guard_band(usable() && !gpu_world_frames ? guard_angle() : 0.0);
    }
    // Camera motion of one canonical transition in radians (rotation angle plus
    // translation parallax at the reference depth); nullopt for a cut.
    std::optional<double> camera_motion(const ViewSnapshot &a, const ViewSnapshot &b) const
    {
        if (a.controller != b.controller || a.camera_mode != b.camera_mode ||
            a.controller_vehicle != b.controller_vehicle)
            return std::nullopt;
        const auto wa = camera_rotation(a), wb = camera_rotation(b);
        const double da = determinant(wa), db = determinant(wb);
        if (!std::isfinite(da) || !std::isfinite(db) || !(da * db > 0))
            return std::nullopt;
        const double turn = angle_between(wa, wb);
        const auto step = sub(b.camera_position, a.camera_position);
        if (!std::isfinite(turn) || turn > options.max_turn_radians || !(length(step) <= options.cut_distance))
            return std::nullopt;
        const auto local = mul(step, wb); // camera-space displacement (x right, y up, z forward)
        const double near_clip = std::isfinite(b.clip_near) && b.clip_near > 0 ? b.clip_near : 100.0;
        const double depth = parallax_near_planes * (near_clip + 1.0);
        // Forward motion pushes the picture outward (nothing is uncovered);
        // lateral motion and moving back uncover the edges.
        const double parallax = (std::hypot(local[0], local[1]) + 0.5 * std::max(0.0, -local[2])) / depth;
        const double result = turn + parallax;
        if (!std::isfinite(result))
            return std::nullopt;
        return result;
    }
    void record_motion(const ViewSnapshot &a, const ViewSnapshot &b)
    {
        const auto value = camera_motion(a, b);
        if (!value)
            return;
        motion[motion_next] = *value;
        motion_next = (motion_next + 1) % guard_history;
        motion_count = std::min(motion_count + 1, guard_history);
    }
    // NEW HOST (R23): live interpolation setting, applied between source passes.
    void apply_settings()
    {
        const auto revision = host_graphics::revision();
        if (revision == settings_revision)
            return;
        // NEW HOST (R28): presentation only (the tags are always collected): at once.
        smooth_effects = host_graphics::current().smooth_effects;
        // Collection must not change inside a capture; retried after the pass.
        if (backend.source_frame_active())
            return;
        settings_revision = revision;
        const bool wanted = host_graphics::current().interpolation;
        if (wanted == settings_interpolation)
            return;
        settings_interpolation = wanted;
        set_enabled(wanted);
    }
    void set_enabled(bool on) noexcept
    {
        if (on)
        {
            if (enabled && !failed)
                return;
            enabled = true;
            failed = false;
            failure.clear();
            gpu_world_frames = false;
            set_collection_enabled(true); // tags from the next source frame on
            OutputDebugStringA("SGN interp: high-refresh presentation enabled by the graphics settings\n");
        }
        else
        {
            if (!enabled)
                return;
            // Back to the canonical path: the next canonical race Flip presents
            // again (its deferred image is rasterized by the surfaces first).
            enabled = false;
            deactivate();
            set_collection_enabled(false);
            OutputDebugStringA("SGN interp: high-refresh presentation disabled by the graphics settings\n");
        }
        update_guard();
    }
    // NEW HOST (R30): why a frame cannot be interpolated (profiler counter name,
    // counted by the caller), or nullptr. Same tests as the former interpolatable().
    const char *rejection(const SourceCompletedFrame &frame) const
    {
        const auto &world = frontend.world();
        if (world.scene_mode != 1)
            return "interp.reject_scene";
        if (world.split_setting != 0)
            return "interp.reject_split_screen";
        if (!frame.submission || !frame.tags)
            return "interp.reject_no_tags";
        const auto &t = *frame.tags;
        if (!t.race_view)
            return "interp.reject_not_race_view";
        if (!t.consistent)
            return "interp.reject_tags_inconsistent";
        if (!t.packet_hooks || !t.model_hooks)
            return "interp.reject_no_world_hooks";
        if (t.world_views != 1)
            return "interp.reject_world_views";
        if (!t.view.valid || !(std::abs(t.view.projection_x) > 0) || !(std::abs(t.view.projection_y) > 0))
            return "interp.reject_view";
        std::size_t draws = 0;
        for (const auto &segment : frame.submission->segments)
        {
            if (!segment.device_frame || !segment.device_frame->polygon_commands)
                return "interp.reject_segment";
            const auto &commands = *segment.device_frame->polygon_commands;
            // gpu_world insertions draw from static buffers: no vertices to re-project.
            // NEW HOST (R28): frame phase markers carry no geometry; build() keeps them.
            if (host_frame_phase::has_foreign_insertions(commands))
                return "interp.reject_foreign_insertions";
            for (const auto &command : commands.commands)
                if (const auto *draw = std::get_if<CapturedPolygonDraw>(&command))
                {
                    if (draws >= t.draws.size() || t.draws[draws].vertices != draw->vertices.size())
                        return "interp.reject_draw_tag_mismatch";
                    ++draws;
                }
        }
        return draws == t.draws.size() ? nullptr : "interp.reject_draw_tag_mismatch";
    }
    void observe()
    {
        if (collection_off_pending && !backend.source_frame_active())
        {
            collection_off_pending = false; // NEW HOST (R31): deferred from fail() inside a pass
            if (!usable())
                set_collection_enabled(false);
        }
        apply_settings();
        observe_frame();
        update_guard();
        if (!usable())
            diagnose_disabled(); // NEW HOST (R30)
    }
    // NEW HOST (R30): with interpolation off or failed the canonical path draws
    // every race frame; an unrasterizable one is still named in the session log.
    std::uint64_t diagnosed_serial{};
    void diagnose_disabled()
    {
        if (frontend.world().scene_mode != 1)
            return;
        const auto *latest = backend.last_completed_frame();
        if (!latest || latest->serial == diagnosed_serial)
            return;
        diagnosed_serial = latest->serial;
        if (const auto fault = unrasterizable(*latest))
            report("host interp (inactive): canonical " + *fault);
    }
    void observe_frame()
    {
        if (!usable())
            return;
        const auto &world = frontend.world();
        if (world.scene_mode != 1)
        {
            if (active || current)
                deactivate();
            return;
        }
        const auto *latest = backend.last_completed_frame();
        if (!latest || latest->serial == observed_serial)
            return;
        observed_serial = latest->serial;
        if (latest->tags && latest->tags->race_view)
            gpu_world_frames = has_gpu_insertions(*latest);
        // NEW HOST (R30): a canonical frame with an unrasterizable vertex (a source
        // producer fault; the polygon recorder rejects it) is never taken into the
        // history. While interpolating, the presenter holds its last good pair and
        // the canonical Present stays suppressed, so that frame is never drawn;
        // before R30 presenting it threw and fail() silently disabled interpolation
        // for the rest of the session. Reported once with its exact position.
        if (const auto fault = unrasterizable(*latest))
        {
            sgn::host_profile::add("interp.hold_unrasterizable_source_frame", 0);
            report("host interp: held back an unrasterizable " + *fault);
            if (!active)
                deactivate();
            return;
        }
        last_rejection = nullptr; // NEW HOST (R30 perf): transition reason for the profiler report
        if (const char *reason = rejection(*latest))
        {
            sgn::host_profile::add(reason, 0);
            last_rejection = reason;
            deactivate();
            return;
        }
        // Due time of the tick that produced this state, in the same counter
        // units: 442490 leaves scheduled = next due, so the last due was
        // scheduled-increment (32-bit source words, wrap-safe difference).
        const auto &clock = world.tick_clock;
        const auto raw = counter.counter();
        const auto behind_counts =
            static_cast<std::int32_t>(static_cast<std::uint32_t>(raw) - (clock.scheduled - clock.increment));
        const auto layout = host_wide::current();
        Frame next{latest->submission,
                   latest->gamma,
                   latest->tags,
                   latest->serial,
                   double(static_cast<std::int64_t>(raw) - behind_counts),
                   latest->tags->simulation_tick,
                   layout.canvas_width,
                   layout.canvas_height};
        const bool same_canvas =
            current && current->canvas_width == next.canvas_width && current->canvas_height == next.canvas_height;
        if (current && same_canvas && next.tick != current->tick && next.time > current->time)
        {
            record_motion(current->tags->view, next.tags->view);
            older = std::move(previous);
            previous = std::move(current);
        }
        else
        {
            older.reset();
            previous.reset();
        } // first frame, new canvas, or no tick between (pause gate still ticks): snap
        current = std::move(next);
        active = true;
        waiting = true;
        set_race_present_suppressed(true);
    }
    // Pair (A,B) of consecutive canonical frames around the displayed state
    // time and the blend factor. state = now - one tick - margin (R19: margin 0).
    struct Blend
    {
        const Frame *a{};
        const Frame *b{};
        double t{1.0};
    };
    Blend blend_at(double at) const
    {
        Blend r;
        r.b = &*current;
        if (!previous)
            return r;
        const double state = at - double(frontend.world().tick_clock.increment) * (1.0 + presentation_margin_ticks);
        if (state >= previous->time || !older)
        {
            r.a = &*previous;
            r.b = &*current;
        }
        else
        {
            r.a = &*older;
            r.b = &*previous;
        }
        const double span = r.b->time - r.a->time;
        r.t = span > 0 ? std::clamp((state - r.a->time) / span, 0.0, 1.0) : 1.0;
        return r;
    }
    bool wants() const noexcept
    {
        return usable() && active && current && (waiting || previous);
    }
    // Interpolated transforms relative to frame N-1 (previous), the BASE capture.
    // NEW HOST (R22): the base is the OLDER frame re-projected forward. Its near-plane
    // cut then moves behind the camera. Re-projecting frame N backward (R19)
    // pulled N's near-plane cut into view: a straight edge with nothing below
    // it at the bottom of the screen while driving forward.
    bool transforms(double t, const Frame &base, const Frame &next, Motions &motions)
    {
        std::array<Affine, vehicle_slots + 2> out;
        const auto &a = base.tags->view;
        const auto &b = next.tags->view;
        if (a.controller != b.controller || a.camera_mode != b.camera_mode ||
            a.controller_vehicle != b.controller_vehicle)
            return false;
        const auto wa = camera_rotation(a), wb = camera_rotation(b);
        if (length(sub(b.camera_position, a.camera_position)) > options.cut_distance)
            return false;
        const auto wa_inverse = inverse(wa);
        if (!wa_inverse)
            return false;
        if (rotation(wa) && rotation(wb) && angle_between(wa, wb) > options.max_turn_radians)
            return false;
        const auto w = blend_camera(wa, wb, t);
        const auto position = lerp(a.camera_position, b.camera_position, t);
        const auto translation = lerp(a.camera_translation, b.camera_translation, t);
        // p = (c - tB)*WB^-1 + posB ; c' = (p - pos)*W + t   (B = base frame N-1)
        const auto to_world = mul(scale(a.camera_translation, -1.0), *wa_inverse); // -tB*WB^-1
        Affine fixed;
        fixed.a = mul(*wa_inverse, w);
        fixed.b = add(mul(add(to_world, sub(a.camera_position, position)), w), translation);
        // Unchanged camera (pause gate, standing start): keep the captured bits.
        fixed.identity =
            t <= 0.0 || (a.camera_position == b.camera_position && a.world_to_camera == b.world_to_camera &&
                         a.camera_translation == b.camera_translation);
        Affine sky;
        sky.a = fixed.a;
        sky.b = add(mul(to_world, w), translation);
        sky.identity = fixed.identity;
        out[0] = fixed;
        out[1] = sky;
        for (unsigned k = 0; k < vehicle_slots; ++k)
        {
            auto &g = out[2 + k];
            g = fixed; // an inactive, new or cut vehicle keeps its N pose
            const auto &va = base.tags->vehicles[k];
            const auto &vb = next.tags->vehicles[k];
            if (!va.active || !vb.active || t <= 0.0)
                continue;
            if (va.position == vb.position && va.up == vb.up && va.forward == vb.forward)
                continue; // at rest: moves with the world
            if (length(sub(vb.position, va.position)) > options.cut_distance)
                continue;
            const auto ba = body_basis(va), bb = body_basis(vb);
            if (!ba || !bb || angle_between(*ba, *bb) > options.max_turn_radians)
                continue;
            const auto body = blend(*ba, *bb, t);
            const auto pos = lerp(va.position, vb.position, t);
            // p' = (p - PB)*BB^T*B + P   (base pose of frame N-1)
            const auto delta = mul(transpose(*ba), body);
            g.a = mul(mul(*wa_inverse, delta), w);
            const auto local = mul(sub(add(to_world, a.camera_position), va.position), delta);
            g.b = add(mul(sub(add(local, pos), position), w), translation);
            g.identity = false;
        }
        // NEW HOST (R28 smooth effects). Off: shadows move like their vehicle and
        // effects like the world, exactly as before.
        motions.to_world.a = *wa_inverse;
        motions.to_world.b = add(to_world, a.camera_position);
        motions.to_world.identity = false;
        for (unsigned k = 0; k < 2 + vehicle_slots; ++k)
            motions.groups[k] = Motion{out[k]};
        for (unsigned k = 0; k < vehicle_slots; ++k)
        {
            auto &shadow = motions.groups[2 + vehicle_slots + k];
            shadow = Motion{out[2 + k]};
            if (!smooth_effects)
                continue;
            // The blob shadow lies on the ground: the body's TRANSLATION only, no tilt.
            const auto &va = base.tags->vehicles[k];
            const auto &vb = next.tags->vehicles[k];
            V3 d{};
            if (va.active && vb.active && t > 0.0 && length(sub(vb.position, va.position)) <= options.cut_distance)
                d = scale(sub(vb.position, va.position), t);
            shadow = Motion{translated(fixed, d, w)};
        }
        const auto &effects = base.tags->effects;
        motions.effects.assign(effects.size(), Motion{fixed});
        if (!smooth_effects)
            return true;
        // The same particles in the next capture (slot-indexed; 64 effect, 32 impact).
        std::array<const EffectTag *, 64> effect_particles{};
        std::array<const EffectTag *, 32> impact_particles{};
        for (const auto &e : next.tags->effects)
        {
            if (e.owner == EffectOwner::effect_particle && e.slot < effect_particles.size() &&
                !effect_particles[e.slot])
                effect_particles[e.slot] = &e;
            else if (e.owner == EffectOwner::impact_particle && e.slot < impact_particles.size() &&
                     !impact_particles[e.slot])
                impact_particles[e.slot] = &e;
        }
        for (std::size_t i = 0; i < effects.size(); ++i)
        {
            const auto &e = effects[i];
            Affine target = fixed;
            if (e.owner == EffectOwner::vehicle)
            {
                if (e.slot < vehicle_slots)
                    target = out[2 + e.slot];
            }
            else
            {
                const EffectTag *later = nullptr;
                if (e.owner == EffectOwner::effect_particle && e.slot < effect_particles.size())
                    later = effect_particles[e.slot];
                else if (e.owner == EffectOwner::impact_particle && e.slot < impact_particles.size())
                    later = impact_particles[e.slot];
                // Continuity: same spawn and a step of about its own velocity; a
                // respawned or teleported slot stays world-fixed for this tick.
                if (later && later->generation == e.generation && t > 0.0)
                {
                    const auto step = sub(later->position, e.position);
                    if (length(step) <= 2.0 * length(e.velocity) + particle_step_slack)
                        target = translated(fixed, scale(step, t), w);
                }
            }
            auto &m = motions.effects[i];
            if (e.kind == EffectKind::attached)
            {
                m = Motion{target};
                continue;
            }
            // trail_segment: stretch the newest quad from the moving owner.
            if (target.identity)
                continue;
            m.blend = true;
            m.target = target;
            m.older = e.older;
            m.axis = sub(e.newest, e.older);
            const double length2 = dot(m.axis, m.axis);
            m.inverse_length2 = length2 > 1e-9 && std::isfinite(length2) ? 1.0 / length2 : 0.0;
        }
        return true;
    }
    // NEW HOST (R23): one vertex on the edge inside->outside where it meets the near
    // plane w = w_near. Every attribute is interpolated linearly along the 3D
    // edge (the same parameter for position, colours, UVs and the Z/RHW ratio),
    // and the endpoint order is fixed (inside first), so the two triangles
    // sharing an edge produce bit-identical vertices (no cracks).
    // NEW HOST (R30): the device event of a draw that clip_near() rebuilt must name
    // its new form, exactly as the capture would have recorded an indexed type-4
    // draw: validate_polygon_submission cross-checks the event's operation
    // (draw / draw_indexed) and arguments (type, FVF, flags) with the command.
    // FVF and flags are unchanged by the rebuild; the event carries no counts.
    static void retype_draw_event(CapturedPolygonDeviceFrame &device, std::size_t command_index)
    {
        for (auto &event : device.events)
            if (event.polygon_command && *event.polygon_command == command_index &&
                (event.operation == PolygonDeviceOperation::draw ||
                 event.operation == PolygonDeviceOperation::draw_indexed))
            {
                event.operation = PolygonDeviceOperation::draw_indexed;
                event.arguments[0] = 4;
                return;
            }
        throw std::logic_error("near-clip rebuild: the draw has no device event");
    }
    static retail::PolygonSubmissionVertex near_vertex(const retail::PolygonSubmissionVertex &in, const V3 &in_moved,
                                                       const retail::PolygonSubmissionVertex &out, const V3 &out_moved,
                                                       double w_near, const Mapping &m)
    {
        const double w_in = in_moved[2] + 1.0, w_out = out_moved[2] + 1.0;
        const double s = std::clamp((w_near - w_in) / (w_out - w_in), 0.0, 1.0);
        const V3 p = lerp(in_moved, out_moved, s);
        retail::PolygonSubmissionVertex v = in;
        auto &words = v.words;
        const float rhw = static_cast<float>(32.0 / w_near);
        words[0] = as_word(static_cast<float>(m.kx * p[0] / w_near + m.cx));
        words[1] = as_word(static_cast<float>(m.ky * p[1] / w_near + m.cy));
        if (in.words[2] == in.words[3] && out.words[2] == out.words[3])
            words[2] = as_word(rhw);
        else
        {
            const double qi = double(as_float(in.words[2])) / double(as_float(in.words[3]));
            const double qo = double(as_float(out.words[2])) / double(as_float(out.words[3]));
            words[2] = as_word(static_cast<float>((qi + (qo - qi) * s) * double(rhw)));
        }
        words[3] = as_word(rhw);
        words[4] = lerp_color(in.words[4], out.words[4], s);
        words[5] = lerp_color(in.words[5], out.words[5], s);
        for (unsigned i = 6; i < 10; ++i)
        {
            // Inactive UV words may hold opaque bits: interpolate finite pairs only.
            const float a = as_float(in.words[i]), b = as_float(out.words[i]);
            if (!std::isfinite(a) || !std::isfinite(b))
                continue;
            const float r = static_cast<float>(double(a) + (double(b) - double(a)) * s);
            if (std::isfinite(r))
                words[i] = as_word(r);
        }
        return v;
    }
    // NEW HOST (R23): rebuild a triangle draw whose re-projected vertices crossed the
    // near plane: every triangle is clipped at w_near (Sutherland-Hodgman, one
    // plane, winding kept) and the draw becomes an indexed triangle list over
    // the original vertices plus the new plane vertices. `original` holds the
    // captured words, draw->vertices the re-projected ones. Returns false (draw
    // untouched) when the result would exceed WORD indices.
    bool clip_near(CapturedPolygonDraw &draw, const std::vector<retail::PolygonSubmissionVertex> &original,
                   double w_near, const Mapping &m)
    {
        const std::size_t used = draw.indexed ? draw.indices.size() : draw.vertices.size();
        const auto element = [&](std::size_t k) -> std::uint32_t {
            return draw.indexed ? draw.indices[k] : static_cast<std::uint32_t>(k);
        };
        // NEW HOST (R30): member scratch (capacity kept across presents); same contents as before.
        auto &extra = clip_extra;
        auto &indices = clip_indices;
        extra.clear();
        indices.clear();
        const auto extra_capacity = extra.capacity(), index_capacity = indices.capacity();
        indices.reserve(used * 3);
        const std::size_t base = draw.vertices.size();
        bool overflow = false;
        const auto plane_vertex = [&](std::uint32_t in, std::uint32_t out) -> std::uint32_t {
            const std::size_t index = base + extra.size();
            if (index >= max_draw_vertices)
            {
                overflow = true;
                return 0;
            }
            extra.push_back(near_vertex(original[in], moved[in], original[out], moved[out], w_near, m));
            return static_cast<std::uint32_t>(index);
        };
        const auto triangle = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
            const std::array<std::uint32_t, 3> t{a, b, c};
            const unsigned outside = unsigned(behind[a]) + unsigned(behind[b]) + unsigned(behind[c]);
            if (outside == 3)
                return;
            if (outside == 0)
            {
                for (const auto i : t)
                    indices.push_back(static_cast<std::uint16_t>(i));
                return;
            }
            std::array<std::uint32_t, 4> polygon{};
            unsigned n = 0;
            for (unsigned k = 0; k < 3; ++k)
            {
                const auto from = t[k], to = t[(k + 1) % 3];
                const bool from_in = !behind[from], to_in = !behind[to];
                if (from_in)
                    polygon[n++] = from;
                if (from_in != to_in)
                    polygon[n++] = from_in ? plane_vertex(from, to) : plane_vertex(to, from);
            }
            for (unsigned j = 1; j + 1 < n; ++j)
            {
                indices.push_back(static_cast<std::uint16_t>(polygon[0]));
                indices.push_back(static_cast<std::uint16_t>(polygon[j]));
                indices.push_back(static_cast<std::uint16_t>(polygon[j + 1]));
            }
        };
        if (draw.type == 4)
            for (std::size_t k = 0; k + 2 < used; k += 3)
                triangle(element(k), element(k + 1), element(k + 2));
        else if (draw.type == 5)
        {
            // D3D strip order: odd triangles are (k+1,k,k+2), keeping one winding.
            for (std::size_t k = 0; k + 2 < used; ++k)
            {
                if (k & 1u)
                    triangle(element(k + 1), element(k), element(k + 2));
                else
                    triangle(element(k), element(k + 1), element(k + 2));
            }
        }
        else
            for (std::size_t k = 1; k + 1 < used; ++k)
                triangle(element(0), element(k), element(k + 1)); // type 6 fan
        // R29 allocated: original, indices, extra, the vertex insert.
        counters.legacy_allocations += 2 + (extra.empty() ? 0 : 2);
        counters.allocations += (extra.capacity() != extra_capacity) + (indices.capacity() != index_capacity);
        if (overflow)
            return false;
        const auto vertex_capacity = draw.vertices.capacity();
        draw.vertices.insert(draw.vertices.end(), extra.begin(), extra.end());
        counters.allocations += draw.vertices.capacity() != vertex_capacity;
        draw.type = 4;
        draw.indexed = true;
        draw.indices.swap(indices); // the previous index buffer stays as clip scratch
        return true;
    }
    // NEW HOST (R30): the draw buffers of a command slot that stops being a draw
    // are kept for the next slot that becomes one (no free/allocate churn when
    // the command layout of the next base frame shifts).
    void release(CapturedPolygonCommand &command)
    {
        auto *draw = std::get_if<CapturedPolygonDraw>(&command);
        if (!draw)
            return;
        const auto a = spare_vertices.capacity(), b = spare_indices.capacity();
        if (draw->vertices.capacity())
            spare_vertices.push_back(std::move(draw->vertices));
        if (draw->indices.capacity())
            spare_indices.push_back(std::move(draw->indices));
        counters.allocations += (spare_vertices.capacity() != a) + (spare_indices.capacity() != b);
    }
    CapturedPolygonDraw &acquire(CapturedPolygonCommand &command)
    {
        release(command);
        auto &draw = command.emplace<CapturedPolygonDraw>();
        if (!spare_vertices.empty())
        {
            draw.vertices = std::move(spare_vertices.back());
            spare_vertices.pop_back();
        }
        if (!spare_indices.empty())
        {
            draw.indices = std::move(spare_indices.back());
            spare_indices.pop_back();
        }
        return draw;
    }
    // Exact copy of a base draw into its scratch slot (capacity reused).
    void copy_draw(const CapturedPolygonDraw &in, CapturedPolygonDraw &out)
    {
        const auto vc = out.vertices.capacity(), ic = out.indices.capacity();
        out.type = in.type;
        out.fvf = in.fvf;
        out.flags = in.flags;
        out.indexed = in.indexed;
        out.viewport = in.viewport;
        out.vertices.assign(in.vertices.begin(), in.vertices.end());
        out.indices.assign(in.indices.begin(), in.indices.end());
        counters.allocations += (out.vertices.capacity() != vc) + (out.indices.capacity() != ic);
        counters.vertices_copied += static_cast<std::int64_t>(in.vertices.size());
        counters.bytes += static_cast<std::int64_t>(in.vertices.size() * sizeof(retail::PolygonSubmissionVertex) +
                                                    in.indices.size() * sizeof(std::uint16_t));
    }
    // NEW HOST (R30): mirror everything of the base capture except the command
    // list into the scratch (once per new base frame). Same values the R29 copy
    // constructors produced; validate_polygon_submission sees identical content.
    void mirror_structure(const Frame &base)
    {
        scratch_source.reset(); // a failure below leaves no stale mirror behind
        const auto &source = *base.submission;
        const auto segment_capacity = scratch.segments.capacity();
        scratch.device = source.device;
        scratch.source_tick = source.source_tick;
        scratch.initial = source.initial;
        scratch.final = source.final;
        scratch.commands = source.commands;
        scratch.events = source.events;
        scratch.segments.resize(source.segments.size());
        counters.allocations += scratch.segments.capacity() != segment_capacity;
        if (scratch_segments.size() < source.segments.size())
            scratch_segments.resize(source.segments.size());
        for (std::size_t i = 0; i < source.segments.size(); ++i)
        {
            auto &slot = scratch_segments[i];
            if (!slot.device)
            {
                slot.device = std::make_shared<CapturedPolygonDeviceFrame>();
                ++counters.allocations;
            }
            if (!slot.commands)
            {
                slot.commands = std::make_shared<CapturedPolygonFrame>();
                ++counters.allocations;
            }
            const auto &device = *source.segments[i].device_frame;
            const auto &commands = *device.polygon_commands;
            const auto events = slot.device->events.capacity(), insertions = slot.commands->gpu_insertions.capacity(),
                       pristine = slot.pristine.capacity();
            slot.device->device = device.device;
            slot.device->initial = device.initial;
            slot.device->final = device.final;
            slot.device->events = device.events;
            slot.events_retyped = false;
            slot.device->polygon_commands = slot.commands;
            slot.commands->source_tick = commands.source_tick;
            slot.commands->resource_epoch = commands.resource_epoch;
            slot.commands->gpu_insertions = commands.gpu_insertions; // host markers are kept (R28)
            slot.pristine.assign(commands.commands.size(), 0);
            counters.allocations += (slot.device->events.capacity() != events) +
                                    (slot.commands->gpu_insertions.capacity() != insertions) +
                                    (slot.pristine.capacity() != pristine);
            scratch.segments[i].device_frame = slot.device;
            scratch.segments[i].textures = source.segments[i].textures;
        }
        scratch_source = base.submission;
    }
    // NEW HOST (R30): builds into the persistent scratch instead of deep-copying
    // the whole capture per present (R29: make_shared submission, device frames,
    // command frames, every command, vertex and index vector, then projected in
    // place). The per-vertex math, its order and its inputs are unchanged: pass 1
    // reads the base capture's words (R29 read the identical copy), each output
    // vertex is the base vertex with project() applied when transformed, and
    // clip_near() receives the base vertices as `original` (R29: a copy of them
    // taken before projection). The scratch is only read by the Present below.
    const CapturedPolygonSubmission &build(const Frame &base, const Motions &g)
    {
        SGN_PROFILE_SCOPE("host.interp_build");
        counters = {};
        const auto &tags = *base.tags;
        const auto &source = *base.submission;
        const double px = tags.view.projection_x, py = tags.view.projection_y;
        const double w_min = std::max(1e-3, 0.5 * (tags.view.clip_near + 1.0));
        // NEW HOST (R23): the source near plane of the displayed pose (4277D0: z > clip_near).
        const double w_near =
            std::isfinite(tags.view.clip_near) && tags.view.clip_near > 0 ? tags.view.clip_near + 1.0 : w_min;
        if (scratch_source != base.submission)
            mirror_structure(base);
        auto &result = scratch;
        result.payload_bytes = source.payload_bytes;
        counters.legacy_allocations += 1 + (source.segments.empty() ? 0 : 1);
        counters.legacy_bytes += static_cast<std::int64_t>(sizeof(CapturedPolygonSubmission) +
                                                           source.segments.size() * sizeof(PolygonResourceSegment));
        std::size_t ordinal = 0;
        for (std::size_t segment = 0; segment < source.segments.size(); ++segment)
        {
            const auto &device_source = *source.segments[segment].device_frame;
            const auto &input = *device_source.polygon_commands;
            auto &slot = scratch_segments[segment];
            auto *commands = slot.commands.get();
            commands->payload_bytes = input.payload_bytes;
            if (slot.events_retyped)
            {
                // The previous present near-clipped a draw of this base: its event is the base event again.
                const auto event_capacity = slot.device->events.capacity();
                slot.device->events = device_source.events;
                slot.events_retyped = false;
                counters.allocations += slot.device->events.capacity() != event_capacity;
            }
            counters.legacy_allocations +=
                2 + !device_source.events.empty() + 1 + !input.commands.empty() + !input.gpu_insertions.empty();
            counters.legacy_bytes +=
                static_cast<std::int64_t>(sizeof(CapturedPolygonDeviceFrame) + sizeof(CapturedPolygonFrame) +
                                          device_source.events.size() * sizeof(PolygonDeviceCall) +
                                          input.commands.size() * sizeof(CapturedPolygonCommand) +
                                          input.gpu_insertions.size() * sizeof(PolygonGpuInsertion));
            auto &list = commands->commands;
            const std::size_t count = input.commands.size();
            const auto list_capacity = list.capacity();
            for (std::size_t k = count; k < list.size(); ++k)
                release(list[k]);
            list.resize(count);
            counters.allocations += list.capacity() != list_capacity;
            for (std::size_t k = 0; k < count; ++k)
            {
                const auto *in = std::get_if<CapturedPolygonDraw>(&input.commands[k]);
                if (!in)
                {
                    release(list[k]);
                    list[k] = input.commands[k];
                    continue;
                }
                auto *draw = std::get_if<CapturedPolygonDraw>(&list[k]);
                if (!draw)
                {
                    draw = &acquire(list[k]);
                    slot.pristine[k] = 0;
                }
                counters.legacy_allocations += !in->vertices.empty() + !in->indices.empty();
                counters.legacy_bytes +=
                    static_cast<std::int64_t>(in->vertices.size() * sizeof(retail::PolygonSubmissionVertex) +
                                              in->indices.size() * sizeof(std::uint16_t));
                const auto &tag = tags.draws[ordinal++];
                const std::size_t n = in->vertices.size();
                // A uniform group that does not move (2D overlay, unchanged camera)
                // transforms no vertex in pass 1: the output is the base draw's bits.
                if (!tag.per_vertex)
                {
                    const Motion *uniform_motion = g.of(tag.uniform);
                    if (!uniform_motion || !moves(*uniform_motion))
                    {
                        if (slot.pristine[k])
                            ++counters.draws_kept;
                        else
                            copy_draw(*in, *draw);
                        slot.pristine[k] = 1;
                        continue;
                    }
                }
                // NEW HOST (R20 wide_3d): 3D corners of a Hor+ view were mapped by
                // 429A40 with the widened view scalars; invert/reapply those.
                // NEW HOST (R23): the factors include the guard band (same mapping).
                const auto wide =
                    host_wide::widened_view(tags.view.wide_factor_x, tags.view.wide_factor_y, in->viewport);
                auto view = in->viewport;
                if (wide.active)
                {
                    view.scale_x = wide.widened.half_width;
                    view.scale_y = wide.widened.half_height;
                    view.center_x = wide.widened.center_x;
                    view.center_y = wide.widened.center_y;
                }
                const auto m = mapping_of(view, px, py);
                const auto scratch_capacity = moved.capacity();
                behind.assign(n, 0);
                known.assign(n, 0);
                transformed.assign(n, 0);
                moved.resize(n);
                counters.allocations += moved.capacity() != scratch_capacity;
                // Pass 1: transformed camera-space points; which lie behind the near plane.
                bool any_behind = false, any_transformed = false,
                     clippable = in->fvf == 0x2c4u && (in->type == 4 || in->type == 5 || in->type == 6);
                for (std::size_t i = 0; i < n; ++i)
                {
                    const auto group = tag.per_vertex ? tags.groups[tag.first + i] : tag.uniform;
                    const Motion *group_motion = g.of(group); // NEW HOST (R28): per-group motion
                    if (!group_motion)
                    {
                        clippable = false;
                        continue;
                    }
                    if (!moves(*group_motion))
                        continue; // captured bits kept (camera unchanged)
                    const auto c = unproject(in->vertices[i], m);
                    if (!c)
                    {
                        clippable = false;
                        continue;
                    } // RHW<=0 (rejected ring point) or degenerate mapping: as captured
                    moved[i] = apply(*group_motion, *c, g.to_world);
                    known[i] = 1;
                    transformed[i] = 1;
                    any_transformed = true;
                    if (moved[i][2] + 1.0 < w_near * (1.0 - near_tolerance))
                    {
                        behind[i] = 1;
                        any_behind = true;
                    }
                }
                if (!any_transformed)
                {
                    // No vertex moved (so none is behind): the base draw's bits.
                    if (slot.pristine[k])
                        ++counters.draws_kept;
                    else
                        copy_draw(*in, *draw);
                    slot.pristine[k] = 1;
                    continue;
                }
                // Triangles that cross the near plane are clipped when the whole
                // draw is expressible (valid WORD indices, complete list).
                if (any_behind && clippable)
                {
                    const std::size_t used = in->indexed ? in->indices.size() : n;
                    if (in->type == 4 && used % 3 != 0)
                        clippable = false;
                    for (const auto index : in->indices)
                        if (index >= n)
                        {
                            clippable = false;
                            break;
                        }
                    if (n > max_draw_vertices)
                        clippable = false;
                    // Untransformed vertices of the draw take part as they are.
                    for (std::size_t i = 0; clippable && i < n; ++i)
                    {
                        if (known[i])
                            continue;
                        const auto c = unproject(in->vertices[i], m);
                        if (!c)
                        {
                            clippable = false;
                            break;
                        }
                        moved[i] = *c;
                        known[i] = 1;
                    }
                }
                const bool clip = any_behind && clippable;
                // Output = the base draw with its transformed vertices projected (one pass).
                const auto vc = draw->vertices.capacity(), ic = draw->indices.capacity();
                draw->type = in->type;
                draw->fvf = in->fvf;
                draw->flags = in->flags;
                draw->indexed = in->indexed;
                draw->viewport = in->viewport;
                draw->indices.assign(in->indices.begin(), in->indices.end());
                draw->vertices.resize(n);
                counters.allocations += (draw->vertices.capacity() != vc) + (draw->indices.capacity() != ic);
                slot.pristine[k] = 0;
                std::int64_t projected = 0;
                for (std::size_t i = 0; i < n; ++i)
                {
                    draw->vertices[i] = in->vertices[i];
                    if (transformed[i])
                    {
                        project(draw->vertices[i], moved[i], w_min, m);
                        ++projected;
                    }
                }
                counters.vertices_copied += static_cast<std::int64_t>(n);
                counters.vertices_projected += projected;
                counters.bytes += static_cast<std::int64_t>(n * sizeof(retail::PolygonSubmissionVertex) +
                                                            in->indices.size() * sizeof(std::uint16_t));
                if (clip)
                {
                    const std::size_t before = draw->vertices.size() * sizeof(retail::PolygonSubmissionVertex) +
                                               draw->indices.size() * sizeof(std::uint16_t);
                    if (clip_near(*draw, in->vertices, w_near, m))
                    {
                        // NEW HOST (R30): keep the device event consistent with the rebuilt draw.
                        slot.events_retyped = true;
                        retype_draw_event(*slot.device, k);
                        sgn::host_profile::add("interp.nearclip_rebuilt_draw", 0);
                        const std::size_t after = draw->vertices.size() * sizeof(retail::PolygonSubmissionVertex) +
                                                  draw->indices.size() * sizeof(std::uint16_t);
                        commands->payload_bytes = commands->payload_bytes - before + after;
                        result.payload_bytes = result.payload_bytes - before + after;
                        sgn::host_profile::add("interp.near_clipped_draw", 0);
                    }
                }
            }
        }
        if (sgn::host_profile::enabled())
        {
            sgn::host_profile::sample("interp.build_allocations", counters.allocations);
            sgn::host_profile::sample("interp.build_allocations_r29_equivalent", counters.legacy_allocations);
            sgn::host_profile::sample("interp.build_bytes_copied", counters.bytes);
            sgn::host_profile::sample("interp.build_bytes_r29_equivalent", counters.legacy_bytes);
            sgn::host_profile::sample("interp.build_vertices_copied", counters.vertices_copied);
            sgn::host_profile::sample("interp.build_vertices_projected", counters.vertices_projected);
            sgn::host_profile::sample("interp.build_draws_kept", counters.draws_kept);
        }
        return result;
    }
    bool present(PresentGate *gate)
    {
        if (!wants())
            return false;
        auto *r = renderer.get();
        if (!r || r->occluded())
            return false;
        SGN_PROFILE_SCOPE("host.interp_present");
        try
        {
            // NEW HOST (R22): wait for the back buffer this Present will use BEFORE
            // sampling time and building the frame. R19 sampled at the display-
            // slot grant and then blocked inside the Present on that buffer's
            // previous retirement (measured 5.5 ms average, up to 15 ms at
            // 240 Hz windowed), so frames carried poses of irregular age: judder.
            r->wait_transient_present_buffer();
            // NEW HOST (R27): frame rate limiter wait, after the buffer and before
            // the time sample, so the pose matches the moment of this Present.
            if (gate && !gate->open())
                return false;
            // NEW HOST (R30): a vsync-paced present samples the state of its display
            // slot (pose_delay_seconds); 0 leaves the sample exactly as before.
            const double pose_delay = gate ? gate->pose_delay_seconds() : 0.0;
            const double at = pose_delay != 0.0 ? now() - pose_delay * double(counter.frequency()) : now();
            const auto pair = blend_at(at);
            if (host_frame_trace::active()) // NEW HOST (R34) diagnostics: the pose sample of this present
                host_frame_trace::interp_sample(
                    options, at, at - double(frontend.world().tick_clock.increment) * (1.0 + presentation_margin_ticks),
                    double(counter.frequency()), pair.t, pair.a ? &pair.a->tags->view : nullptr, pair.b->tags->view,
                    pair.a ? pair.a->tick : 0u, pair.b->tick, pose_delay);
            if (sgn::host_profile::enabled())
            {
                // NEW HOST (R30): content-time step between consecutive presents (ideally constant).
                if (last_pose_at)
                    sgn::host_profile::sample(
                        "interp.pose_step_us",
                        static_cast<std::int64_t>((at - *last_pose_at) * 1e6 / double(counter.frequency())));
                last_pose_at = at;
            }
            const double t = pair.t;
            if (sgn::host_profile::enabled())
            {
                const auto stamp = std::chrono::steady_clock::now();
                if (last_present)
                    sgn::host_profile::add(
                        "interp.present_interval",
                        std::chrono::duration_cast<std::chrono::nanoseconds>(stamp - *last_present).count());
                last_present = stamp;
                sgn::host_profile::mark(sgn::host_profile::Series::present); // NEW HOST (R30) frame-time series
            }
            const Frame *shown_frame = pair.b;
            const CapturedPolygonSubmission *shown = pair.b->submission.get(); // R30: no reference count traffic
            bool presented_gpu = false;                                        // NEW HOST (R33)
            if (pair.a && t < 1.0)
            {
                if (transforms(t, *pair.a, *pair.b, effect_motions))
                {
                    shown_frame = pair.a;
                    sgn::host_profile::add("interp.frame_interpolated", 0);
                    if (gpu_path && present_gpu(*r, *pair.a, effect_motions))
                    {
                        presented_gpu = true;
                        sgn::host_profile::add("interp.gpu_path_frames", 0);
                    }
                    else
                    {
                        shown = &build(*pair.a, effect_motions);
                        if (gpu_path)
                            sgn::host_profile::add("interp.cpu_fallback_frames", 0);
                    }
                }
                else
                    sgn::host_profile::add("interp.frame_cut_shown_as_B", 0);
            }
            else
            {
                sgn::host_profile::add(!pair.a               ? "interp.frame_first"
                                       : pair.b == &*current ? "interp.frame_at_N"
                                                             : "interp.frame_at_pair_end",
                                       0);
                // NEW HOST (R33b): the displayed time passed the newest canonical frame: the
                // source pass that produces the next one ended later than half a tick after its
                // due time, so this present cannot interpolate (and cannot take the GPU path).
                if (pair.a && pair.b == &*current)
                {
                    sgn::host_profile::add("interp.frame_late_source", 0);
                    report("host interp: frames shown uninterpolated: the next canonical frame arrived later than "
                           "half a tick after its due time (slow source pass)");
                }
            }
            if (presented_gpu)
            {
                // NEW HOST (R33): presented by the GPU re-projection above.
            }
            else if (shown_frame == pair.a)
            {
                // NEW HOST (R30): a presenter-built frame the renderer rejects (a
                // host re-projection fault) drops only THIS frame: the canonical
                // frame B is shown instead and interpolation stays on. A device
                // loss or a failing canonical frame keeps the handling below.
                try
                {
                    r->present_polygon_submission_transient(*shown, shown_frame->gamma, true);
                }
                catch (const GraphicsError &)
                {
                    throw;
                }
                catch (const std::exception &error)
                {
                    sgn::host_profile::add("interp.drop_rejected_interpolated_frame", 0);
                    report(std::string("host interp: dropped an interpolated frame the renderer rejected: ") +
                           error.what());
                    r->present_polygon_submission_transient(pair.b->submission, pair.b->gamma, true);
                }
            }
            else
                // Menu canvas is always fitted (source_menu_presentation_lifecycle.cpp). The
                // shared overload validates a repeated canonical capture only once (R30).
                r->present_polygon_submission_transient(shown_frame->submission, shown_frame->gamma, true);
            waiting = false;
            return true;
        }
        catch (const GraphicsError &error)
        {
            // The source path owns device-loss recovery: hand presentation back.
            failure = error.what();
            deactivate();
            if (!error.device_lost())
                fail(error.what());
            return false;
        }
        catch (const std::exception &error)
        {
            fail(error.what());
            return false;
        }
    }
    // NEW HOST (R30). Inactive: the scratch keeps only its buffers, no capture, identity, texture
    // bank or GPU command reference (history and scratch are released together,
    // e.g. before a device-loss recovery or a scene's texture epoch ends).
    void release_scratch_references() noexcept
    {
        scratch_source.reset();
        scratch.device.reset();
        for (auto &segment : scratch.segments)
            segment.textures.reset();
        for (auto &slot : scratch_segments)
        {
            if (slot.device)
                slot.device->device.reset();
            if (slot.commands)
                slot.commands->gpu_insertions.clear();
        }
    }
    bool shown_active() const noexcept
    {
        return active && usable();
    }
    // NEW HOST (R30): presenter state transitions for the profiler report and the
    // end of a present run (no interval across an inactive period); the scratch
    // releases its base capture. Called by the public wrappers only, so the
    // decisions inside observe()/present() are untouched.
    void after_call(bool was_active, const char *where) noexcept
    {
        const bool now_active = shown_active();
        if (was_active == now_active)
            return;
        if (!now_active)
        {
            last_pose_at.reset();
            release_scratch_references();
            sgn::host_profile::mark_break(sgn::host_profile::Series::present);
        }
        if (!sgn::host_profile::enabled())
            return;
        if (now_active)
        {
            sgn::host_profile::transition("interp.activated", where);
            return;
        }
        char reason[160];
        if (failed)
            std::snprintf(reason, sizeof reason, "%s: failed: %s", where, failure.c_str());
        else if (!enabled)
            std::snprintf(reason, sizeof reason, "%s: disabled by the graphics settings", where);
        else if (frontend.world().scene_mode != 1)
            std::snprintf(reason, sizeof reason, "%s: left the race scene (scene_mode %u)", where,
                          unsigned(frontend.world().scene_mode));
        else if (std::strcmp(where, "present") == 0 && !failure.empty())
            std::snprintf(reason, sizeof reason, "%s: graphics error: %s", where, failure.c_str());
        else if (last_rejection)
            std::snprintf(reason, sizeof reason, "%s: %s", where, last_rejection);
        else
            std::snprintf(reason, sizeof reason, "%s: no interpolatable canonical frame", where);
        sgn::host_profile::transition("interp.deactivated", reason);
    }
};

InterpolatingPresenter::InterpolatingPresenter(retail::FrontendState &frontend, SourceMenuPresentationBackend &backend,
                                               const std::unique_ptr<Renderer> &renderer,
                                               retail::FrontendPerformanceCounter &counter, PresenterOptions options)
    : impl_(std::make_unique<Impl>(frontend, backend, renderer, counter, options))
{
}
InterpolatingPresenter::~InterpolatingPresenter()
{
    set_collection_enabled(false);
    host_wide::set_race_guard_band(0.0);
}
void InterpolatingPresenter::observe_completed_frames()
{
    const bool was_active = impl_->shown_active();
    impl_->observe();
    impl_->after_call(was_active, "observe");
}
bool InterpolatingPresenter::active() const noexcept
{
    return impl_->active && impl_->usable();
}
bool InterpolatingPresenter::frame_waiting() const noexcept
{
    return active() && impl_->waiting;
}
bool InterpolatingPresenter::wants_present() const noexcept
{
    return impl_->wants();
}
bool InterpolatingPresenter::present(PresentGate *gate)
{
    const bool was_active = impl_->shown_active();
    const bool presented = impl_->present(gate);
    impl_->after_call(was_active, "present");
    return presented;
}
void InterpolatingPresenter::deactivate() noexcept
{
    const bool was_active = impl_->shown_active();
    impl_->deactivate();
    impl_->after_call(was_active, "deactivate()");
}
bool InterpolatingPresenter::enabled() const noexcept
{
    return impl_->usable();
}
const std::string &InterpolatingPresenter::last_failure() const noexcept
{
    return impl_->failure;
}
} // namespace sgn::host_presentation
