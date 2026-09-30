#pragma once
#include "sgn/source_core_race_draw.hpp"

namespace sgn::retail
{
class SourceMenuSampleNative;
struct RaceTextureReplacement;
// Reached source children without a complete direct-owner receiving adapter.
// A receiver must finish the whole child on this frontend's retained platform
// or throw. No RaceSession, shadow vehicle/camera, or success notification.
struct SourcePauseRequired
{
    virtual ~SourcePauseRequired() = default;
    virtual void restart_40d500(FrontendState &, std::uint32_t source_callsite) = 0;
    virtual void camera_preset(FrontendState &, std::uint32_t source_callsite, std::uint32_t source_entry,
                               std::uint32_t captured_vehicle) = 0;
};
class SourcePauseUnavailable final : public SourcePauseRequired
{
  public:
    void restart_40d500(FrontendState &, std::uint32_t) override;
    void camera_preset(FrontendState &, std::uint32_t, std::uint32_t, std::uint32_t) override;
};
// Root binds at44277E and442783 using the CURRENT texture adapter and the
// SAME retained sample receiver. These calls do not admit/activate a race.
// Existing finite numeric drawing contracts remain; see packet GAPS.md.
void submit_source_pause_menu_4473f0(SourceCoreRaceDrawContext &, RaceTextureReplacement &, SourceMenuSampleNative &,
                                     SourcePauseRequired &);
void submit_source_results_449fb0(SourceCoreRaceDrawContext &, RaceTextureReplacement &);
} // namespace sgn::retail
