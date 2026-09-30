#pragma once
#include "sgn/contact_rail_state.hpp"

namespace sgn::retail
{
struct RailProjectionAudit
{
    std::uint32_t segments_examined{};
    bool adjacent_storage_read{};
};
// Full00418E10. segment is the existing contact.surface.special_ticks at764C;
// source_position borrows the live physical position. No replica is retained.
RailProjectionAudit project_rail_position(SplineView, Vec3 source_position, std::uint32_t &segment, VehicleRailState &);
} // namespace sgn::retail
