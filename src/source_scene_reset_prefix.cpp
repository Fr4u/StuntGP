#include "sgn/source_scene_reset_prefix.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_race_start.hpp"
#include "sgn/retail_world_globals.hpp"
#include "sgn/vehicle_score.hpp"

namespace sgn::retail
{
namespace
{
void reset_frontend_launch_word_443050(FrontendLaunchState &state) noexcept
{
    // Literal60DF90 store. The source word is not a new host pointer/count.
    state.catalog_count = 0x005096c0u;
}
// clang-format off: keep the original closing comment (FixNamespaceComments would replace it)
} //anonymous frontend-only leaf
// clang-format on
void reset_scene_effects_45e0e0(contact::effects::EffectParticleBank &bank) noexcept
{
    for (auto &particle : bank.particles)
        particle.active = 2;
    bank.active_signal = 0; // 62D478, after all64 active stores; retain payload.
}
SourceSceneResetResult reset_source_scene_445fa0_prefix(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    auto &state = platform.state();
    reset_frontend_launch_word_443050(state.frontend_launch); // 445FA3
    for (unsigned i = 0; i < 6; ++i)
    { // six physical records, including inactive cars
        state.vehicles.race[i].active = 0;
        state.vehicles.models[i].model.reset();
        state.vehicles.models[i].shadow.reset();
        refill_vehicle(state.vehicles.parameters[i], state.vehicles.physics[i]);
        refill_impact_energy(state.vehicles.physics[i], state.vehicles.contact[i]);
    }
    state.gravity.factor = 4.0f; // 445FE8, before the600-byte REP clear
    static_assert(sizeof(state.cars) == 600);
    state.cars = {};                                    // 445FF2: only60F02C..60F283, not selection or ghosts
    initialize_world_gravity(state.gravity);            // 445FF4 ->441F90, retains unit.W
    clear_score_popups(state.score_popups);             // 445FF9 ->401000, retains sound cache
    reset_scene_effects_45e0e0(state.effect_particles); // 445FFE
    state.source_overlay_selection_61b634 = 0;          // 44600F, same world; no resource owner required
    state.outer.loop_exit_word_612948 = 0;              // 446015: source process-loop word
    state.frame_pause = 0;                              // 44601B:61294C, not pause_flag
    state.ui.second_pause = 0;                          // 446021:612958
    state.normal_finish.confirm_pressed = 0;            // 446027:612968
    state.outer.source_word_612974 = 0;                 // 44602D: no helper call in this reset
    state.outer.source_word_612978 = 0;                 // 446033, even when the first word was0
    state.outer.source_word_612980 = 0;                 // 446039
    state.outer.render_counter = 0;                     // 44603F: same owner as4427B9/4458B0
    state.outer.source_word_612988 = 0;                 // 446045
    state.outer.source_word_61298c = 0;                 // 44604B
    state.outer.source_word_612994 = 0;                 // 446051
    state.demo_ticks = 0;                               // 446057, before612998
    state.outer.source_word_612998 = 0;                 // 44605D
    state.hud_block = 0;                                // 446063
    state.outer.source_word_6129a0 = 0;                 // 446069
    state.outer.source_word_612be4 = 0;                 // 44606F
    state.normal_finish.field_612bf0 = 0;               // 446075
    state.grid.start_spline.reset();                    // 44607B: clear borrowed identity only
    for (unsigned i = 0; i < 6; ++i)
        state.finish.order[i].reset();   // 446081:6DWORD, not16
    state.finish.count = 0;              // 44608D, retain best totals
    state.impact_particles.emitted = 0;  // 446093
    state.impact_particles.cursor = 0;   // 446099
    state.impact_particles.throttle = 0; // 44609F, retain rate_selector61297C
    static_assert(sizeof(state.impact_particles.particles) == 0x8800);
    state.impact_particles.particles = {};                                                   // 4460A5:8704DWORD
    state.impact_draw_list.slots.fill(contact::effects::ImpactParticleDrawList::null_value); // 4460B1 sourceNULLs
    state.automatic.initialized = 0; // 4460B3: retain routes and cached payload
    state.impact_draw_list.count =
        contact::effects::ImpactParticleDrawList::null_value;      // 4460B9 sourceNULL, not listbase
    state.ui.loading_tile_base = 0;                                // 4460BF
    state.outer.source_word_61b62c = 0;                            // 4460C5
    return {SourceSceneResetStatus::callee_required, 0x4460cb, 0}; // before442100, no clock read
}
} // namespace sgn::retail
