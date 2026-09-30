#pragma once
namespace sgn::retail
{
class RaceSessionPlatform;
// Required full442EF0: same live selected-script format, real4199F0 file path,
// same ReplayBank and track triple. No host AssetStore-plan replacement.
class SourceTrackReplay442ef0
{
  public:
    virtual ~SourceTrackReplay442ef0() = default;
    virtual void load_track_replay_442ef0(RaceSessionPlatform &) = 0;
};
void reset_source_time_trial_ghosts_443060(RaceSessionPlatform &);
void prepare_source_time_trial_selection_445150(RaceSessionPlatform &, SourceTrackReplay442ef0 &);
void prepare_source_time_trial_445360(RaceSessionPlatform &, SourceTrackReplay442ef0 &);
} // namespace sgn::retail
