#include "sgn/session_outer_frame.hpp"
#include "sgn/race_session.hpp"
#include "sgn/contact_player.hpp"
#include "sgn/retail_screen_polygon_submission.hpp"
#include "sgn/race_overlay_textures.hpp"
// The menu invocation borrows the same Session and retained drawing owners.
#include "sgn/race_session_menu.hpp"
#include <utility>

namespace sgn::retail
{
namespace
{
// Pinned D3D PE4a1d234c...,4421F0..4421F2 = 33 C0 C3.
// Complete recovered XOR EAX,EAX;RET, not a substitute for missing behavior.
std::uint32_t source_render_gate_4421f0() noexcept
{
    return 0;
}
} // namespace

SessionOuterSceneResult prepare_session_outer_scene_442640(SessionOuterFrameContext &f)
{
    f.textures.require_for(f.session.shared_platform()); // host wiring before source writes
    auto &s = f.session.state();
    SessionOuterSceneResult result;
    const auto view = [&](std::int32_t selector) {
        select_viewport(s.viewport, selector); // full424C30
        // New invocation borrows AFTER controller/viewport selection. No
        // resource/alpha/material spans escape into a later child or view.
        const auto resources = f.sources.borrow_world_resources();
        result.views[result.view_count++] = prepare_session_scene_frame(f.session, f.materials, f.capabilities,
                                                                        f.scratch, f.sink, resources, f.textures);
    };
    // 442640..44275B. Source reads unsigned576BE0 ONCE. Split0 retains the
    // entry controller; split>4 performs no inner views but still the suffix.
    const auto split = s.split_setting;
    switch (split)
    {
    case 0:
        view(1);
        break;
    case 1:
        s.controls.current = 0;
        view(6);
        s.controls.current = 1;
        view(7);
        break;
    case 2:
        s.controls.current = 0;
        view(8);
        s.controls.current = 1;
        view(9);
        break;
    case 3:
        s.controls.current = 0;
        view(10);
        s.controls.current = 1;
        view(11);
        s.controls.current = 2;
        view(12);
        break;
    case 4:
        s.controls.current = 0;
        view(13);
        s.controls.current = 1;
        view(14);
        s.controls.current = 2;
        view(15);
        s.controls.current = 3;
        view(16);
        break;
    default:
        break;
    }
    select_viewport(s.viewport, 1);               // 442760..442765, even after split0/view1
    contact::player::restore_primary(s.controls); // 44276A ->402580
    s.selected_hud = s.controls.current;          // 44276F..442779, current+210 identity
    {
        // Nonmovable binder owns its audio/transition/results/drawing views.
        // It must outlive the complete synchronous call and all its children.
        SessionMenuInvocation invocation(f.session, f.materials, f.scratch, f.textures, f.restart, f.sink);
        auto &menu = invocation.view();
        if (&menu.sink != &f.sink)
            throw FormatError("outer menu uses a different submission sink");
        submit_pause_menu_4473f0(menu); // 44277E, exactly once
    }
    // Do not recheck pause/scene/pending and do not restore controller/HUD.
    // The synchronous menu callback may have changed them or restarted race.
    submit_session_results_449fb0(f); // 442783, full results source body
    {
        auto countdown = f.sources.borrow_countdown();   // fresh live bindings
        submit_race_countdown_449b50(countdown, f.sink); // 442788, reuse Hud body
    }
    submit_session_demo_40e030(f); // 44278D, full demo source body
    {
        auto fade = f.sources.borrow_age_fade();
        draw_pause_age_fade_442200(fade, f.sink); // 442792 tail; no extra stores
    }
    return result;
}

SessionOuterFrameResult prepare_session_outer_frame_4427b0(SessionOuterFrameContext &f)
{
    f.textures.require_for(f.session.shared_platform()); // host admission before source/device work
    auto &s = f.session.state();
    if (s.scene_mode != 1)
        return {SessionOuterFrameKind::not_race, std::nullopt}; // 4427B0..B7
    ++f.device_words.render_counter;                            // 4427B9, modulo32, before source gate
    if (source_render_gate_4421f0() == 1)
        return {SessionOuterFrameKind::source_gate, std::nullopt};
    prepare_loading_surface_432a60(f.device);                          // 4427C9, exact existing Flip boundary
    begin_loading_scene_433ee0(f.device_words.scene_active, f.device); // 4427CE
    // 4427D3..4427E7: cropped must equal1; split is a separate nonzero test.
    // Read these live AFTER the API calls, not from outer-entry snapshots.
    if (s.viewport.cropped == 1 || s.split_setting != 0)
    {
        auto loading = f.sources.borrow_loading_frame();
        clear_loading_frame_4328c0(loading, 0, f.sink, f.device);
    }
    auto scene = prepare_session_outer_scene_442640(f);         // 4427EC
    reset_screen_material_cache_42e420(s.ui.selected_material); // 4427F1
    {
        // 432F60 is mandatory even when per-view4425D0 already consumed the
        // screen queue, and even when count==0 (trailing flags/state effects).
        const auto &bank = f.materials.validate_for(f.session);
        std::array<PolygonSubmissionVertex, 8> local{}; // NEW host opaque UV1 bits
        auto screen_access = lock_screen_polygon_owner(f.session.shared_platform());
        submit_screen_polygons_432f60({live_screen_polygon_const_reader(screen_access), screen_access.count_word(),
                                       s.ui.selected_material, bank.names.materials.active_materials(), local,
                                       s.viewport},
                                      f.sink);
    }
    finalize_loading_frame_431a80(f.device_words.finalized, f.device); // 4427FB ->423430
    end_loading_scene_433f20(f.device_words.scene_active, f.device);   // 442800 tail
    return {SessionOuterFrameKind::rendered, std::move(scene)};
}

void SessionOuterFrameCadence::begin(std::uint64_t serial)
{
    if (poisoned_ || active_ || (last_ && serial <= *last_))
        throw FormatError("outer source frame duplicated, reentered or poisoned");
    last_ = serial;
    active_ = true; // consume BEFORE any mutable source call
}
void SessionOuterFrameCadence::complete() noexcept
{
    active_ = false;
}
void SessionOuterFrameCadence::poison() noexcept
{
    active_ = false;
    poisoned_ = true;
}
SessionOuterFrameResult prepare_session_outer_frame_once(SessionOuterFrameContext &f, SessionOuterFrameCadence &cadence,
                                                         std::uint64_t accepted_outer_serial)
{
    f.textures.require_for(f.session.shared_platform()); // reject before consuming host cadence
    cadence.begin(accepted_outer_serial);
    try
    {
        auto result = prepare_session_outer_frame_4427b0(f);
        cadence.complete();
        return result;
    }
    catch (...)
    {
        cadence.poison();
        detach_world_polygon_level(f.session.state().polygon_frame);
        throw;
    }
}
} // namespace sgn::retail
