// NEW HOST rendering (gpu_world R19). 433F40 side of the hybrid track path:
// per-view override publication and the TrackRun replacement of the corner
// loop / draw payload. The route state sequence stays in 433F40 itself.
#include "sgn/detail/gpu_world_ticket.hpp"
#include "sgn/assets.hpp"
#include "sgn/format_require.hpp"
#include "sgn/race_session.hpp"
#include "sgn/host_wide_race_view.hpp"
#include "sgn/retail_polygon_submission.hpp"
#include <bit>
#include <cstring>
#include <limits>

namespace sgn::gpu_world
{
namespace
{
using W = std::uint32_t;
using detail::Ticket;
using detail::ticket;
using format_check::require;
void publish(Ticket &t, retail::RaceSessionState &state)
{
    auto data = std::make_shared<detail::ViewData>(t.pending);
    const auto &v = state.viewport; // same four scalars 429A46..429A5A captured
    data->constants[20] = std::bit_cast<W>(v.scale_x);
    data->constants[21] = std::bit_cast<W>(v.scale_y);
    data->constants[22] = std::bit_cast<W>(v.center_x);
    data->constants[23] = std::bit_cast<W>(v.center_y);
    if (const auto *wide = host_wide::race_view())
    {
        // NEW HOST (R20 wide_3d) Hor+: the same widened scalars as 429A40
        // (source_world_polygon_clipping.cpp) inside the race 3D stages.
        data->constants[20] = std::bit_cast<W>(wide->widened.half_width);
        data->constants[21] = std::bit_cast<W>(wide->widened.half_height);
        data->constants[22] = std::bit_cast<W>(wide->widened.center_x);
        data->constants[23] = std::bit_cast<W>(wide->widened.center_y);
    }
    const auto &transformed = state.polygon_frame.transformed.allocation;
    const auto slots = transformed.view();
    const auto n = t.mesh->positions.size();
    require(t.region_start <= slots.size() && n <= slots.size() - t.region_start,
            "gpu_world track region left the transformed storage");
    // NEW repair (PORT-FIX-014): the same written-slot marks as 429880/429730,
    // so a never-written zero slot is computed by the shader, as the CPU path
    // now transforms it, instead of passing its zero XYZRHW as an override.
    const auto &allocation = transformed.source_binding().allocation;
    const auto &writes = state.polygon_frame.track_vertex_writes;
    data->overrides.assign(n, {0u, 0u, 0u, 0u});
    for (std::size_t i = 0; i < n; ++i)
    {
        const auto &words = slots[t.region_start + i].words;
        // 429774/4298CC compare the WORD at +18 with the whole 61C508 value.
        if ((words[4] >> 16) == t.stamp && writes.written(allocation, t.region_start + i))
            data->overrides[i] = {words[0], words[1], words[7], 1u};
    }
    t.published = std::move(data);
}
Ticket *admitted(const std::shared_ptr<retail::RaceSessionPlatform> &platform,
                 const retail::PolygonSubmissionSink &sink)
{
    auto &t = ticket();
    if (!t.active)
        return nullptr;
    require(t.thread == std::this_thread::get_id(), "gpu_world ticket used outside its owner thread");
    require(platform && t.platform.lock() == platform, "gpu_world ticket belongs to another platform");
    auto &state = platform->state();
    require(&state.polygon_frame == t.frame, "gpu_world ticket belongs to another polygon frame");
    // 42E8B1..42E8BC increments 61C508 once between 42ACF2 and 433F40.
    require(state.polygon_frame.frame_stamp == ((t.stamp + 1u) & 0xffffu),
            "gpu_world ticket is not the world view submitted by this 433F40");
    require(detail::attached_to(sink), "gpu_world insertion target is not the device behind this 433F40 sink");
    if (!t.published)
        publish(t, state);
    return &t;
}
void motion(detail::WorldDraw &d, const void *uv, bool reached)
{
    if (!reached)
        return; // no corner reached: the reference never reads motion
    require(uv != nullptr, "source submission lacks its captured motion pointer");
    std::memcpy(&d.motion_u_bits, uv, 4);
    std::memcpy(&d.motion_v_bits, static_cast<const std::uint8_t *>(uv) + 4, 4);
}
} // namespace

TrackRun::TrackRun(std::shared_ptr<const detail::StaticMesh> mesh, std::shared_ptr<const detail::ViewData> view,
                   std::uint32_t coordinates, std::uint32_t vertices)
    : mesh_(std::move(mesh)), view_(std::move(view)), coordinates_token_(coordinates), vertices_token_(vertices)
{
}

std::optional<TrackRun> TrackRun::open_inside_opaque(const std::shared_ptr<retail::RaceSessionPlatform> &platform,
                                                     const retail::PolygonSubmissionSink &sink)
{
    const auto *t = admitted(platform, sink);
    if (!t)
        return std::nullopt;
    return TrackRun(t->mesh, t->published, t->coordinates_token, t->vertices_token);
}
std::optional<TrackRun> TrackRun::open_inside_alpha(const std::shared_ptr<retail::RaceSessionPlatform> &platform,
                                                    const retail::PolygonSubmissionSink &sink,
                                                    std::uint32_t vertices_word)
{
    const auto &current = ticket();
    // Plain word compare first: other models' packets never touch the ticket.
    if (!current.active || vertices_word != current.vertices_token)
        return std::nullopt;
    const auto *t = admitted(platform, sink);
    if (!t)
        return std::nullopt;
    return TrackRun(t->mesh, t->published, t->coordinates_token, t->vertices_token);
}
void TrackRun::admit_packet(std::uint32_t attributes_word, std::uint32_t vertices_word) const
{
    require(attributes_word == coordinates_token_ && vertices_word == vertices_token_,
            "gpu_world packet is not a 42A090 track-base packet of this view");
}
void TrackRun::corner(std::size_t local_vertex, std::uint16_t coordinate)
{
    require(coordinate < mesh_->coordinates.size(), "gpu_world corner names a coordinate outside the track base part");
    if (local_vertex >= local_.size())
        local_.resize(local_vertex + 1);
    local_[local_vertex] = coordinate;
}
namespace
{
// The reference local vertex j of this draw: coordinate id and, when the CPU
// already produced its XYZRHW this frame, those exact words.
std::array<W, 4> local_entry(const detail::StaticMesh &mesh, const detail::ViewData &view, W coordinate)
{
    const auto vertex = mesh.coordinates[coordinate][2];
    const auto &o = view.overrides[vertex];
    return o[3] ? std::array<W, 4>{coordinate | 0x80000000u, o[0], o[1], o[2]} : std::array<W, 4>{coordinate, 0, 0, 0};
}
} // namespace
void TrackRun::flush_indexed(std::span<const std::uint16_t> local_indices, std::size_t local_vertices,
                             const void *motion_uv, const retail::ViewportState &viewport)
{
    require(local_vertices <= local_.size(), "gpu_world draw names local vertices without corners");
    // PolygonDeviceContext acceptance for draw_indexed(4,...): the reference
    // refuses (and poisons the capture) otherwise; an insertion cannot be
    // refused, so the same frame fails here instead (never valid source data).
    require(local_vertices <= 65536, "polygon draw exceeds WORD vertex domain");
    require(local_vertices >= 3 && local_indices.size() >= 3 && local_indices.size() % 3 == 0,
            "gpu_world draw refused: draw_layout (type 4 needs >=3 vertices and a multiple of 3 indices)");
    for (const auto index : local_indices)
        require(index < local_vertices, "gpu_world draw refused: draw_index (index outside submitted vertices)");
    detail::WorldDraw d;
    d.type = 4;
    d.viewport = viewport;
    d.view = view_->constants;
    d.locals.reserve(local_vertices);
    for (std::size_t j = 0; j < local_vertices; ++j)
        d.locals.push_back(local_entry(*mesh_, *view_, local_[j]));
    d.indices.assign(local_indices.begin(), local_indices.end());
    motion(d, motion_uv, true);
    d.color_and = 0xffffffffu;
    d.color_or = 0xff000000u; // 433612 OR EAX,0FF000000h; specular word 0 (433620)
    detail::insert_world_draw(mesh_, std::move(d));
}
void TrackRun::flush_alpha(std::uint32_t count, const void *motion_uv, std::uint32_t mode, std::uint32_t alpha_word,
                           const retail::ViewportState &viewport)
{
    // PolygonDeviceContext acceptance for draw(5,...): >=3 vertices.
    require(count >= 3, "gpu_world draw refused: draw_layout (type 5 needs >=3 vertices)");
    require(count <= local_.size(), "gpu_world alpha draw names corners that were not recorded");
    detail::WorldDraw d;
    d.type = 5;
    d.viewport = viewport;
    d.view = view_->constants;
    d.locals.reserve(count);
    for (W j = 0; j < count; ++j)
        d.locals.push_back(local_entry(*mesh_, *view_, local_[j])); // 477CD0 order
    motion(d, motion_uv, true);
    if (mode == 0x50)
    {
        d.color_and = 0xffffffffu;
        d.color_or = 0;
    } // 433B3B -> 433B4D
    else
    {
        d.color_and = 0x00ffffffu;
        d.color_or = alpha_word;
    } // 433B40..433B46
    detail::insert_world_draw(mesh_, std::move(d));
}
} // namespace sgn::gpu_world
