#pragma once
#include "sgn/retail_vehicle_strip.hpp"
#include "sgn/retail_replay.hpp"
#include "sgn/vehicle_presentation.hpp"
#include <bitset>

namespace sgn::retail
{
struct VehicleSkidSegment
{
    VehicleVector start{}, end{}, step{};
    std::uint32_t first_vertex{};
}; // original0x34-byte record; W words are opaque payload, not geometry inputs.
struct VehicleSkidRing
{
    // Native: borrowed corner index. Source: captured bank address token, not
    // an index resolved against a later57130C allocation. NULL is empty in both.
    std::optional<std::uint32_t> last_corner;
    std::array<VehicleSkidSegment, 64> segments{};
};
struct VehicleSkidRings
{
    std::array<VehicleSkidRing, 4> items{};
};
// Optional diagnostic of written undefined W lanes: start/end/step per record.
// No persistent state and no alternative computation path.
struct VehicleSkidWriteAudit
{
    std::array<std::bitset<192>, 4> undefined_writes{};
};
// Headers3964/3968/396C remain in VehicleStripHeaders. No second owner for
// current wheel centers, physical up/grip usage/speed, replay status or world geometry.
void emit_vehicle_skid_quad(VehicleSkidRing &, unsigned segment, VehicleStripGeometry &, Vec3 up); // 41CB40
void update_vehicle_wheel_skid(unsigned wheel, const VehicleState &, const VehiclePresentationState &,
                               VehicleStripHeaders &, VehicleSkidRings &, VehicleStripGeometry &,
                               VehicleSkidWriteAudit * = nullptr); // 41CD30
void update_vehicle_skid(const VehicleState &, const VehicleRaceState &, const VehiclePresentationState &,
                         const ReplayBank &, std::uint32_t quality, VehicleStripHeaders &, VehicleSkidRings &,
                         VehicleStripGeometry &, VehicleSkidWriteAudit * = nullptr); // 41D140
} // namespace sgn::retail
