#pragma once
#include "sgn/race_session_platform.hpp"
#include "sgn/vehicle_skid_strip.hpp"
namespace sgn::retail
{
// Actual current vehicle, replay, two front-wheel headers/rings and source
// strip allocations. No vehicle/world copy, native geometry or preview kernel.
void update_source_vehicle_strip_41d140(RaceSessionPlatform &, VehicleSkidWriteAudit * = nullptr);
void update_source_vehicle_wheel_strip_41cd30(RaceSessionPlatform &, unsigned wheel, VehicleSkidWriteAudit * = nullptr);
// header_slot is the captured ring owner; up is reloaded through actual current
// vehicle inside41CB40. The reached caller's surface argument is unused by PE.
void emit_source_vehicle_strip_quad_41cb40(RaceSessionPlatform &, unsigned header_slot, unsigned wheel,
                                           std::uint32_t segment, std::uint32_t surface_argument);
} // namespace sgn::retail
