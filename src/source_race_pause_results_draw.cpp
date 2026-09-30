#include "sgn/source_race_pause_results_draw.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session_scene.hpp"
#include "sgn/retail_pause_menu.hpp"
#include "sgn/retail_results_submission.hpp"
#include "sgn/session_pause_meter_labels.hpp"
#include "sgn/source_menu_sample_native.hpp"
#include "sgn/source_application_literals.hpp"
#include "sgn/source_music_calls.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
template <class T> T read(const T &value)
{
    return static_cast<const volatile T &>(value);
}
template <class T> void put(T &target, T value)
{
    static_cast<volatile T &>(target) = value;
}
RaceHudView hud(RaceSessionState &w)
{
    return {w.controls, w.hud_countdown, w.hud_layout, w.selected_hud, w.hud_margins};
}
std::shared_ptr<RaceSessionPlatform> admit(SourceCoreRaceDrawContext &c, W at)
{
    const auto p = c.frontend.shared_platform();
    if (!p)
        throw SourceAllocationError(at, "pause/results platform expired");
    p->require_owner_thread();
    if (!p->source_construction_only_entry())
        throw SourceAllocationError(at, "pause/results require source platform");
    c.frames.require_source_frontend(c.frontend);
    return p; // host ownership only; no drawing/resource acquisition on early returns
}
// Invocation-only reference adapter. Each actual child borrows current
// descriptor/alpha/material/font owners after any previous texture upload.
class Drawing final : public PauseMenuDrawingSources, public ResultsDrawingSources
{
    SourceCoreRaceDrawContext &c;
    RaceSessionState &w;
    LoadingFrameWorld frame()
    {
        auto f = c.frames.borrow_loading_frame();
        if (&f.submission != &c.submission || f.quads.descriptor.data() != w.ui.descriptor.data() ||
            f.quads.vertices.data() != c.submission.alpha_vertices.data() ||
            &f.quads.selected_material != &w.ui.selected_material || &f.quads.viewport != &w.viewport)
            throw FormatError("pause/results received foreign render owners");
        return f;
    }
    TextSubmission text(const LoadingFrameWorld &f)
    {
        return {w.ui.fonts,        w.ui.selected_material, c.submission.specular_enabled,
                f.quads.materials, w.view_quality.second,  w.view_quality.legacy_driver_restriction,
                w.viewport};
    }

  public:
    explicit Drawing(SourceCoreRaceDrawContext &context) : c(context), w(context.frontend.world()) {}
    TextSubmission borrow_text() override
    {
        auto f = frame();
        return text(f);
    }
    std::span<const std::uint8_t> borrow_result_initial_text(W address, W site) override
    {
        // Resolve the sole App through this same retained Platform only when
        // the current pointer actually names App bytes. No eager text read.
        const auto app = c.frontend.shared_platform()->source_application_literals();
        return app.result_initial_text(address, site);
    }
    QuadSubmission borrow_quad() override
    {
        auto f = frame();
        return {w.ui.descriptor, f.quads.vertices, w.ui.selected_material, f.quads.materials, w.viewport};
    }
    std::span<PolygonSubmissionVertex> borrow_alpha_vertices() override
    {
        c.frontend.shared_platform()->require_owner_thread();
        return c.submission.alpha_vertices;
    }
    void submit_position_4487d0(PolygonSubmissionSink &sink, W index, float x, float y, W depth) override
    {
        if (&sink != &c.sink)
            throw FormatError("results position received foreign sink");
        auto f = frame();
        RaceHudLeafInputs inputs{hud(w),          w.vehicles,       w.clock,         w.records,     w.ranking,
                                 w.cars,          w.game_text,      w.split_setting, w.frame_pause, w.ui.stars,
                                 w.ui.descriptor, f.quads.vertices, text(f)};
        submit_race_position_4487d0(inputs, c.sink, index, x, y, depth);
    }
};
class Actions final : public PauseMenuActions
{
    SourceCoreRaceDrawContext &c;
    RaceSessionPlatform &platform;
    RaceSessionState &w;
    SourceMenuSampleNative &samples;
    SourcePauseRequired &required;
    void resume()
    {
        resume_pause_442350(PauseResumeWorld{w.frame_pause,
                                             w.pause.status,
                                             w.ui.second_pause,
                                             w.pause.physical_input,
                                             hud(w),
                                             w.overlays,
                                             w.vehicles.physics,
                                             w.audio_level,
                                             {w.audio_level.output_a, w.audio_stream.playback_active,
                                              w.audio_stream.field_e100, w.audio_maintenance.music.gain_step}});
    }
    void stop_music()
    {
        // 45F9D0's inactive/output-zero gates precede the dependency lookup.
        if (read(w.audio_stream.playback_active) == 0 || read(w.audio_level.output_a) == 0)
            return;
        const auto binding = platform.source_music_calls();
        if (!binding)
            throw SourceCoreRaceDrawBoundary(0x446af0, 0x45f9d0, "active source music has no retained stop receiver");
        binding->stop(); // same source460330/native wave owners, then45F9E7 store
    }
    void camera()
    {
        auto next = read(w.selected_camera_preset) + 1u;
        put(w.selected_camera_preset, next); // 446BBA before signed wrap
        if (std::bit_cast<std::int32_t>(next) > 4)
        {
            next = 1;
            put(w.selected_camera_preset, next);
        } // 446BC7
        const auto input = read(w.pause.physical_input.token); // 446BCD
        if (input < 1 || input > 4)
            throw SourceAllocationError(0x446bd2, "pause camera physical input outside source owner");
        const auto vehicle = read(w.frontend_launch.input_bindings[input - 1].vehicle); // 446BD2 before selector gate
        if (next - 1u > 3u)
            return; // 446BF2 does not dereference the computed vehicle
        constexpr W sites[]{0x446bfb, 0x446c00, 0x446c05, 0x446c0a};
        constexpr W entries[]{0x40d280, 0x40d200, 0x40d300, 0x40d380};
        required.camera_preset(c.frontend, sites[next - 1], entries[next - 1], vehicle);
    }
    void audio_increment(W &input)
    {
        const auto next = read(input) + 10u;
        put(input, next);
        if (next > 100u)
            put(input, 0u);
        update_level_audio(w.audio_level, w.options); // 446AAC/446ADC tail460BC0
    }

