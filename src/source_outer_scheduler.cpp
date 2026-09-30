#include "sgn/source_outer_scheduler.hpp"
#include "sgn/source_scene_reset_composition.hpp"
#include "sgn/source_numeric_win64.hpp"
#include "sgn/menu_keyboard_acquisition.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_iteration.hpp"
#include "sgn/race_session_music.hpp"
#include "sgn/race_session_scene.hpp"
#include "sgn/retail_camera_initialization.hpp"
#include "sgn/session_effect_resources.hpp"
#include <limits>
#include <utility>

namespace sgn::retail
{
namespace
{
struct MissingSceneChild final
{
    std::uint32_t address;
};
// Actual431810,42EAA0 and pure460AF0 on the retained Session platform.
// Active audio stops at source QueryInterface; an inactive guard returns.
class WarmImageReleaseServices final : public SceneTransitionServices
{
    SessionOuterCapture &outer_;
    SourceOuterTrace &trace_;
    RaceSessionPlatform &platform_;
    FrontendPerformanceCounter &counter_;

  public:
    WarmImageReleaseServices(SessionOuterCapture &outer, SourceOuterTrace &trace, RaceSessionPlatform &platform,
                             FrontendPerformanceCounter &counter)
        : outer_(outer), trace_(trace), platform_(platform), counter_(counter)
    {
    }
    void perform(SceneOperation op) override
    {
        const auto prefix = perform_source_scene_initialization_prefix(op, platform_, counter_);
        if (prefix.status == SourceScenePrefixStatus::completed)
            return;
        if (prefix.status == SourceScenePrefixStatus::child_required)
            throw MissingSceneChild{prefix.blocked_address};
        if (op == SceneOperation::release_image_resources)
        {
            outer_.release_image_resources_431810();
            return;
        }
        if (op == SceneOperation::release_allocations)
        {
            platform_.release_scene_allocations_42eaa0();
            return;
        }
        trace_.missing_scene_operations.push_back(op);
        throw MissingSceneChild{static_cast<std::uint32_t>(op)};
    }
    void select_audio_environment(std::uint32_t type) override
    {
        const auto selected = select_source_audio_environment_460af0(platform_, type);
        if (selected.status != SourceAudioEnvironmentStatus::completed)
            throw MissingSceneChild{selected.blocked_address};
    }
    void initialize_menu_viewport(std::uint32_t) override
    {
        throw MissingSceneChild{0x424c30};
    }
    void reset_tick_clock() override
    {
        reset_platform_source_clock_442100(platform_, counter_);
    }
};
class ActualOuterCalls final
{
    SourceRaceOuterContext &context_;
    RaceOverlayTextures &textures_;
    SessionOuterCapture &outer_;
    void entered(SourceOuterBoundary b)
    {
        context_.trace.entered.push_back(b);
    }

