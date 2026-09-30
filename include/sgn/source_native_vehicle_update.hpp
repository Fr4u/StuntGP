#pragma once
#include "sgn/source_core_race_update.hpp"
#include "sgn/source_audio_device_adapter.hpp"
#include "sgn/source_track_collision.hpp"

namespace sgn::retail
{
class RaceSessionPlatform;
// NEW R18 (DESIGN A8): the single 442810 receiver of the unified game. The
// 44285A vehicle child runs the complete native NEW 418A90 traversal (the same
// decomp-backed modules as the former START race) directly on the one menu
// world. Track splines are resolved per call from the live source allocation,
// collision is the per-level immutable decode of the same bytes, audio is the
// world's single source sample owner. 40DE37 demo keeps the exact 40DE30 body.
class SourceNativeVehicleUpdate final : public SourceCoreRaceUpdateRequired
{
    RaceSessionPlatform &platform_;
    SourceTrackCollision collision_;
    SourceAudioDeviceAdapter audio_;
    SourceCoreRaceUpdateUnavailable remaining_;

  public:
    explicit SourceNativeVehicleUpdate(RaceSessionPlatform &);
    void perform(SourceCoreRaceUpdateChild, FrontendState &, RaceTextureReplacement &) override;
    std::uint32_t contains_4278a0(const AudioEnvironmentRegion &, const CameraVector &) override; // PORT-FIX-001 body
    // 4420E5 ->42EAA0 releases the level allocations; release the decoded copy with them.
    void release_scene_collision() noexcept
    {
        collision_.drop();
    }
};
} // namespace sgn::retail
