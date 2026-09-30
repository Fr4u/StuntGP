#include "sgn/source_core_race_update.hpp"
#include "sgn/source_audio_region.hpp"
#include "sgn/source_race_lifecycle.hpp"
#include "sgn/source_camera_dispatch.hpp"
#include "sgn/source_impact_particle_update.hpp"
#include "sgn/source_score_popup_update.hpp"
#include "sgn/source_numeric_win64.hpp"
#include "sgn/menu_keyboard_acquisition.hpp"
#include "sgn/track_part_animation.hpp"
#include "sgn/effect_particle_lifetime.hpp"
#include "sgn/source_menu_audio_maintenance.hpp"
#include "sgn/source_music_calls.hpp"
#include "sgn/race_session_music.hpp"
#include "sgn/race_session_pause.hpp"
#include "sgn/race_session_scene.hpp"
#include "sgn/retail_camera_initialization.hpp"
#include "sgn/retail_hud_lifetime.hpp"
#include "sgn/race_session.hpp"
#include "sgn/host_profiler.hpp"
#include "sgn/host_presentation.hpp"

namespace sgn::retail
{
namespace
{
std::uint32_t site(SourceCoreRaceUpdateChild child)
{
    switch (child)
    {
    case SourceCoreRaceUpdateChild::vehicles:
        return 0x44285a;
    case SourceCoreRaceUpdateChild::demo:
        return 0x44286e;
    }
    throw FormatError("unknown mandatory source update child");
}
class CoreOuterCalls final
{
    FrontendState &frontend_;
    RaceSessionPlatform &platform_;
    FrontendPerformanceCounter &counter_;
    const WindowInputBorrow &input_;
    RaceTextureReplacement &textures_;
    SourceMenuSampleNative &samples_;
    SourceCoreRaceUpdateRequired &required_;
    SceneTransitionServices &transition_;
    SourceOuterTrace *trace_;
    void entered(SourceOuterBoundary boundary)
    {
        if (trace_)
            trace_->entered.push_back(boundary);
    }
    RaceSessionState &world()
    {
        return platform_.state();
    }
    void required(SourceCoreRaceUpdateChild child)
    {
        required_.perform(child, frontend_, textures_);
    }

