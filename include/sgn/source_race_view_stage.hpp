#pragma once
#include "sgn/race_session_platform.hpp"
#include "sgn/contact_takeoff_particles.hpp"
#include "sgn/impact_particle_lifetime.hpp"

namespace sgn::retail
{
// Reached source calls only. Root/world owns418180. The trail receiver must
// consume this real particle's reserved DWORD ring, not copy64 points or
// substitute the preview41F2F0. Captured arguments match4160BB..4160E4.
struct SourceRaceViewStageRequired
{
    virtual ~SourceRaceViewStageRequired() = default;
    virtual void vehicles_418180(RaceSessionPlatform &) = 0;
    virtual void impact_trail_41f2f0(RaceSessionPlatform &, const contact::effects::ImpactParticle &,
                                     std::uint32_t profile_index, std::uint32_t cursor, std::uint32_t count,
                                     float scale, float alpha) = 0;
};
class SourceRaceViewStageUnavailable final : public SourceRaceViewStageRequired
{
  public:
    void vehicles_418180(RaceSessionPlatform &) override;
    void impact_trail_41f2f0(RaceSessionPlatform &, const contact::effects::ImpactParticle &, std::uint32_t,
                             std::uint32_t, std::uint32_t, float, float) override;
};
// Full outer source order:418180,45E4F0,416050,449D60(RET).
// Never executes the second402E90 or42E870; the draw caller owns those.
void prepare_source_race_view_stage_442500(RaceSessionPlatform &, SourceRaceViewStageRequired &);
void append_source_effect_particle_sprites_45e4f0(RaceSessionPlatform &);
void append_source_impact_particle_trails_416050(RaceSessionPlatform &, SourceRaceViewStageRequired &);
} // namespace sgn::retail
