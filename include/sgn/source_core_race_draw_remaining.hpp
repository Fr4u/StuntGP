#pragma once
#include "sgn/source_core_race_draw.hpp"
#include "sgn/source_race_pause_results_draw.hpp"

namespace sgn::retail
{
class SourceApplicationCore;
class NativeSourceMenuCrt;
class SourceFrontendMaterialBinding;
// R17 concrete receiver for the three remaining4427B0/442640 children:
// 44277E ->4473F0 pause menu,442783 ->449FB0 results,44278D ->40E030 demo.
// Entities/world/HUD/countdown stay in Core's CoreDrawChildren, which forwards
// the other three here. Borrows only this Core's retained owners: frontend
// world, frames/submission/sink (via the context), the CURRENT material adapter
// resolved at each reached child, the retained sample receiver and the CRT the
// Composition already binds to this Core. No RaceSession, copied world,
// captured scene or success fallback. Inert construction.
class SourceCoreRaceDrawRemaining final : public SourceCoreRaceDrawRequired, private SourcePauseRequired
{
    SourceApplicationCore &core_;
    NativeSourceMenuCrt &crt_;
    void same(FrontendState &, std::uint32_t site, std::uint32_t entry) const;
    // 446AF5 CALL40D500 after the completed45F9D0 (pause restart callback).
    void restart_40d500(FrontendState &, std::uint32_t source_callsite) override;
    // 446BFB/446C00/446C05/446C0A direct preset tails with the captured vehicle.
    void camera_preset(FrontendState &, std::uint32_t source_callsite, std::uint32_t source_entry,
                       std::uint32_t captured_vehicle) override;

  public:
    SourceCoreRaceDrawRemaining(SourceApplicationCore &, NativeSourceMenuCrt &);
    SourceCoreRaceDrawRemaining(const SourceCoreRaceDrawRemaining &) = delete;
    SourceCoreRaceDrawRemaining &operator=(const SourceCoreRaceDrawRemaining &) = delete;
    void perform(SourceCoreRaceDrawChild, std::uint32_t source_callsite, SourceCoreRaceDrawContext &) override;
};
// Complete40E030 (40E030..40E043): exact hud_block==1 gate, then40DE80 and
// tail40DF90 on the same Core owners. Inactive branch borrows nothing.
void submit_source_core_demo_40e030(SourceCoreRaceDrawContext &, SourceFrontendMaterialBinding &);
// Complete40DE30 (40DE30..40DE74) including40DDD0/40DD80/40DD30/447B60 and the
// tail jump to441E20, on the frontend's retained platform world. Usable from
// SourceCoreRaceUpdateRequired::perform(SourceCoreRaceUpdateChild::demo,...).
void update_source_demo_40de30(FrontendState &);
} // namespace sgn::retail
