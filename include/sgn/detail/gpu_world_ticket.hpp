#pragma once
// NEW HOST rendering (gpu_world R19). Internal types shared by the 42ACF2 side
// (gpu_world_track.cpp), the 433F40 side (gpu_world_submission.cpp) and the
// renderer side (gpu_world_d3d12.cpp). Host presentation state, owner thread.
#include "sgn/gpu_world.hpp"
#include <thread>

namespace sgn::gpu_world::detail
{
// Immutable per-level geometry of one PMD part, parsed once from the loaded PMD.
// positions: part+0x30 records (x,y,z; w forced 0). coordinates: part+0x2C
// records as {u bits, v bits, vertex WORD+8, colour DWORD+0C}.
struct StaticMesh
{
    std::uint64_t serial{};
    std::vector<std::array<float, 4>> positions;
    std::vector<std::array<std::uint32_t, 4>> coordinates;
};
// One world view. constants (binary32 words): [0..15] 61C428 model-view after
// 44FBD0, [16..19] px,py,pz,offset (61C3E8/3FC/410/420), [20..23] scale_x,
// scale_y,center_x,center_y (429A46..429A5A). overrides[v] = {x,y,rhw,1} when
// the reference transformed+projected vertex v this frame (stamp == 61C508:
// 429880, or a stamp coincidence); {0,0,0,0} lets the shader compute it.
struct ViewData
{
    std::array<std::uint32_t, 24> constants{};
    std::vector<std::array<std::uint32_t, 4>> overrides;
};
struct Ticket
{
    bool active{};
    std::thread::id thread;
    std::weak_ptr<retail::RaceSessionPlatform> platform;
    const retail::WorldPolygonFrame *frame{};
    std::uint32_t stamp{}, region_start{}, vertices_token{}, coordinates_token{};
    std::shared_ptr<const StaticMesh> mesh;
    ViewData pending;
    std::shared_ptr<const ViewData> published;
};
Ticket &ticket() noexcept;

// One replacement draw, fully self-contained (no per-view GPU state): the
// reference local vertex table (exactly the vertices 433F40 would have built)
// as {coordinate id | override<<31, x, y, rhw} and its WORD indices.
struct WorldDraw
{
    std::uint32_t type{}, flags{0x1c};
    retail::ViewportState viewport;
    std::vector<std::array<std::uint32_t, 4>> locals;
    std::vector<std::uint16_t> indices; // type 4 only; type 5 draws locals 0..n-1
    std::array<std::uint32_t, 24> view{};
    std::uint32_t motion_u_bits{}, motion_v_bits{}, color_and{0xffffffffu}, color_or{};
};
// Renderer side (gpu_world_d3d12.cpp).
bool attached_to(const retail::PolygonSubmissionSink &) noexcept;
void insert_world_draw(const std::shared_ptr<const StaticMesh> &, WorldDraw &&);
void set_mode_listener(void (*)(Mode)) noexcept;
void set_backend_available(bool) noexcept;
} // namespace sgn::gpu_world::detail
