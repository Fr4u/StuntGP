#pragma once
#include "sgn/world_packet_join.hpp"
#include "sgn/retail_world_polygon_frame.hpp"

namespace sgn::retail
{
class RaceSession;
}
namespace sgn::diagnostics
{
// Invocation-only adapter. Application activation/export is a separate caller.
class SessionWorldJoin final
{
    bool enabled_{};
    retail::RaceSession *session_{};
    std::optional<retail::WorldPolygonFrameTicket> ticket_;
    WorldPacketJoinCapture capture_;

  public:
    explicit SessionWorldJoin(bool enabled = false, WorldJoinLimits limits = {}) noexcept
        : enabled_(enabled), capture_(enabled, limits)
    {
    }
    bool prepare(retail::RaceSession &, const PolygonDeviceContext &, std::uint64_t serial,
                 std::uint64_t tick) noexcept;
    void preclip(retail::RaceSession &) noexcept;
    void before_submission(retail::RaceSession &, const retail::PolygonSubmissionLists &,
                           const retail::PolygonSubmissionInputs &, const retail::PolygonSubmissionSink &) noexcept;
    void after_submission(const retail::PolygonSubmissionSink &sink) noexcept
    {
        (void)capture_.after_submission(sink);
    }
    bool finish(const ImmutablePolygonSubmission &recipe) noexcept;
    void abort() noexcept
    {
        capture_.abort();
        ticket_.reset();
        session_ = nullptr;
    }
    const WorldJoinResult &result() const noexcept
    {
        return capture_.result();
    }
    bool matches_displayed(const ImmutablePolygonSubmission &recipe) const noexcept
    {
        return capture_.matches_displayed(recipe);
    }
};
} // namespace sgn::diagnostics
