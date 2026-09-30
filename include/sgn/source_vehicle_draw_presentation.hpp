#pragma once
#include "sgn/source_vehicle_draw_graph.hpp"
#include "sgn/vehicle_trails.hpp"
#include <span>
namespace sgn::retail
{
enum class SourceVehicleRingKind
{
    speed,
    effect,
    pair_a,
    pair_b
};
// Actual owner identity, not a decoded/copy ring. The ring implementer must
// borrow it from the supplied same platform at the reached41F2F0 invocation.
struct SourceVehicleRingCapture
{
    SourceVehicleRingKind kind;
    unsigned vehicle, side;
    std::uint32_t profile_address, cursor, count;
    float scale, alpha;
};
// Invocation borrow of the captured physical owner's ring. No element is read
// here; the consumer must retain its own source count/branch access order.
std::span<const VehicleVector> borrow_source_vehicle_ring_41f2f0(RaceSessionPlatform &,
                                                                 const SourceVehicleRingCapture &);
struct SourceVehicleRingRequired
{
    virtual ~SourceVehicleRingRequired() = default;
    virtual void append_41f2f0(RaceSessionPlatform &, std::uint32_t callsite, const SourceVehicleRingCapture &) = 0;
};
struct SourceVehicleRingUnavailable final : SourceVehicleRingRequired
{
    void append_41f2f0(RaceSessionPlatform &, std::uint32_t at, const SourceVehicleRingCapture &) override
    {
        throw SourceVehicleDispatchBoundary(at, 0x41f2f0);
    }
};
void present_source_vehicle_parts_414350(RaceSessionPlatform &, SourceVehicleDrawStack &);
void append_source_vehicle_speed_trail_414cf0(RaceSessionPlatform &, SourceVehicleRingRequired &);
void append_source_vehicle_effect_trails_414e50(RaceSessionPlatform &, SourceVehicleRingRequired &);
void append_source_vehicle_pair_a_trails_415040(RaceSessionPlatform &, SourceVehicleRingRequired &);
void append_source_vehicle_pair_b_trails_4153a0(RaceSessionPlatform &, SourceVehicleRingRequired &);
void append_source_vehicle_glows_4154c0(RaceSessionPlatform &, SourceVehicleDrawStack &);
class SourceVehicleDrawPresentation final : public SourceVehicleDispatchRequired
{
    SourceVehicleDispatchRequired &remaining_;
    SourceVehicleDrawStack &stack_;
    SourceVehicleRingRequired &rings_;

  public:
    SourceVehicleDrawPresentation(SourceVehicleDispatchRequired &remaining, SourceVehicleDrawStack &stack,
                                  SourceVehicleRingRequired &rings)
        : remaining_(remaining), stack_(stack), rings_(rings)
    {
    }
    void invoke(RaceSessionPlatform &, std::uint32_t, std::uint32_t, std::optional<unsigned>) override;
    void leader_notice(RaceSessionPlatform &, unsigned, std::string_view) override;
};
} // namespace sgn::retail
