#include "sgn/source_menu_frame.hpp"
#include "sgn/race_session_scene.hpp"
#include "sgn/retail_camera_initialization.hpp"
#include "sgn/source_numeric_win64.hpp"
#include "sgn/host_wide_menu.hpp"
#include "sgn/host_profiler.hpp"

namespace sgn::retail
{
void update_source_menu_frame_441f00(FrontendState &frontend, FrontendPerformanceCounter &counter,
                                     SourceMenuFrameChildren &calls, SceneTransitionServices &transitions)
{
    frontend.shared_platform()->require_owner_thread(); // new host ownership guard
    SGN_PROFILE_SCOPE("menu.update_441f00");            // NEW HOST (R35) diagnostics only
    auto &world = frontend.world();
    prepare_source_numeric_win64_429510();    // 441F00
    camera_begin_outer_frame(world.controls); // 441F05 ->4023F0, no level borrow
    {
        SGN_PROFILE_SCOPE("menu.input_45f210"); // NEW HOST (R35) diagnostics only
        calls.acquire_input_45f210(frontend);   // 441F0A
    }
    sample_frontend_clock_442490(world.tick_clock, world.source_scheduler.fixed_mode_a,
                                 world.source_scheduler.fixed_mode_b, counter); // 441F0F
    if (world.tick_clock.scheduled < world.tick_clock.observed)
    {
        std::uint32_t observed;
        do
        {
            calls.update_controls_40d860(frontend); // 441F25
            {
                SGN_PROFILE_SCOPE("menu.pages_43d200");     // NEW HOST (R35) diagnostics only
                update_source_menu_43d200(frontend, calls); // 441F2A
            }
            update_controller_overlays(world.controls, world.overlays);   // 441F2F
            advance_retail_tick(world.simulation_tick, world.tick_clock); // 441F34
            observed = world.tick_clock.observed;                         // 441F39, before REP MOVSD
            for (unsigned i = 0; i < 64; ++i)
                world.startup.fields_62dcf0[i] = world.startup.fields_62d480[i];
        } while (world.tick_clock.scheduled < observed); // 441F4F..441F57 unsigned JB
    }
    {
        SGN_PROFILE_SCOPE("menu.audio_462280"); // NEW HOST (R35) diagnostics only
        calls.maintain_audio_462280(frontend);  // 441F5B, even when no tick was due
    }
    SGN_PROFILE_SCOPE("menu.transition_dispatch"); // NEW HOST (R35) diagnostics only
    auto transition = scene_transition_world(world);
    (void)dispatch_scene_transition(transition, frontend.source.scene, transitions); // 441F60 tail
}
void draw_source_menu_frame_441d70(FrontendState &frontend, LoadingFrameWorld frame, PolygonSubmissionSink &sink,
                                   LoadingFrameDevice &device, SourceMenuFrameChildren &calls)
{
    frontend.shared_platform()->require_owner_thread(); // new host ownership guard
    auto &world = frontend.world();
    if (world.scene_mode != 2)
        return;                                             // 441D70..441D77, before any source effect
    SGN_PROFILE_SCOPE("menu.draw_441d70");                  // NEW HOST (R35) diagnostics only
    ++world.outer.render_counter;                           // 441D79; uint32 wrap is intentional
    calls.prepare_camera_402e90(frontend);                  // 441D7F
    prepare_loading_surface_432a60(device);                 // 441D84
    begin_loading_scene_433ee0(frame.scene_active, device); // 441D89
    clear_loading_frame_4328c0(frame, 0, sink, device);     // 441D8E zeroes ECX
    {
        SGN_PROFILE_SCOPE("menu.draw_pages_43d250"); // NEW HOST (R35) diagnostics only
        draw_source_menu_43d250(frontend, calls);    // 441D95
    }
    {
        host_wide_menu::DrawScope wide(host_wide_menu::Kind::fill); // NEW R20 (wide_menu) host tag only
        draw_loading_overlay_before_442a00(frame.quads, sink);      // 441D9A
        draw_loading_overlay_after_442a20(frame.quads, sink);       // 441D9F
    }
    finalize_loading_frame_431a80(frame.finalized, device); // 441DA4 actual thunk address
    calls.prepare_geometry_44fd60(frontend);                // 441DA9, after finalization
    end_loading_scene_433f20(frame.scene_active, device);   // 441DAE tail
}
} // namespace sgn::retail