  public:
    CoreOuterCalls(FrontendState &frontend, FrontendPerformanceCounter &counter, const WindowInputBorrow &input,
                   RaceTextureReplacement &textures, SourceMenuSampleNative &samples,
                   SourceCoreRaceUpdateRequired &required, SceneTransitionServices &transition, SourceOuterTrace *trace)
        : frontend_(frontend), platform_(*frontend.shared_platform()), counter_(counter), input_(input),
          textures_(textures), samples_(samples), required_(required), transition_(transition), trace_(trace)
    {
    }
    RetailTickClock &clock()
    {
        return world().tick_clock;
    }
    void numeric_429510()
    {
        entered(SourceOuterBoundary::numeric);
        prepare_source_numeric_win64_429510();
    }
    void camera_4023f0()
    {
        entered(SourceOuterBoundary::camera_reset);
        camera_begin_outer_frame(world().controls);
    }
    std::optional<std::uint32_t> input_45f210_40dad0()
    {
        SGN_PROFILE_SCOPE("update.input_45f210_40dad0"); // NEW HOST (R30) profiler marker only
        entered(SourceOuterBoundary::input_pair);
        const auto result = acquire_dispatch_source_platform_input(frontend_, input_.keyboard, input_.direct_input);
        if (result.status != SourceInputStatus::completed)
            return result.physical_slot;
        return std::nullopt;
    }
    void clock_442490()
    {
        SGN_PROFILE_SCOPE("update.clock_442490");
        entered(SourceOuterBoundary::sample_clock);
        auto &s = world();
        sample_frontend_clock_442490(s.tick_clock, s.source_scheduler.fixed_mode_a, s.source_scheduler.fixed_mode_b,
                                     counter_);
    }
    void iteration_442838()
    {
        SGN_PROFILE_SCOPE("update.tick_442838_total");
        entered(SourceOuterBoundary::entered_iteration);
        auto &s = world();
        if (advance_pause_gate_4423d0(session_pause_gate_view(s)) == 0)
        {
            {
                SGN_PROFILE_SCOPE("update.camera_405210");
                advance_source_camera_405210(platform_);
            } // 442841, actual CAM/PMD/query before track lookup
            auto &scene = platform_.source_scene_resources();
            if (const auto handle = scene.model_handle(3))
            {
                SGN_PROFILE_SCOPE("update.track_anim_427140"); // NEW HOST (R30) profiler marker only
                // Source descriptor/count, no decoded/native ModelResource path.
                const auto model = scene.models().source_at(*handle);
                advance_track_part_animations(s.track_part_animations ? s.track_part_animations->records()
                                                                      : std::span<TrackPartAnimation>{},
                                              model); // 442846 ->427140, original null/count guards
            }
            {
                SGN_PROFILE_SCOPE("update.effect_particles_45e1d0"); // NEW HOST (R30) profiler marker only
                contact::effects::update_effect_particles(
                    s.effect_particles, s.gravity.vector,
                    s.effect_resources.animations.frames()); // 44284B ->45E1D0, fresh actual descriptor projection
            }
            {
                SGN_PROFILE_SCOPE("update.impact_415df0");
                update_source_impact_particles_415df0(platform_);
            } // 442850, same PMD/contact/particle owners
            {
                SGN_PROFILE_SCOPE("update.lifecycle_443a80");
                advance_source_race_lifecycle_443a80(frontend_, samples_, textures_);
            } // 442855, same retained owners
            host_presentation::pass_yield_point(); // NEW HOST (R33): may present one interpolated frame, no source work
            {
                SGN_PROFILE_SCOPE("update.vehicles_418a90");
                required(SourceCoreRaceUpdateChild::vehicles);
            } // 44285A ->418A90
            host_presentation::pass_yield_point(); // NEW HOST (R33): may present one interpolated frame, no source work
            {
                SGN_PROFILE_SCOPE("update.popups_401770");
                advance_source_score_popups_401770(frontend_, samples_);
            } // 44285F, same popup/RNG/native audio owners
        }
        {
            SGN_PROFILE_SCOPE("update.overlays_hud_442be0_447c90"); // NEW HOST (R30) profiler marker only
            update_controller_overlays(s.controls, s.overlays);     // 442864 ->442BE0, even if gate skipped
            update_race_hud({s.controls, s.hud_countdown, s.hud_layout, s.selected_hud, s.hud_margins},
                            s.vehicles.physics, s.vehicles.race); // 442869 ->447C90
        }
        if (s.hud_block == 1)
        {
            SGN_PROFILE_SCOPE("update.demo_40de37");   // NEW HOST (R30) profiler marker only
            required(SourceCoreRaceUpdateChild::demo); // 40DE37 not-exact1 -> RET40DE74
        }
        SGN_PROFILE_SCOPE("update.tick_music_441ec0_441f70"); // NEW HOST (R30) profiler marker only
        advance_retail_tick(s.simulation_tick, s.tick_clock); // 442873 ->441EC0
        advance_source_music_delay_441f70(platform_);         // 442878, before next scheduled/observed comparison
    }
    void audio_446110_462280()
    {
        SGN_PROFILE_SCOPE("update.audio_446110_462280");
        entered(SourceOuterBoundary::audio_pair);
        const auto result = update_source_audio_environment_446110(platform_, required_);
        if (result.status != SourceAudioEnvironmentStatus::completed)
            throw SourceCoreRaceUpdateBoundary(0x44288c, result.blocked_address,
                                               "446110 requires its actual source spatial/property receiver");
        maintain_source_menu_audio_462280(frontend_); // name retained; this body has no scene2 guard
    }
    std::optional<std::uint32_t> transition_441dc0()
    {
        SGN_PROFILE_SCOPE("update.transition_441dc0"); // NEW HOST (R30) profiler marker only
        entered(SourceOuterBoundary::scene_transition);
        auto view = scene_transition_world(world());
        (void)dispatch_scene_transition(view, frontend_.source.scene, transition_);
        return std::nullopt; // only after the actual synchronous transition returns
    }
};
} // namespace
void SourceCoreRaceUpdateUnavailable::perform(SourceCoreRaceUpdateChild child, FrontendState &frontend,
                                              RaceTextureReplacement &)
{
    frontend.shared_platform()->require_owner_thread();
    throw SourceCoreRaceUpdateBoundary(
        site(child), static_cast<std::uint32_t>(child),
        "mandatory source-world update child is not yet bound; preview substitution is forbidden");
}
std::uint32_t SourceCoreRaceUpdateUnavailable::contains_4278a0(const AudioEnvironmentRegion &region,
                                                               const CameraVector &point)
{
    // PORT-FIX-001, explicitly authorized: initialize the original undefined
    // direction W; retain the complete source x87/plane predicate.
    return source_audio_environment_contains_4278a0(region, point);
}
SourceOuterOutcome update_source_core_race_442810(FrontendState &frontend, FrontendPerformanceCounter &counter,
                                                  const WindowInputBorrow &input, RaceTextureReplacement &textures,
                                                  SourceMenuSampleNative &samples,
                                                  SourceCoreRaceUpdateRequired &required,
                                                  SceneTransitionServices &transition, SourceOuterTrace *trace)
{
    const auto platform = frontend.shared_platform();
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry() || &frontend.world() != &platform->state())
        throw FormatError("442810 requires the same source platform/frontend world");
    CoreOuterCalls calls(frontend, counter, input, textures, samples, required, transition, trace);
    return detail::source_outer_body_442810(calls); // one acquisition and QPC sample; source unsigned catch-up loop
}
} // namespace sgn::retail
