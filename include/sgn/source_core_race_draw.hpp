#pragma once
#include "sgn/session_outer_frame_services.hpp"
#include <stdexcept>

namespace sgn::retail
{
struct SourceCoreRaceDrawContext;
enum class SourceCoreRaceDrawChild : std::uint32_t
{
    entities = 0x442500,
    world_submission = 0x433f40,
    hud = 0x44ae40,
    pause_menu = 0x4473f0,
    results = 0x449fb0,
    countdown = 0x449b50,
    demo = 0x40e030
};
class SourceCoreRaceDrawBoundary final : public std::runtime_error
{
  public:
    const std::uint32_t source_callsite, required_child;
    SourceCoreRaceDrawBoundary(std::uint32_t site, std::uint32_t child, const char *reason)
        : std::runtime_error(reason), source_callsite(site), required_child(child)
    {
    }
};
struct SourceCoreRaceDrawRequired
{
    virtual ~SourceCoreRaceDrawRequired() = default;
    // Execute the entire reached child on these same owners or throw. Never
    // construct a RaceSession/native world, return a fake success or skip work.
    virtual void perform(SourceCoreRaceDrawChild, std::uint32_t source_callsite, SourceCoreRaceDrawContext &) = 0;
};
class SourceCoreRaceDrawUnavailable final : public SourceCoreRaceDrawRequired
{
  public:
    void perform(SourceCoreRaceDrawChild, std::uint32_t, SourceCoreRaceDrawContext &) override;
};
// All references belong to the same Core. Root retains/guards their lifetime,
// completed scene epoch and one accepted outer invocation. No new owner here.
struct SourceCoreRaceDrawContext
{
    FrontendState &frontend;
    SessionOuterFrameServices &frames;
    PolygonSubmissionState &submission;
    PolygonSubmissionSink &sink;
    LoadingFrameDevice &device;
    LightGammaDevice *const &gamma_publication;
    SourceCoreRaceDrawRequired &required;
};
enum class SourceCoreRaceViewKind
{
    normal,
    race_finished,
    no_player
};
enum class SourceCoreRaceDrawKind
{
    not_race,
    rendered
};
// Full source order. Explicit required groups prevent an incomplete normal
// route from returning success. No source update, input sampling or activation.
SourceCoreRaceDrawKind draw_source_core_race_4427b0(SourceCoreRaceDrawContext &);
std::uint32_t draw_source_core_views_442640(SourceCoreRaceDrawContext &);
SourceCoreRaceViewKind draw_source_core_view_4425d0(SourceCoreRaceDrawContext &);
// Actual source42E870 through the final61C508 store. Includes the SECOND
// 402E90, then42DCF0 on its current LightCameraView. Actual PMD/sprite/strip
// owners reach42B040; clipping/projection/stamp never run after child failure.
void prepare_source_core_world_42e870(const std::shared_ptr<RaceSessionPlatform> &,
                                      LightGammaDevice *const &source_gamma);
} // namespace sgn::retail