  public:
    ActualOuterCalls(SourceRaceOuterContext &c, RaceOverlayTextures &t, SessionOuterCapture &outer)
        : context_(c), textures_(t), outer_(outer)
    {
    }
    RetailTickClock &clock()
    {
        return context_.session.state().tick_clock;
    }
    void numeric_429510()
    {
        entered(SourceOuterBoundary::numeric);
        prepare_source_numeric_win64_429510();
    }
    void camera_4023f0()
    {
        entered(SourceOuterBoundary::camera_reset);
        // 4023F0 borrows only the SAME controls; no scene/spline/track getter is needed.
        camera_begin_outer_frame(context_.session.state().controls);
    }
    std::optional<std::uint32_t> input_45f210_40dad0()
    {
        entered(SourceOuterBoundary::input_pair);
        const auto input = context_.input.borrow_initialized(context_.session);
        const auto result = acquire_dispatch_session_input(context_.session, input.keyboard, input.direct_input);
        if (result.status != SourceInputStatus::completed)
            return result.physical_slot;
        return std::nullopt;
    }
    void clock_442490()
    {
        entered(SourceOuterBoundary::sample_clock);
        auto &s = context_.session.state();
        sample_frontend_clock_442490(s.tick_clock, s.source_scheduler.fixed_mode_a, s.source_scheduler.fixed_mode_b,
                                     context_.counter);
    }
    void iteration_442838()
    {
        entered(SourceOuterBoundary::entered_iteration);
        // Fresh invocation-only descriptor views; never constructor-cached.
        const auto frames = borrow_session_effect_animation_frames(context_.session);
        advance_session_iteration(context_.session, textures_, frames, context_.tick_audit);
    }
    void audio_446110_462280()
    {
        entered(SourceOuterBoundary::audio_pair);
        maintain_session_audio(context_.session);
    }
    std::optional<std::uint32_t> transition_441dc0()
    {
        entered(SourceOuterBoundary::scene_transition);
        auto view = scene_transition_world(context_.session.state());
        WarmImageReleaseServices partial(outer_, context_.trace, *context_.session.shared_platform(), context_.counter);
        auto &services = context_.transition_services ? *context_.transition_services : partial;
        try
        {
            (void)dispatch_scene_transition(view, context_.frontend.source.scene, services);
        }
        catch (const MissingSceneChild &stopped)
        {
            return stopped.address;
        }
        catch (const SourceSceneResetBoundary &stopped)
        {
            return stopped.result.blocked_address;
        }
        return std::nullopt;
    }
};
} // namespace

SourceSceneResetResult SessionOuterCapture::reset_scene_prefix_445fa0() &
{
    // textures_ survives the earlier GPU-bank release; it is never replaced.
    return reset_source_scene_445fa0_prefix(*session_.shared_platform());
}

CapturedSourceRaceOuterFrame SessionOuterCapture::advance_and_capture_source_frame(
    SourceRaceOuterContext &context, const SessionOuterBoundCaptureRequest &request, LightGammaDevice *expected_gamma) &
{
    session_.shared_platform()->require_owner_thread();
    SessionOuterFrameServices sources(session_, materials_, scratch_, textures_, request.selected_depth,
                                      request.world_resources, expected_gamma);
    // Reference-only bindings survive the actual update. Drawing getters read
    // current resources after it; this provider remains alive through seal.
    const SessionOuterCaptureRequest bound{request.restart,
                                           sources,
                                           sources.device_words(),
                                           sources.capabilities(),
                                           request.accepted_outer_serial,
                                           request.simulation_tick,
                                           request.limits};
    return advance_and_capture_source_frame(context, bound);
}

CapturedSourceRaceOuterFrame SessionOuterCapture::advance_and_capture_source_frame(
    SourceRaceOuterContext &context, const SessionOuterCaptureRequest &request) &
{
    session_.shared_platform()->require_owner_thread();
    if (&context.session != &session_ || &context.frontend.world() != &session_.state() ||
        context.frontend.shared_platform().get() != session_.shared_platform().get())
        throw FormatError("source outer binding has another session/world/platform");
    // Validate native lifetime and exact Session binding before numeric/source writes.
    context.input.require_session(session_);
    (void)context.input.borrow_initialized(session_);
    // Scope/admission guards are host checks, not invented source predicates.
    if (session_.state().scene_mode != 1 || session_.state().hud_block != 0)
        throw FormatError("source outer requires the entered local non-demo race branch");
    if (session_.state().tick_clock.increment == 0 ||
        session_.state().tick_clock.increment > std::numeric_limits<std::uint32_t>::max() / 60u)
        throw FormatError("source outer clock has no valid source initialization");
    if (capture_.capturing() || capture_.device().capturing() || capture_.poisoned())
        throw FormatError("source outer capture is already active or poisoned");
    // A failed/nested begin has no source writes and must not poison an
    // already active caller. Reuse the sole existing cadence guard.
    cadence_.begin(request.accepted_outer_serial);
    bool opened_capture = false;
    try
    {
        ActualOuterCalls calls(context, textures_, *this);
        auto outcome = detail::source_outer_body_442810(calls);
        if (outcome.status != SourceOuterStatus::completed)
        {
            cadence_.poison();
            return {outcome, std::nullopt};
        }
        // Source44EB2A runs even when441DC0 changed scene.4427B0 itself makes
        // the scene predicate. No early recheck or manufactured empty clear.
        // CPU capture starts AFTER update. Source texture replacements while
        // idle go through this same capture owner's actual replacement bank.
        // Request.simulation_tick is intentionally NOT used:441EC0/transition
        // can have changed it. It is host metadata, not an outer cadence clock.
        capture_.begin_source_frame(session_.state().simulation_tick, request.limits);
        opened_capture = true;
        SessionOuterFrameContext frame{
            session_,        materials_,      request.capabilities, scratch_,       capture_.device(), textures_,
            request.restart, request.sources, request.device_words, loading_device_};
        context.trace.entered.push_back(SourceOuterBoundary::draw);
        auto source = prepare_session_outer_frame_4427b0(frame);
        context.trace.entered.push_back(SourceOuterBoundary::capture_seal);
        auto submission = capture_.seal_source_frame();
        if (source.kind != SessionOuterFrameKind::rendered)
            submission.reset();
        cadence_.complete();
        CapturedSessionOuterFrame captured{request.accepted_outer_serial, std::move(source), std::move(submission)};
        return {outcome, std::move(captured)};
    }
    catch (...)
    {
        cadence_.poison();
        detach_world_polygon_level(session_.state().polygon_frame);
        if (opened_capture)
            capture_.discard_failed_source_frame();
        throw;
    }
}
} // namespace sgn::retail
