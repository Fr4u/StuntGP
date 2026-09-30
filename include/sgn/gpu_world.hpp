#pragma once
// NEW HOST rendering (gpu_world R19). Nothing in this header is recovered game
// behaviour. Mode 0 (default) leaves the reconstructed CPU world path
// completely unchanged; mode 1 draws the track base (42AC70) inside-list
// polygons from static GPU buffers with a vertex shader that reproduces the
// reference arithmetic (see source-work/subagents/gpu-world-r19/DESIGN.md).
// Renderer side uses only perf_gpu's R19 API: Renderer::create_static_buffer,
// PolygonDeviceContext::insert_gpu_command (planned insertion), the transient
// arena and Renderer::set_polygon_precise_vertex_mapping.
#include "sgn/retail_viewport.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace sgn
{
class Renderer;
class PolygonDeviceContext;
} // namespace sgn
namespace sgn::retail
{
class RaceSessionPlatform;
class SourceAllocationBank;
struct WorldPolygonFrame;
struct ProjectionParameters;
struct PolygonSubmissionSink;
} // namespace sgn::retail

namespace sgn::gpu_world
{
enum class Mode : std::uint32_t
{
    cpu_reference = 0,
    hybrid_exact = 1
};
// SGN_GPU_WORLD=1 selects hybrid_exact; anything else (or unset) is cpu_reference.
// set_requested_mode() is the A/B switch for a host key; it takes effect at the
// next 42ACF2 (one decision per world view) and also switches the attached
// renderer's precise polygon mapping (on exactly in hybrid_exact).
Mode requested_mode() noexcept;
void set_requested_mode(Mode);
// True only while attach() succeeded (double-precision VS pipeline created).
bool backend_available() noexcept;

// Composition hooks (root). attach(): after SourceMenuPresentationBackend::bind
// with the SAME renderer/device context; forwarding_sink is the sink 433F40
// receives (presentation.sink()), used to prove insertions land in the stream
// that sink forwards to. detach(): before the renderer is destroyed/replaced
// (device recreation, shutdown). Neither touches any source state.
void attach(Renderer &, PolygonDeviceContext &, const retail::PolygonSubmissionSink &forwarding_sink);
void detach() noexcept;

// Hook at 42ACF2 (produce_base_prefix_42ac70, after 42ACED/429880). Returns
// true when this view's track-base inside polygons will be drawn by the GPU;
// the caller then skips 429730. Returns false (and clears any ticket) for the
// CPU reference path. Only plain reads; no reference state is written.
bool begin_track_frame_42acf2(retail::SourceAllocationBank &, retail::WorldPolygonFrame &,
                              const retail::ProjectionParameters &);

namespace detail
{
struct StaticMesh;
struct ViewData;
} // namespace detail
// Per 433F40 route invocation replacement of the corner loop and the draw.
class TrackRun
{
    std::shared_ptr<const detail::StaticMesh> mesh_;
    std::shared_ptr<const detail::ViewData> view_;
    std::uint32_t coordinates_token_{}, vertices_token_{};
    std::vector<std::uint32_t> local_;
    TrackRun(std::shared_ptr<const detail::StaticMesh>, std::shared_ptr<const detail::ViewData>, std::uint32_t,
             std::uint32_t);

  public:
    // group(Route::opaque,1,2) = 6243E0 list of 42A090 track packets.
    static std::optional<TrackRun> open_inside_opaque(const std::shared_ptr<retail::RaceSessionPlatform> &,
                                                      const retail::PolygonSubmissionSink &);
    // One packet of the inside-alpha list 6243DC (second alpha() call only).
    // Engaged iff this view has a ticket and packet word3 is the track region.
    static std::optional<TrackRun> open_inside_alpha(const std::shared_ptr<retail::RaceSessionPlatform> &,
                                                     const retail::PolygonSubmissionSink &,
                                                     std::uint32_t vertices_word);
    // Packet admission: word2/word3 must be the ticket's part+2C / region.
    void admit_packet(std::uint32_t attributes_word, std::uint32_t vertices_word) const;
    // Replaces corner(): local output slot -> coordinate id (in.index(...)).
    void corner(std::size_t local_vertex, std::uint16_t coordinate);
    // Replaces draw_indexed(4,0x2C4,vertices,indices,0x1C,viewport): planned
    // insertion {4,0x1C,viewport} at the same stream position.
    void flush_indexed(std::span<const std::uint16_t> local_indices, std::size_t local_vertices, const void *motion_uv,
                       const retail::ViewportState &);
    // Replaces draw(5,0x2C4,vertices(count),0x1C,viewport) of 433B7C.
    void flush_alpha(std::uint32_t count, const void *motion_uv, std::uint32_t mode, std::uint32_t alpha_word,
                     const retail::ViewportState &);
};

// CPU evaluation of the shader formula for tests/diagnostics: 429730 per-vertex
// math (== perf_world detail::world_track_vertex_inside_42979a) then 429A40,
// binary64 intermediates and binary32 stores. model_view = 61C428 words,
// projection = px,py bits, viewport = scale_x,scale_y,center_x,center_y bits.
// Returns {x,y,rhw} binary32 words.
std::array<std::uint32_t, 3> reference_project(const std::array<std::uint32_t, 16> &model_view,
                                               const std::array<std::uint32_t, 2> &projection,
                                               const std::array<std::uint32_t, 4> &viewport,
                                               const std::array<float, 4> &position) noexcept;
} // namespace sgn::gpu_world
