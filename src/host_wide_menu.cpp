#include "sgn/host_wide_menu.hpp"
#include "sgn/polygon_device_context.hpp"
#include "sgn/host_profiler.hpp"
#include "sgn/host_wide_layout.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

// NEW HOST widescreen frontend presentation (wide_menu, R20); see the header.
// Plain bookkeeping and arithmetic on a copy of the sealed capture. No source
// function is called and no source owner is read or written from here.
namespace sgn::host_wide_menu
{
namespace
{
// Menu logical canvas of the source: 432E00 maps x,y by (x-320)*ui_scale_x +
// center_x and (y-240)*ui_scale_y + center_y; 43F6B0 "full" is 0,0,640,480.
constexpr double menu_width = 640.0, menu_height = 480.0, menu_center_x = 320.0, menu_center_y = 240.0;
// 438830: u = (x*0.015625 - scroll)*1024, v likewise; 432C00 then scales by
// 1/1024. One texture period per 64 menu units in both axes.
constexpr double tiled_period = 64.0;
// A 438830 quad is clipped by 438370/4251A0 to the canvas edge; a rectangle
// edge lying within this many canvas pixels of it "reaches" the edge.
constexpr double edge_tolerance = 0.5;
constexpr std::size_t queued_capacity = 128;

struct Queued
{
    std::uint32_t id{};
    Kind kind{Kind::none};
};
struct Collector
{
    Kind queue_kind{Kind::none};
    Kind consume_kind{Kind::none};
    std::array<Queued, queued_capacity> queued{};
    std::size_t queued_count{};
    bool building{};
    std::shared_ptr<FrameTags> frame; // R30: shared (not unique) so its storage can be recycled
    // Recipe association ring (remember/presentation_copy).
    struct Remembered
    {
        const CapturedPolygonSubmission *key{};
        std::weak_ptr<const CapturedPolygonSubmission> alive;
        ImmutableFrameTags tags;
    };
    std::array<Remembered, 6> remembered{};
    std::size_t next_remembered{};
    // NEW HOST (R30): storage recycled across frames (see Recycler and remember()).
    std::shared_ptr<const FrameTags> spare_tags;
};
// NEW HOST (R30): the widened presentation copy of a menu frame reuses the objects (and the vector
// capacities: commands, per-draw vertices/indices) of an earlier copy that nobody holds any more, by copy
// ASSIGNMENT from the same source instead of copy construction. The value of the copy is the same either
// way, so the rasterized frame is identical; only the per-frame allocations go. An object is reused only
// while this pool holds its only reference (use_count() == 1): the renderer reads a copy synchronously
// during staging, and a copy still held elsewhere (both roles of a restore) is never touched. An idle
// object keeps no reference to anything else (scrub_idle: texture banks, device frames, device
// identities, GPU insertions are dropped at the next begin_frame and by release_recycled_storage), so the
// pool never extends the lifetime of a GPU resource or of a sealed frame. Owner (source draw) thread
// only, like the collector.
struct Recycler
{
    std::array<std::shared_ptr<CapturedPolygonSubmission>, 4> submissions;
    std::array<std::shared_ptr<CapturedPolygonDeviceFrame>, 16> devices;
    std::array<std::shared_ptr<CapturedPolygonFrame>, 16> frames;
    std::size_t rotate{};
};
Recycler &recycler() noexcept
{
    static Recycler state; // owner (source draw) thread only
    return state;
}
// Drops every reference an idle pooled object holds; the command list keeps its capacity (its commands
// are plain values: states, texture tokens, vertices, indices).
void scrub_idle(Recycler &pool) noexcept
{
    for (auto &held : pool.submissions)
        if (held && held.use_count() == 1)
            *held = CapturedPolygonSubmission{};
    for (auto &held : pool.devices)
        if (held && held.use_count() == 1)
            *held = CapturedPolygonDeviceFrame{};
    for (auto &held : pool.frames)
        if (held && held.use_count() == 1)
            held->gpu_insertions.clear();
}
template <class T, std::size_t N>
std::shared_ptr<T> recycled_copy(std::array<std::shared_ptr<T>, N> &pool, std::size_t &rotate, const T &source)
{
    for (auto &held : pool)
        if (held && held.use_count() == 1)
        {
            *held = source;
            return held;
        }
    auto fresh = std::make_shared<T>(source);
    for (auto &held : pool)
        if (!held)
        {
            held = fresh;
            return fresh;
        }
    pool[rotate++ % N] = fresh; // all in use: keep the newest instead of the oldest
    return fresh;
}
Collector &collector() noexcept
{
    static Collector state; // owner (source draw) thread only
    return state;
}
void spoil(Collector &c) noexcept
{
    if (c.frame)
        c.frame->consistent = false;
}

using sgn::bit_reinterpret::as_float;
std::uint32_t as_word(double v) noexcept
{
    return std::bit_cast<std::uint32_t>(static_cast<float>(v));
}
bool finite_positive(double v) noexcept
{
    return std::isfinite(v) && v > 0.0;
}

// Words of an FVF 0x2C4 vertex: 0 X, 1 Y, 2 Z, 3 RHW, 4 diffuse, 5 specular, 6/7 stage-0 UV.
constexpr unsigned word_x = 0, word_y = 1, word_u = 6, word_v = 7;

bool admissible(const CapturedPolygonDraw &d) noexcept
{
    // Exactly the 432E00 form: one FVF 0x2C4 four-vertex fan, flags 0x14.
    if (d.type != 6 || d.fvf != 0x2c4u || d.indexed || !d.indices.empty() || d.vertices.size() != 4 || d.flags != 0x14u)
        return false;
    const auto &v = d.viewport;
    if (!finite_positive(v.ui_scale_x) || !finite_positive(v.ui_scale_y) || !std::isfinite(v.center_x) ||
        !std::isfinite(v.center_y))
        return false;
    if (v.full_width <= 0 || v.full_height <= 0)
        return false;
    for (const auto &vertex : d.vertices)
        for (const unsigned w : {word_x, word_y, word_u, word_v})
            if (!std::isfinite(as_float(vertex.words[w])))
                return false;
    return true;
}

// Moves the vertices lying on the quad's low (or high) edge of one axis
// outward by `reach` canvas pixels and continues their texture coordinate
// with the exact 438830 slope (1/64 per menu unit = 1/(64*ui_scale) per pixel),
// in the direction the quad's own coordinates run.
void extend_tiled_axis(CapturedPolygonDraw &d, unsigned position, unsigned texture, double scale, double canvas_low,
                       double canvas_high, double reach_low, double reach_high)
{
    auto &vs = d.vertices;
    float low = as_float(vs[0].words[position]), high = low;
    for (const auto &v : vs)
    {
        const auto p = as_float(v.words[position]);
        low = std::min(low, p);
        high = std::max(high, p);
    }
    if (!(high > low))
        return;
    // Texture direction along this axis from the quad itself (438830: increasing).
    double t_low = 0, t_high = 0;
    for (const auto &v : vs)
    {
        if (as_float(v.words[position]) == low)
            t_low = as_float(v.words[texture]);
        if (as_float(v.words[position]) == high)
            t_high = as_float(v.words[texture]);
    }
    const double direction = t_high >= t_low ? 1.0 : -1.0;
    const double slope = direction / (tiled_period * scale); // texture units per canvas pixel
    const bool at_low = reach_low > 0 && double(low) <= canvas_low + edge_tolerance;
    const bool at_high = reach_high > 0 && double(high) >= canvas_high - edge_tolerance;
    for (auto &v : vs)
    {
        const auto p = as_float(v.words[position]);
        double delta = 0;
        if (at_low && p == low)
            delta = -reach_low;
        else if (at_high && p == high)
            delta = reach_high;
        else
            continue;
        v.words[position] = as_word(double(p) + delta);
        v.words[texture] = as_word(double(as_float(v.words[texture])) + delta * slope);
    }
}
void extend_tiled(CapturedPolygonDraw &d, const Extension &e)
{
    const auto &vp = d.viewport;
    const double sx = vp.ui_scale_x, sy = vp.ui_scale_y;
    const double left = (0.0 - menu_center_x) * sx + vp.center_x,
                 right = (menu_width - menu_center_x) * sx + vp.center_x;
    const double top = (0.0 - menu_center_y) * sy + vp.center_y,
                 bottom = (menu_height - menu_center_y) * sy + vp.center_y;
    extend_tiled_axis(d, word_x, word_u, sx, left, right, e.left * sx, e.right * sx);
    extend_tiled_axis(d, word_y, word_v, sy, top, bottom, e.top * sy, e.bottom * sy);
}
// 4428A0 full-screen overlay: canvas-edge vertices follow the output edges;
// the constant texel region and the fade alpha are unchanged.
void extend_fill(CapturedPolygonDraw &d, const Extension &e)
{
    const auto &vp = d.viewport;
    const double sx = vp.ui_scale_x, sy = vp.ui_scale_y;
    const double left = (0.0 - menu_center_x) * sx + vp.center_x,
                 right = (menu_width - menu_center_x) * sx + vp.center_x;
    const double top = (0.0 - menu_center_y) * sy + vp.center_y,
                 bottom = (menu_height - menu_center_y) * sy + vp.center_y;
    for (auto &v : d.vertices)
    {
        const double x = as_float(v.words[word_x]), y = as_float(v.words[word_y]);
        if (x <= left + edge_tolerance)
            v.words[word_x] = as_word(x - e.left * sx);
        else if (x >= right - edge_tolerance)
            v.words[word_x] = as_word(x + e.right * sx);
        if (y <= top + edge_tolerance)
            v.words[word_y] = as_word(y - e.top * sy);
        else if (y >= bottom - edge_tolerance)
            v.words[word_y] = as_word(y + e.bottom * sy);
    }
}
// 439450/439520 on the full canvas: corners, edges and middle slices follow
// the output edges. Texture coordinates are unchanged (the source stretches
// the 8-texel middle slices to any rectangle length).
void extend_screen_frame(CapturedPolygonDraw &d, const Extension &e)
{
    const auto &vp = d.viewport;
    const double cx = vp.center_x, cy = vp.center_y; // menu (320,240)
    const double sx = vp.ui_scale_x, sy = vp.ui_scale_y;
    for (auto &v : d.vertices)
    {
        const double x = as_float(v.words[word_x]), y = as_float(v.words[word_y]);
        if (x < cx)
            v.words[word_x] = as_word(x - e.left * sx);
        else if (x > cx)
            v.words[word_x] = as_word(x + e.right * sx);
        if (y < cy)
            v.words[word_y] = as_word(y - e.top * sy);
        else if (y > cy)
            v.words[word_y] = as_word(y + e.bottom * sy);
    }
}
std::size_t draw_count(const CapturedPolygonFrame &frame) noexcept
{
    std::size_t n = 0;
    for (const auto &command : frame.commands)
        if (std::holds_alternative<CapturedPolygonDraw>(command))
            ++n;
    return n;
}
} // namespace

QueueScope::QueueScope(Kind kind) noexcept : previous_(collector().queue_kind)
{
    collector().queue_kind = kind;
}
QueueScope::~QueueScope()
{
    collector().queue_kind = previous_;
}
void queued_command(std::uint32_t id) noexcept
{
    auto &c = collector();
    if (c.queue_kind == Kind::none || id == 0)
        return;
    if (c.queued_count >= c.queued.size())
    {
        spoil(c);
        return;
    }
    c.queued[c.queued_count++] = {id, c.queue_kind};
}
ConsumeScope::ConsumeScope(std::uint32_t id) noexcept : previous_(collector().consume_kind)
{
    auto &c = collector();
    Kind kind = Kind::none;
    for (std::size_t i = 0; i < c.queued_count; ++i)
        if (c.queued[i].id == id)
        {
            kind = c.queued[i].kind;
            break;
        }
    c.consume_kind = kind;
}
ConsumeScope::~ConsumeScope()
{
    collector().consume_kind = previous_;
}
void consumed_all() noexcept
{
    collector().queued_count = 0;
}
DrawScope::DrawScope(Kind kind) noexcept : previous_(collector().consume_kind)
{
    collector().consume_kind = kind;
}
DrawScope::~DrawScope()
{
    collector().consume_kind = previous_;
}

void begin_frame() noexcept
{
    auto &c = collector();
    scrub_idle(recycler()); // R30: no idle copy outlives its frame's resources
    c.building = false;
    c.frame.reset();
    c.queued_count = 0;
    c.consume_kind = Kind::none;
    try
    {
        // R30: the tags of a frame that left the recipe ring (nobody else holds them) are reused with their
        // draw-list capacity; the fields are reset to a default FrameTags.
        if (c.spare_tags && c.spare_tags.use_count() == 1)
        {
            auto &tags = const_cast<FrameTags &>(*c.spare_tags); // created non-const by this collector
            tags.consistent = true;
            tags.tagged = 0;
            tags.draws.clear();
            c.frame = std::shared_ptr<FrameTags>(std::move(c.spare_tags), &tags);
        }
        else
        {
            c.spare_tags.reset();
            c.frame = std::make_shared<FrameTags>();
        }
    }
    catch (...)
    {
        return;
    } // not building: presented as captured
    c.building = true;
}
void note_draw(std::size_t vertices, bool accepted) noexcept
{
    auto &c = collector();
    if (!c.building || !accepted)
        return; // a refused draw appends no capture command
    auto &f = *c.frame;
    try
    {
        f.draws.push_back(c.consume_kind);
        if (c.consume_kind != Kind::none)
        {
            ++f.tagged;
            if (vertices != 4)
                f.consistent = false;
        }
    }
    catch (...)
    {
        spoil(c);
    }
}
ImmutableFrameTags end_frame() noexcept
{
    auto &c = collector();
    if (!c.building)
        return {};
    c.building = false;
    if (!c.frame->tagged)
    {
        c.spare_tags = std::move(c.frame); // R30: untagged (race) frames keep their storage too
        return {};
    }
    return ImmutableFrameTags(std::move(c.frame)); // R30: no new control block
}
void abandon_frame() noexcept
{
    auto &c = collector();
    c.building = false;
    c.frame.reset();
    c.queued_count = 0;
    c.consume_kind = Kind::none;
}

ImmutablePolygonSubmission widen_submission(const ImmutablePolygonSubmission &submission, const FrameTags *tags,
                                            const Extension &visible)
{
    if (!submission || !tags || !tags->consistent || !tags->tagged || visible.empty())
        return submission;
    for (const double side : {visible.left, visible.right, visible.top, visible.bottom})
        if (!std::isfinite(side) || side < 0)
            return submission;
    SGN_PROFILE_SCOPE("host.wide_menu");
    // Preflight: the tags must name exactly the captured draws, and every
    // tagged draw must be the 432E00 quad form. Otherwise present as captured.
    std::size_t ordinal = 0;
    for (const auto &segment : submission->segments)
    {
        if (!segment.device_frame || !segment.device_frame->polygon_commands)
            return submission;
        for (const auto &command : segment.device_frame->polygon_commands->commands)
        {
            const auto *draw = std::get_if<CapturedPolygonDraw>(&command);
            if (!draw)
                continue;
            if (ordinal >= tags->draws.size())
                return submission;
            if (tags->draws[ordinal++] != Kind::none && !admissible(*draw))
                return submission;
        }
    }
    if (ordinal != tags->draws.size())
        return submission;
    auto &pool = recycler();
    auto result = recycled_copy(pool.submissions, pool.rotate, *submission);
    ordinal = 0;
    for (auto &segment : result->segments)
    {
        const auto &source = *segment.device_frame->polygon_commands;
        const auto count = draw_count(source);
        const bool touched = std::any_of(tags->draws.begin() + static_cast<std::ptrdiff_t>(ordinal),
                                         tags->draws.begin() + static_cast<std::ptrdiff_t>(ordinal + count),
                                         [](Kind k) { return k != Kind::none; });
        if (!touched)
        {
            ordinal += count;
            continue;
        }
        auto device = recycled_copy(pool.devices, pool.rotate, *segment.device_frame);
        auto commands = recycled_copy(pool.frames, pool.rotate, source);
        for (auto &command : commands->commands)
        {
            auto *draw = std::get_if<CapturedPolygonDraw>(&command);
            if (!draw)
                continue;
            const auto kind = tags->draws[ordinal++];
            if (kind == Kind::tiled)
                extend_tiled(*draw, visible);
            else if (kind == Kind::screen_frame)
                extend_screen_frame(*draw, visible);
            else if (kind == Kind::fill)
                extend_fill(*draw, visible);
        }
        device->polygon_commands = std::move(commands);
        segment.device_frame = std::move(device);
    }
    return result;
}

void remember(const ImmutablePolygonSubmission &sealed, ImmutableFrameTags tags) noexcept
{
    if (!sealed || !tags)
        return;
    auto &c = collector();
    auto &slot = c.remembered[c.next_remembered];
    c.next_remembered = (c.next_remembered + 1) % c.remembered.size();
    auto dropped = std::move(slot.tags);
    slot.key = sealed.get();
    slot.alive = sealed;
    slot.tags = std::move(tags);
    if (dropped && dropped.use_count() == 1)
        c.spare_tags = std::move(dropped); // R30: storage for a later begin_frame
}
void release_recycled_storage() noexcept
{
    auto &pool = recycler();
    for (auto &held : pool.submissions)
        held.reset();
    for (auto &held : pool.devices)
        held.reset();
    for (auto &held : pool.frames)
        held.reset();
}
ImmutablePolygonSubmission presentation_copy(const CapturedPolygonSubmission *recipe) noexcept
{
    if (!recipe)
        return {};
    try
    {
        const auto layout = host_wide::current();
        if (!layout.wide())
            return {}; // 4:3 output or --aspect-4x3: rasterize the recipe itself
        auto &c = collector();
        for (const auto &entry : c.remembered)
        {
            if (entry.key != recipe || !entry.tags)
                continue;
            const auto sealed = entry.alive.lock();
            if (!sealed || sealed.get() != recipe)
                continue;
            auto copy = widen_submission(sealed, entry.tags.get(),
                                         {layout.extend_x, layout.extend_x, layout.extend_y, layout.extend_y});
            if (copy == sealed)
                return {};
            return copy;
        }
    }
    catch (...)
    {
    } // host presentation failure: the recipe is presented as captured
    return {};
}
// NEW HOST (widescreen R37): frontend clip at the real screen edges (see header).
namespace
{
// Owner (source menu) thread only, like the collector. Negative: no cap.
float &clip_limit() noexcept
{
    static float value = -1.0f;
    return value;
}
} // namespace
ClipLimitScope::ClipLimitScope(float max_extension) noexcept : previous_(clip_limit())
{
    clip_limit() = max_extension;
}
ClipLimitScope::~ClipLimitScope()
{
    clip_limit() = previous_;
}
bool widened_frontend_clip(const std::array<std::uint32_t, 4> &source_clip, FrontendClipBounds &out) noexcept
{
    // 438190 stores integer words; the full frame is exactly {0,0,640,480}.
    if (source_clip[0] != 0u || source_clip[1] != 0u || source_clip[2] != 640u || source_clip[3] != 480u)
        return false;
    const auto layout = host_wide::current();
    if (!layout.wide())
        return false; // 4:3 output or original_4x3: the source clip, bit for bit
    float ex = layout.extend_x, ey = layout.extend_y;
    if (!std::isfinite(ex) || !std::isfinite(ey) || ex < 0.0f || ey < 0.0f)
        return false;
    const float cap = clip_limit();
    if (cap >= 0.0f)
    {
        ex = std::min(ex, cap);
        ey = std::min(ey, cap);
    }
    out = {-ex, -ey, 640.0f + ex, 480.0f + ey};
    return true;
}
} // namespace sgn::host_wide_menu

