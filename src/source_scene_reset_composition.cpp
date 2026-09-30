#include "sgn/source_scene_reset_composition.hpp"
#include "sgn/source_cold_language_input.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_music.hpp"
#include "sgn/race_session_scene.hpp"
#include "sgn/retail_frontend_clock.hpp"
#include "sgn/source_rail_reset.hpp"

namespace sgn::retail
{
SourceSceneResetResult reset_source_scene_445fa0(RaceSessionPlatform &platform, FrontendPerformanceCounter &counter)
{
    const auto prefix = reset_source_scene_445fa0_prefix(platform);
    if (prefix.status != SourceSceneResetStatus::callee_required || prefix.blocked_address != 0x4460cb ||
        prefix.missing_source_word != 0)
        return prefix;
    auto &state = platform.state();                         // the prefix already checked the creating thread
    reset_frontend_clock_442100(state.tick_clock, counter); // 4460CB
    reset_rail_4192d0(state.rail);                          // 4460D3 tail; reached only after both counter returns
    return {SourceSceneResetStatus::completed, 0, 0};
}
bool perform_source_scene_reset_445fa0(SceneOperation operation, RaceSessionPlatform &platform,
                                       FrontendPerformanceCounter &counter)
{
    if (operation != SceneOperation::reset_scene_state)
        return false;
    const auto result = reset_source_scene_445fa0(platform, counter);
    if (result.status != SourceSceneResetStatus::completed)
        throw SourceSceneResetBoundary{result};
    return true; // 445FA0 returned;442030 may now request its next mandatory child
}

SourceScenePrefixResult perform_source_scene_initialization_prefix(SceneOperation operation,
                                                                   RaceSessionPlatform &platform,
                                                                   FrontendPerformanceCounter &counter)
{
    if (perform_source_scene_reset_445fa0(operation, platform, counter))
        return {SourceScenePrefixStatus::completed, 0, 0};
    if (operation == SceneOperation::begin_allocations)
    {
        platform.begin_scene_allocations_42ea80();
        return {SourceScenePrefixStatus::completed, 0, 0};
    }
    if (operation == SceneOperation::stop_music)
    {
        const auto result = stop_source_music_45f9d0(platform);
        if (result.status != SourceMusicStopStatus::completed)
            return {SourceScenePrefixStatus::child_required, result.blocked_address, result.required_identity};
        return {SourceScenePrefixStatus::completed, 0, 0};
    }
    if (operation == SceneOperation::stop_sound_instances)
    {
        const auto result = stop_source_samples_460dd0(platform);
        if (result.status != SourceSampleStopStatus::completed)
            return {SourceScenePrefixStatus::child_required, result.blocked_address, result.required_buffer.value};
        return {SourceScenePrefixStatus::completed, 0, 0};
    }
    if (operation == SceneOperation::reset_sound_output)
    {
        reset_source_sound_output_460c90(platform);
        return {SourceScenePrefixStatus::completed, 0, 0};
    }
    return {SourceScenePrefixStatus::unhandled_operation, static_cast<std::uint32_t>(operation), 0};
}
namespace
{
struct ColdSceneRequired
{
    SourceColdSceneObservation observation;
};
class ColdSceneServices final : public SceneTransitionServices
{
    RaceSessionPlatform &platform_;
    FrontendPerformanceCounter &counter_;

  public:
    ColdSceneServices(RaceSessionPlatform &platform, FrontendPerformanceCounter &counter)
        : platform_(platform), counter_(counter)
    {
    }
    void perform(SceneOperation operation) override
    {
        const auto result = perform_source_scene_initialization_prefix(operation, platform_, counter_);
        if (result.status == SourceScenePrefixStatus::completed)
            return;
        throw ColdSceneRequired{{result.blocked_address, result.required_identity}};
    }
    void select_audio_environment(std::uint32_t) override
    {
        throw FormatError("cold442030 cannot enter unload460AF0");
    }
    void initialize_menu_viewport(std::uint32_t) override
    {
        throw FormatError("cold442030 menu reached beyond the unadmitted42E6E0");
    }
    void reset_tick_clock() override
    {
        throw FormatError("cold442030 final clock reached beyond the unadmitted42E6E0");
    }
};
} // namespace
SourceColdSceneObservation initialize_source_cold_scene_442030(FrontendState &frontend,
                                                               FrontendPerformanceCounter &counter)
{
    const auto &platform = frontend.shared_platform();
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry())
        throw FormatError("cold442030 requires the existing source-construction platform");
    if (&frontend.world() != &platform->state())
        throw FormatError("cold442030 frontend has another platform world");
    auto world = scene_transition_world(frontend.world());
    ColdSceneServices services(*platform, counter);
    try
    {
        initialize_scene(world, frontend.source.scene, services);
    }
    catch (const ColdSceneRequired &required)
    {
        return required.observation;
    }
    throw FormatError("cold442030 unexpectedly returned beyond its admitted prefix");
}

SourceColdLanguageSceneObservation initialize_source_cold_language_scene_44215c(FrontendState &frontend,
                                                                                SourceBootFilePlatform &files,
                                                                                InputWindowHandles handles,
                                                                                FrontendPerformanceCounter &counter)
{
    const auto &platform = frontend.shared_platform();
    platform->require_owner_thread();
    if (&frontend.world() != &platform->state())
        throw FormatError("cold44215C frontend has another platform world");
    const auto input = initialize_platform_source_language_input_44215c(*platform, files, handles);
    // 442166 completed on every normal input return;442170 has no predicate.
    const auto scene = initialize_source_cold_scene_442030(frontend, counter);
    return {input.ignored_input_return, scene};
}
} // namespace sgn::retail
