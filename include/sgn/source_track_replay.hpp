#pragma once
#include "sgn/source_time_trial_selection.hpp"
#include "sgn/source_boot_config.hpp"
namespace sgn::retail
{
void load_source_track_replay_442ef0(RaceSessionPlatform &, SourceBootFilePlatform &);
class SourceTrackReplayImpl final : public SourceTrackReplay442ef0
{
    SourceBootFilePlatform &files_;

  public:
    explicit SourceTrackReplayImpl(SourceBootFilePlatform &files) : files_(files) {}
    void load_track_replay_442ef0(RaceSessionPlatform &platform) override
    {
        load_source_track_replay_442ef0(platform, files_);
    }
};
} // namespace sgn::retail
