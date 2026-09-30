#pragma once
#include "sgn/race_session_platform.hpp"
#include "sgn/assets.hpp"
#include <optional>
#include <string_view>

namespace sgn::retail
{
class SourceVehicleDispatchBoundary final : public FormatError
{
  public:
    std::uint32_t callsite, child;
    SourceVehicleDispatchBoundary(std::uint32_t, std::uint32_t);
};
// Invocation-only receiver of the children not qualified in this packet.
// Implementations borrow the SAME platform and must preserve partial writes.
// captured_vehicle transports ECX only where the caller explicitly loads it;
// other children read platform.state().current_slot themselves.409B60 has ECX=0.
// No receiver may construct a Session, snapshot the world or silently succeed.
struct SourceVehicleDispatchRequired
{
    virtual ~SourceVehicleDispatchRequired() = default;
    virtual void invoke(RaceSessionPlatform &, std::uint32_t callsite, std::uint32_t child,
                        std::optional<unsigned> captured_vehicle) = 0;
    // Reached41E3DE ->442CF0, duration180/audience1/priority100. The local32-byte
    // formatted string lives only during this synchronous call; source is leader.
    virtual void leader_notice(RaceSessionPlatform &, unsigned leader, std::string_view) = 0;
};
struct SourceVehicleDispatchUnavailable final : SourceVehicleDispatchRequired
{
    void invoke(RaceSessionPlatform &, std::uint32_t, std::uint32_t, std::optional<unsigned>) override;
    void leader_notice(RaceSessionPlatform &, unsigned, std::string_view) override;
};
// Separate draw entry.418A90 never invokes it. Updates41D140 and publication
// 41D1C0 remain in their original phases and borrow the same strip allocations.
void append_source_vehicle_draw_418180(RaceSessionPlatform &, SourceVehicleDispatchRequired &);
} // namespace sgn::retail
