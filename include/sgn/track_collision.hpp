#pragma once
#include "sgn/track_contact.hpp"

namespace sgn::retail::contact
{
// remaining is a finite signed movement scale, not a fraction constrained to
// [0,1]. Retail can return a negative value and feed it into the next query.
struct SweepInput
{
    Vec3 start, motion;
    float radius{}, remaining{1};
};
struct SweepResult
{
    bool hit{};
    Vec3 start, end, direction, step, normal, center, surface;
    float remaining{}, travel_length{}, step_length{};
    std::uint32_t polygon{0xffffffffu};
    std::uint32_t normal_payload{};
    bool normal_payload_defined{};
    // 00420CA0 writes the query normal even when no accepted hit survives.
    // Kept separate from the hit result for the original persistent workspace.
    bool query_normal_written{};
    Vec3 query_normal{};
    bool query_normal_payload_written{}, query_normal_payload_defined{};
    std::uint32_t query_normal_payload{};
    // Source 00420857 copies input W for a near-static face; vertex/edge and
    // moving-face centers use an unwritten temporary W. Diagnostics only.
    bool start_payload_inherits_input{true};
};
struct PolygonQuery
{
    Vec3 start, end, step, direction;
    float radius{}, radius_squared{}, step_length{};
    bool include_edges{true};
};
struct PolygonHit
{
    bool hit{};
    Vec3 normal, center, surface;
    float distance{};
    std::uint32_t normal_payload{};
    bool normal_payload_defined{};
};
PolygonHit intersect_polygon(const TrackGeometry &, std::uint32_t polygon, const PolygonQuery &); // 00420C40
PolygonHit intersect_edge(Vec3 a, Vec3 b, const PolygonQuery &, float previous_distance);         // 00420200
// Original 00420FE0 query, including vertex, edge and polygon-face candidates.
// Numerical qualification is recorded separately from full vehicle response.
// Input contracts are checked; query does not allocate or mutate track data.
SweepResult sweep_sphere(const TrackGeometry &, const SweepInput &);
} // namespace sgn::retail::contact
