#pragma once
#include "sgn/polygon_submission.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

// NEW HOST (R33) GPU re-projection of interpolated frames. Not recovered
// behaviour. The presenter (host_presentation_presenter.cpp) describes, once
// per canonical base capture, which motion slot moves every captured vertex and
// the exact 429A40/41E410 screen mapping of every draw; per present it supplies
// only the motion of each slot. The polygon recorder (polygon_d3d12.cpp) keeps
// the base capture's plan, vertex payload and slot stream on the GPU and, per
// present, re-projects the moving draws in a vertex shader (un-project XYZRHW
// with the exact inverse mapping, apply the slot's affine motion, re-project to
// clip space with the true w, clip at the source near plane). Draws whose slots
// do not move are drawn with the unchanged polygon pipeline from the same bytes,
// so an identity motion reproduces the canonical capture exactly.
namespace sgn
{
inline constexpr std::uint16_t polygon_reprojection_passthrough = 0xffffu; // 2D overlay: never moved
inline constexpr std::size_t polygon_reprojection_fixed_slots = 14;        // static, sky, 6 vehicles, 6 shadows
// Screen mapping of one captured draw in capture space (before the host canvas
// offset): X = kx * x / w + cx, Y = ky * y / w + cy, w = 32 / RHW.
struct PolygonReprojectionMapping
{
    float kx{}, ky{}, cx{}, cy{};
    std::uint32_t valid{};
};
struct PolygonReprojectionDraw
{
    std::size_t first_slot{}; // index into PolygonReprojectionBase::slots of its first vertex
    PolygonReprojectionMapping mapping;
};
// Immutable per canonical base capture (built once, shared by every present that uses it).
struct PolygonReprojectionBase
{
    ImmutablePolygonSubmission submission;
    std::vector<std::uint16_t> slots;            // per captured vertex, capture order over all segments
    std::vector<PolygonReprojectionDraw> draws;  // per captured draw, capture order over all segments
    std::vector<std::size_t> segment_first_draw; // draws index of each segment's first draw
    std::unordered_map<const CapturedPolygonDraw *, std::uint32_t> draw_index; // captured draw -> draws index
    float w_near{};                                                            // source near plane w (clip_near + 1)
};
// One slot's motion, shader layout (8 x float4): columns x,y,z of the affine
// motion (xyz = matrix column, w = translation), the same for the blend target,
// the trail blend's older point (w = 1/|axis|^2, 0: weight 1) and axis
// (w = flag bits as float bits: 1 moves, 2 blend).
struct PolygonReprojectionMotion
{
    std::array<float, 32> words{};
};
// Per present.
struct PolygonReprojection
{
    std::shared_ptr<const PolygonReprojectionBase> base;
    std::span<const PolygonReprojectionMotion> motions; // indexed by slot
    std::array<float, 12> to_world{};                   // base camera space -> world, columns x,y,z (w = translation)
    std::uint64_t moving{};                             // bit s: slot s moves (s < 63); bit 63: a slot >= 63 moves
};
// Slot -> moving bit.
constexpr std::uint64_t polygon_reprojection_bit(std::uint32_t slot) noexcept
{
    return slot < 63 ? std::uint64_t{1} << slot : std::uint64_t{1} << 63;
}
// The re-projection of the present being recorded (owner/render thread only):
// set by the Renderer around one presentation so the host post process can
// re-project its emissive copies of the same draws. Null otherwise.
inline const PolygonReprojection *polygon_reprojection_current = nullptr;
class PolygonReprojectionScope
{
    const PolygonReprojection *previous_;

  public:
    explicit PolygonReprojectionScope(const PolygonReprojection *current) noexcept
        : previous_(polygon_reprojection_current)
    {
        polygon_reprojection_current = current;
    }
    ~PolygonReprojectionScope()
    {
        polygon_reprojection_current = previous_;
    }
    PolygonReprojectionScope(const PolygonReprojectionScope &) = delete;
    PolygonReprojectionScope &operator=(const PolygonReprojectionScope &) = delete;
};
} // namespace sgn
