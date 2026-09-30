#pragma once
#include "sgn/source_vehicle_draw_stack.hpp"

namespace sgn::retail
{
// Full first418180 child, including both near and far detail branches. It uses
// the SAME model descriptor/texture namespace; distance never chooses a skin.
//  The result records the branch that actually completed, never a later LOD read.
//  True proves4166F5 saved literal ESI=1 at the next body's EBP-4.
bool select_source_vehicle_draw_detail_413e10(RaceSessionPlatform &);
// Full reached41C420 shadow draw including zero-flags42B7F0, source matrices,
// background publication and late body->shadow descriptor vector copy.
void append_source_vehicle_shadow_41c420(RaceSessionPlatform &);
// Non-owning adapter for the immutable dispatcher prerequisite. Only the two
// qualified pairs are intercepted. Every other callsite remains mandatory.
class SourceVehicleDrawGraph final : public SourceVehicleDispatchRequired
{
    SourceVehicleDispatchRequired &remaining_;
    SourceVehicleDrawKnownPrefix *prefix_{};

  public:
    explicit SourceVehicleDrawGraph(SourceVehicleDispatchRequired &remaining) : remaining_(remaining) {}
    SourceVehicleDrawGraph(SourceVehicleDispatchRequired &remaining, SourceVehicleDrawKnownPrefix &prefix)
        : remaining_(remaining), prefix_(&prefix)
    {
    }
    void invoke(RaceSessionPlatform &, std::uint32_t callsite, std::uint32_t child, std::optional<unsigned>) override;
    void leader_notice(RaceSessionPlatform &, unsigned, std::string_view) override;
};
} // namespace sgn::retail
