#pragma once
#include <cstdint>
#include <stdexcept>
namespace sgn::retail
{
class FrontendState;
class SourceMenuSampleNative;
struct RaceTextureReplacement;
// A reached missing child stops this source invocation. Earlier source writes
// remain visible; Core must poison the partially advanced tick, never retry it.
class SourceRaceLifecycleBoundary final : public std::runtime_error
{
  public:
    const std::uint32_t source_callsite, required_child;
    SourceRaceLifecycleBoundary(std::uint32_t site, std::uint32_t child, const char *reason)
        : std::runtime_error(reason), source_callsite(site), required_child(child)
    {
    }
};
// Actual443A80 dispatcher, single source tick, same shared Platform owners.
// Bounded children are documented in REPORT.md. No preview RaceSession or dt.
void advance_source_race_lifecycle_443a80(FrontendState &, SourceMenuSampleNative &, RaceTextureReplacement &);
} // namespace sgn::retail
