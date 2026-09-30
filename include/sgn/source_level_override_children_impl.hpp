#pragma once
#include "sgn/source_level_overrides.hpp"
#include "sgn/source_track_replay.hpp"
#include "sgn/source_car_selection.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/scene_resources.hpp"

namespace sgn::retail
{
// Real445360/444DB0 caller binding. Only the full183000-byte vehicle clear
// remains mandatory external work; construction cannot supply a default/no-op.
class SourceLevelOverrideChildrenImpl final : public SourceLevelOverrideChildren
{
    SourceTrackReplayImpl replay_;
    SourceVehicleClear444dc9 &clear_;

  public:
    SourceLevelOverrideChildrenImpl(SourceBootFilePlatform &files, SourceVehicleClear444dc9 &clear)
        : replay_(files), clear_(clear)
    {
    }
    void prepare_time_trial_445360(RaceSessionPlatform &platform) override
    {
        prepare_source_time_trial_445360(platform, replay_);
    }
    void prepare_cars_444db0(RaceSessionPlatform &platform) override
    {
        platform.source_scene_resources().models().produce_source_car_requests_444db0(platform, clear_);
    }
};
} // namespace sgn::retail
