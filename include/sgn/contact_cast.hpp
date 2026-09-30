#pragma once
#include "sgn/contact_solver.hpp"
#include "sgn/contact_query_internals.hpp"
namespace sgn::retail::contact
{
// Additional persistent query fields. query_motion4980E8 and last_normal498198
// are always borrowed from the existing shared solver workspace.
struct ContactCastState
{
    Vector4 start{}, end{}, step{}, direction{}, center{}, surface{};
    std::uint32_t motion_payload{};                                               // 4980F4
    float step_length{}, travel_limit{}, remaining{}, radius{}, radius_squared{}; // 498148..158
    ContactGeometryPointer polygon;                   // 498164: native index OR actual weak source pointer
    std::uint32_t hit_flag{}, polygon_list{}, mode{}; // 4981E4,49823C,4754A0
    ContactQueryInternals internals;                  // single persistent49815C..498240/5715CC owner
};
// Test-only definedness tags. They never select geometry or modify values.
struct ContactCastPayloadAudit
{
    bool start{}, end{}, direction{}, center{}, motion{}, shadow_position{};
};
// Literal4212A0: validates source remaining range, enters mode2 (face only),
// invokes actual collision geometry, restores mode1 only on its normal path.
std::uint32_t cast_track_faces(const TrackGeometry &, solver::Workspace &, ContactCastState &,
                               ContactCastPayloadAudit *audit = nullptr,
                               ContactQueryInternalsAudit *internals_audit = nullptr,
                               solver::PayloadAudit *payload = nullptr);
} // namespace sgn::retail::contact
