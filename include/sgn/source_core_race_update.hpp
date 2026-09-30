#pragma once
#include "sgn/source_outer_scheduler.hpp"
#include "sgn/race_session_music.hpp"
#include <stdexcept>

namespace sgn::retail
{
class SourceMenuSampleNative;
// These are the remaining actual source-world bindings, never preview calls.
// A provider must execute the complete reached child on the supplied world or
// throw with its exact boundary. No silent defaults, empty geometry or dt input.
enum class SourceCoreRaceUpdateChild : std::uint32_t
{
    vehicles = 0x418a90,
    demo = 0x40de30
};
class SourceCoreRaceUpdateBoundary final : public std::runtime_error
{
  public:
    const std::uint32_t source_callsite, required_child;
    SourceCoreRaceUpdateBoundary(std::uint32_t callsite, std::uint32_t child, const char *reason)
        : std::runtime_error(reason), source_callsite(callsite), required_child(child)
    {
    }
};
struct SourceCoreRaceUpdateRequired : SourceAudioEnvironmentRegionCalls
{
    virtual ~SourceCoreRaceUpdateRequired() = default;
    virtual void perform(SourceCoreRaceUpdateChild, FrontendState &, RaceTextureReplacement &) = 0;
};
// Vehicle/demo calls remain explicit boundaries until their complete binding.
// The audio predicate is implemented, with the documented source bug fix
// PORT-FIX-001. It does not substitute a vehicle/demo child.
class SourceCoreRaceUpdateUnavailable final : public SourceCoreRaceUpdateRequired
{
  public:
    void perform(SourceCoreRaceUpdateChild, FrontendState &, RaceTextureReplacement &) override;
    std::uint32_t contains_4278a0(const AudioEnvironmentRegion &, const CameraVector &) override;
};
// Full442810 ordering on Core's actual retained owners. No RaceSession is
// constructed/borrowed, no Models::at, capture or4427B0. Core owns entry cadence
// and guards; this function only composes the reached synchronous source calls.
SourceOuterOutcome update_source_core_race_442810(FrontendState &, FrontendPerformanceCounter &,
                                                  const WindowInputBorrow &, RaceTextureReplacement &,
                                                  SourceMenuSampleNative &, SourceCoreRaceUpdateRequired &,
                                                  SceneTransitionServices &, SourceOuterTrace * = nullptr);
} // namespace sgn::retail
