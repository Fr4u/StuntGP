#include "sgn/source_cold_initialize.hpp"
#include "sgn/source_numeric_win64.hpp"
#include "sgn/source_outer_scheduler.hpp"
#include "sgn/source_allocation_bank.hpp"
#include "sgn/source_boot_config.hpp"
#include "sgn/source_input_cold_init.hpp"
#include "sgn/source_cold_audio_input.hpp"
#include "sgn/race_session_scene.hpp"

namespace sgn::retail
{
void initialize_source_cold_442120(SourceColdInitializationContext &context)
{
    const auto &platform = context.frontend.shared_platform();
    platform->require_owner_thread();
    auto &world = platform->state();
    if (&context.frontend.world() != &world)
        throw FormatError("442120 frontend must borrow the same source platform world");
    world.normal_finish.field_612cf8 = 0;                                   // 442120.
    world.impact_particles.rate_selector = 100;                             // 44212A, not display refresh rate.
    prepare_source_numeric_win64_429510();                                  // 442134; actual current-thread control.
    initialize_platform_source_random_time_42f1a0(*platform, context.time); // 442139.
    initialize_platform_source_clock_434840(*platform, context.counter);    // 44213E.
    platform->source_allocations().initialize_42eab0();                     // 442143, SAME existing bank.
    // 442148 CALL4624B0: the entire pinned child is C3 RET, no effects.
    initialize_platform_source_boot_config_42d660(*platform, context.files);         // 44214D.
    (void)initialize_platform_source_audio_462090(*platform, context.input_handles); // 442152, EAX ignored.
    initialize_platform_source_physical_input_45ec30(*platform);                     // 442157.
    // Existing exact order445F80 ->4249B0 ->60DF9C=0. No readiness branch:
    // both normal input return0 and1 continue to the actual scene tail.
    (void)initialize_platform_source_language_input_44215c(*platform, context.files, context.input_handles);
    auto scene_world = scene_transition_world(world);
    initialize_scene(scene_world, context.frontend.source.scene, context.scene); // 442170 tail442030.
}
} // namespace sgn::retail
