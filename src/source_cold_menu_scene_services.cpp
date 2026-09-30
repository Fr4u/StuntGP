#include "sgn/source_cold_menu_scene_services.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_viewport.hpp"
#include "sgn/host_load_timeline.hpp"
#include <bit>
namespace sgn::retail
{
namespace
{
// NEW HOST (R30) load timeline: every scene-transition operation (menu load, race load split by source
// stage) is one timeline stage named after its source function. Diagnostics only.
const char *timeline_stage(SceneOperation operation) noexcept
{
    switch (operation)
    {
    case SceneOperation::release_image_resources:
        return "scene.release_image_resources_431810";
    case SceneOperation::release_allocations:
        return "scene.release_allocations_42eaa0";
    case SceneOperation::stop_music:
        return "scene.stop_music_45f9d0";
    case SceneOperation::stop_sound_instances:
        return "scene.stop_sound_instances_460dd0";
    case SceneOperation::reset_sound_output:
        return "scene.reset_sound_output_460c90";
    case SceneOperation::reset_scene_state:
        return "scene.reset_scene_state_445fa0";
    case SceneOperation::begin_allocations:
        return "scene.begin_allocations_42ea80";
    case SceneOperation::load_scene_resources:
        return "scene.load_scene_resources_42e6e0";
    case SceneOperation::ensure_score_configuration:
        return "scene.ensure_score_configuration_443d70";
    case SceneOperation::ensure_gameplay_configuration:
        return "scene.ensure_gameplay_configuration_4457e0";
    case SceneOperation::prepare_race_state:
        return "scene.prepare_race_state_445f80";
    case SceneOperation::configure_level:
        return "scene.configure_level_4469f0";
    case SceneOperation::initialize_vehicle_numeric:
        return "scene.initialize_vehicle_numeric_40e050";
    case SceneOperation::initialize_vehicle_state:
        return "scene.initialize_vehicle_state_40a2f0";
    case SceneOperation::initialize_vehicle_presentation:
        return "scene.initialize_vehicle_presentation_401de0";
    case SceneOperation::initialize_vehicle_models:
        return "scene.initialize_vehicle_models_413670";
    case SceneOperation::initialize_color_tables:
        return "scene.initialize_color_tables_422420";
    case SceneOperation::reset_recordings:
        return "scene.reset_recordings_4199a0";
    case SceneOperation::initialize_race:
        return "scene.initialize_race_40d500";
    case SceneOperation::initialize_menu:
        return "scene.initialize_menu_43d0b0";
    case SceneOperation::return_to_menu:
        return "scene.return_to_menu_43d1d0";
    case SceneOperation::leave_menu_for_race:
        return "scene.leave_menu_for_race_43d1b0";
    }
    return "scene.other";
}
} // namespace
SourceColdMenuSceneServices::SourceColdMenuSceneServices(SourceColdSceneStartupContext cold,
                                                         LightGammaDevice *const &gamma,
                                                         PersistenceServices &persistence,
                                                         SourceFrontendDataStartup &data,
                                                         SceneTransitionServices &remaining)
    : cold_(cold), gamma_(gamma), persistence_(persistence), data_(data), remaining_(remaining)
{
}
void SourceColdMenuSceneServices::attach_menu(SourceMenuUpdateChildren &update,
                                              SourceNavigationPageServices &navigation)
{
    require_same_world();
    if (update_ || navigation_)
        throw FormatError("source scene menu was already attached");
    update_ = &update;
    navigation_ = &navigation;
}
void SourceColdMenuSceneServices::require_same_world() const
{
    if (!cold_.scene.platform)
        throw FormatError("source scene adapter has no platform");
    cold_.scene.platform->require_owner_thread();
    if (cold_.renderer.frontend.shared_platform() != cold_.scene.platform ||
        &cold_.renderer.frontend.world() != &cold_.scene.platform->state() ||
        &cold_.renderer.materials != &cold_.scene.materials)
        throw FormatError("source scene adapter borrows different application owners");
}
void SourceColdMenuSceneServices::perform(SceneOperation operation)
{
    require_same_world();
    SGN_LOAD_STAGE(timeline_stage(operation));
    switch (operation)
    {
    case SceneOperation::load_scene_resources:
        initialize_source_cold_scene_42e6e0(cold_, gamma_);
        return;
    case SceneOperation::initialize_menu:
        if (!update_ || !navigation_)
            throw FormatError("actual menu must be attached after device production before43D0B0");
        initialize_source_frontend_43d0b0(cold_.renderer.frontend, cold_.scene.files, *update_, *navigation_,
                                          persistence_, data_);
        host_load_timeline::milestone("menu_initialized"); // NEW HOST (R30) diagnostics (first time only)
        return;
    case SceneOperation::return_to_menu:
        return_to_frontend(cold_.renderer.frontend.source);
        return;
    default:
        remaining_.perform(operation);
        return;
    }
}
void SourceColdMenuSceneServices::select_audio_environment(std::uint32_t environment)
{
    require_same_world();
    remaining_.select_audio_environment(environment);
}
void SourceColdMenuSceneServices::initialize_menu_viewport(std::uint32_t single_view)
{
    require_same_world();
    select_viewport(cold_.renderer.frontend.world().viewport, std::bit_cast<std::int32_t>(single_view));
}
void SourceColdMenuSceneServices::reset_tick_clock()
{
    require_same_world();
    remaining_.reset_tick_clock(); // two actual434820 reads owned by Main
}
} // namespace sgn::retail
