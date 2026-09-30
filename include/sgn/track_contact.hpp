#pragma once
#include "sgn/assets.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace sgn::retail
{
class SourcePmdTrackView;
}
namespace sgn::retail::contact
{
// Fourth lanes are copied payload, never interpreted as a homogeneous divisor.
// 00421CC0/00421D90 write an uninitialized stack lane in retail; new code retains
// that lane and qualifies only the physical xyz outputs of those two functions.
struct Vector4
{
    float x{}, y{}, z{};
    std::uint32_t opaque{};
};
using Matrix4 = std::array<float, 16>;
struct Pose
{
    Vector4 position, previous_position, velocity;
    Matrix4 orientation{}, incremental{};
};
struct ContactBody
{
    Vector4 position, previous_position, velocity;
    Matrix4 orientation{}, incremental{};
    float mass{};
};
struct ContactProbe
{
    Vector4 local, current, previous, velocity;
    std::uint32_t contact_iterations{};
    float mass_fraction{}, radius{};
};
struct Material
{
    std::uint8_t flags{}, grip{}, rolling{}, roughness{};
};
struct WheelContact
{
    float filtered_load{}, current_load{};
    Vector4 normal;
    std::int32_t countdown{}, previous_type{}, type{}, changed{};
    float grip{}, roughness{}, rolling{};
};
// Geometry is the independent collision representation at PMD+0xA0, not the
// renderer triangle list. Counts below exclude neither authored padding nor LODs
// by guess: all reachable references are checked before exposing the geometry.
struct Polygon
{
    std::uint8_t flags{}, vertex_count{};
    std::uint16_t first_index{}, normal{}, material{};
};
struct SpatialNode
{
    Vec3 split{};
    std::uint32_t opaque{};
    std::array<std::uint16_t, 8> child{}, leaf{};
};
struct TrackGeometry
{
    std::vector<std::uint16_t> polygon_lists, indices;
    std::vector<Vec3> vertices, normals;
    std::vector<Polygon> polygons;
    std::vector<Material> materials;
    std::vector<SpatialNode> nodes;
    std::array<float, 2> radius_threshold{};
    std::array<std::uint32_t, 2> roots{};
    // Authored fourth words of normals. They are copied by face contact and
    // remain payload, never interpreted as geometric coordinates.
    std::vector<std::uint32_t> normal_payload;
};
TrackGeometry decode_track_collision(std::span<const std::uint8_t>); // 0042ED80
// NEW host producer (WP-B, DESIGN A2): the same TrackGeometry as the file
// decoder above, read from the LIVE track allocation that 42ED80..42EE45
// relocated in place. Every range start is the relocated descriptor pointer
// (the same DWORDs 420F80/420F00 dereference through [624480]); every element
// count uses the file decoder's own extent rule over the unrelocated header
// words and the actual allocation size (= file size). Roots are recovered as
// (descriptor+1C+8i - nodes)/48, radii from descriptor+18+8i. Vertex W is
// never read; normal W goes to normal_payload. Same reachability validation.
TrackGeometry decode_track_collision(const SourcePmdTrackView &);
void pack_pose(const Pose &, ContactBody &);                            // 004088B0
void unpack_pose(const ContactBody &, Pose &);                          // 00408920
void update_probe_position(const ContactBody &, ContactProbe &);        // 00421CC0, xyz
void update_probe_velocity(const ContactBody &, ContactProbe &);        // 00421D90, xyz
void apply_material(Material, std::int32_t track_mode, WheelContact &); // 004091E0 + 00409180
void filter_wheel_load(float contact_load, WheelContact &);             // 00409E27..00409E93
void reflect_motion(Vec3 normal, float restitution, Vec3 &motion);      // 00421C30
void angular_response(const ContactBody &, const ContactProbe &, Vec3 normal, float angle, const Matrix4 &inertia,
                      Matrix4 &accumulated); // 00422020
struct ImpactParameters
{
    std::array<float, 128> restitution_curve{};
    float gravity{}, inverse_probe_count{}, effective_mass{};
    Matrix4 inertia{};
};
struct ImpactAccumulator
{
    Vec3 velocity_delta;
    Matrix4 rotation_delta{};
    float normal_speed{};
    std::uint32_t collision_count{};
    bool touched{};
};
// Defined physical impulse prefix of 004093E0 through 00409664. Wheel-material,
// damage, score, audio and surface-event branches are separate pending work.
void accumulate_impact(const ContactBody &, ContactProbe &, Vec3 normal, Vec3 &motion, const ImpactParameters &,
                       ImpactAccumulator &, bool *audit_rotation_payload_undefined = nullptr);
std::uint32_t find_polygon_list(const TrackGeometry &, Vec3 endpoint, float radius); // 00420F80/00420F00
struct PlaneBand
{
    float endpoint_distance{}, start_distance{};
    bool accepted{};
};
PlaneBand plane_band(Vec3 start, Vec3 end, Vec3 point, Vec3 normal, float radius); // 00420A70
bool inside_polygon(const TrackGeometry &, const Polygon &, Vec3 point);           // 00420690
} // namespace sgn::retail::contact