  public:
    Actions(SourceCoreRaceDrawContext &context, RaceSessionPlatform &owner, SourceMenuSampleNative &sample,
            SourcePauseRequired &child)
        : c(context), platform(owner), w(context.frontend.world()), samples(sample), required(child)
    {
    }
    void sound_461150(W sample) override
    {
        samples.start_461150(c.frontend, sample, 0, 0, 255, 0, 1, nullptr);
    }
    void invoke(W identity) override
    {
        switch (identity)
        {
        case 0x446b40:
            resume();
            put(w.pause.physical_input.token, 0u);
            put(w.menu.release_count, 0u);
            return;
        case 0x446af0:
            stop_music();
            required.restart_40d500(c.frontend, 0x446af5);
            put(w.clock.countdown_pending, 4u);
            resume();
            put(w.menu.release_count, 0u);
            return;
        case 0x446b20: {
            set_pause_exit_marker_445350(w.options); // 446B20, retained before441E20
            auto transition = scene_transition_world(w);
            auto results = scene_race_result_world(w);
            request_scene_transition(transition, results); // 446B25, includes actual445530
            put(w.menu.release_count, 0u);
            return;
        }
        case 0x446a90:
            audio_increment(w.audio_level.input_b);
            return;
        case 0x446ac0:
            audio_increment(w.audio_level.input_a);
            return;
        case 0x446a70: {
            auto next = read(w.impact_particles.rate_selector) + 10u;
            put(w.impact_particles.rate_selector, next);
            if (next > 100u)
            {
                next = 0;
                put(w.impact_particles.rate_selector, next);
            }
            put(w.options.render_a, next);
            return;
        }
        case 0x446bb0:
            camera();
            return;
        default:
            throw SourceCoreRaceDrawBoundary(0x447241, identity, "pause latched callback has no reconstructed target");
        }
    }
};
} // namespace
void SourcePauseUnavailable::restart_40d500(FrontendState &, W site)
{
    throw SourceCoreRaceDrawBoundary(site, 0x40d500,
                                     "source pause restart requires full current-owner race initialization");
}
void SourcePauseUnavailable::camera_preset(FrontendState &, W site, W entry, W)
{
    throw SourceCoreRaceDrawBoundary(site, entry, "source pause camera requires direct-owner preset receiver");
}
void submit_source_pause_menu_4473f0(SourceCoreRaceDrawContext &c, RaceTextureReplacement &textures,
                                     SourceMenuSampleNative &samples, SourcePauseRequired &required)
{
    const auto p = admit(c, 0x4473f0);
    auto &w = c.frontend.world();
    Drawing drawing(c);
    Actions actions(c, *p, samples, required);
    SessionPauseMeterLabels labels(w.menu.meter_labels);
    PauseMenuDrawWorld world{
        {w.menu.release_count,
         w.menu.selected_row,
         w.menu.highlight,
         w.menu.overlay_ready,
         w.menu.requested_language,
         w.menu.rows,
         w.frontend_launch.input_bindings,
         w.menu_input.edges,
         {&w.menu.host_graphics_r24, &w.source_input.acquisition}}, // NEW HOST (R24) pause GRAPHICS row
        w.normal_finish.menu_requested,
        w.ui.second_pause,
        w.frame_pause,
        w.pause.physical_input,
        w.clock,
        w.split_setting,
        w.render_b,
        w.level.game_type,
        w.impact_particles.rate_selector,
        w.selected_camera_preset,
        w.audio_level,
        w.game_text,
        textures,
        w.ui.descriptor,
        w.ui.line_clip,
        labels,
        drawing,
        c.sink,
        actions};
    submit_pause_menu_4473f0(world); // one shared receiver, no captured source-state mirror
}
void submit_source_results_449fb0(SourceCoreRaceDrawContext &c, RaceTextureReplacement &textures)
{
    const auto p = admit(c, 0x449fb0);
    (void)p;
    auto &w = c.frontend.world();
    Drawing drawing(c);
    ResultsWorld world{hud(w),      w.vehicles, w.current_slot, w.normal_finish, w.ranking, w.cars,
                       w.game_text, w.clock,    textures,       w.ui.descriptor, drawing,   c.sink};
    submit_race_results_449fb0(world);
}
} // namespace sgn::retail
