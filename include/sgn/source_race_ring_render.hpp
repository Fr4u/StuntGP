#pragma once
#include "sgn/source_race_view_stage.hpp"
#include "sgn/source_vehicle_draw_presentation.hpp"
#include <stdexcept>
namespace sgn::retail
{
// A reached source-stack read with no reconstructed producer. Never replaces
// the missing byte with zero, a ring W or the old destination payload.
class SourceRaceRingStackBoundary final : public std::runtime_error
{
  public:
    const std::uint32_t source_function, source_instruction;
    const std::int32_t frame_offset;
    SourceRaceRingStackBoundary(std::uint32_t f, std::uint32_t at, std::int32_t offset)
        : std::runtime_error("source ring render reaches an unproduced stack DWORD"), source_function(f),
          source_instruction(at), frame_offset(offset)
    {
    }
};
// Bind this to SourceRaceViewStageRequired::impact_trail_41f2f0. Actual impact
// owner only; arguments are the captures made by416050. Full known head and
// early-return paths; world/screen routes stop at the exact opaque-stack
// frontier, preserving reached writes. No full-ring/whole-frame copy.
void append_source_impact_ring_41f2f0(RaceSessionPlatform &, const contact::effects::ImpactParticle &,
                                      std::uint32_t profile_index, std::uint32_t cursor, std::uint32_t count,
                                      float scale, float alpha);
// Stateless synchronous binding for the seven actual vehicle callsites.
// Borrows the captured vehicle/side ring from the SAME supplied platform;
// never recaptures current_slot, cursor/count, scale/alpha or copied points.
// Preserves the same ring body and all opaque-stack boundaries above.
class SourceVehicleRingBinding final : public SourceVehicleRingRequired
{
  public:
    void append_41f2f0(RaceSessionPlatform &, std::uint32_t callsite, const SourceVehicleRingCapture &) override;
};

} // namespace sgn::retail
