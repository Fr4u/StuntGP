#pragma once
#include "sgn/source_level_script_load.hpp"

namespace sgn::retail
{
// Required full callees on the same Session, bank, replay and scene owners.
// No default implementations and no LevelOverrideState adapter.
class SourceLevelOverrideChildren
{
  public:
    virtual ~SourceLevelOverrideChildren() = default;
    virtual void prepare_time_trial_445360(RaceSessionPlatform &) = 0;
    virtual void prepare_cars_444db0(RaceSessionPlatform &) = 0;
};
void apply_source_demo_override_445310(RaceSessionPlatform &);
void apply_source_challenge_override_445120(RaceSessionPlatform &);
void apply_source_level_overrides_4453a0(RaceSessionPlatform &, SourceLevelOverrideChildren &);

// The42D070 binding is concrete, but construction still requires both genuine
// full children. Never bind old initialization plans or no-ops to qualify it.
class SourceLevelScriptChildrenImpl final : public SourceLevelScriptChildren
{
    SourceLevelOverrideChildren &children_;

  public:
    explicit SourceLevelScriptChildrenImpl(SourceLevelOverrideChildren &children) : children_(children) {}
    void apply_overrides_4453a0(RaceSessionPlatform &) override;
};
} // namespace sgn::retail
