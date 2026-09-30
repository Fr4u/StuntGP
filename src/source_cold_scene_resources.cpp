#include "sgn/source_cold_scene_resources.hpp"
#include "sgn/race_session.hpp"
#include "sgn/session_world_materials.hpp"
#include "sgn/source_level_script_load.hpp"
#include "sgn/source_sprite_scene.hpp"

namespace sgn::retail
{
namespace
{
void require_cold_menu(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    if (!platform.source_construction_only_entry())
        throw FormatError("cold scene prefix requires the source platform entry");
    const auto &world = platform.state();
    if (world.scene_mode != 2 || world.options.selection_phase != 2)
        throw FormatError("cold scene prefix requires actual44EABA/44EABF scene2/target2 stores");
    if (!platform.source_allocations().initialized())
        throw FormatError("cold scene prefix requires the earlier442143 allocation-bank initialization");
}
class ColdMenuLevelScriptChildren final : public SourceLevelScriptChildren
{
    RaceSessionPlatform &platform_;

  public:
    explicit ColdMenuLevelScriptChildren(RaceSessionPlatform &platform) : platform_(platform) {}
    void apply_overrides_4453a0(RaceSessionPlatform &platform) override
    {
        if (&platform != &platform_)
            throw FormatError("cold script overrides require their actual world");
        platform.require_owner_thread();
        // 4453A0 reads612BEC;4453AB compares EXACT1;4453AD JNZ445522.
        // This is the actual source return branch, not an empty override hook.
        // No source write or child call precedes that branch.
        if (platform.state().options.selection_phase == 1)
            throw SourceColdSceneResourceBoundary(0x4453b3, 0x4453a0,
                                                  "cold script reached the unsupported race override branch4453B3");
    }
};
} // namespace
void load_source_cold_level_script_42d070(RaceSessionPlatform &platform, SourceBootFilePlatform &files)
{
    require_cold_menu(platform);
    ColdMenuLevelScriptChildren children(platform);
    load_source_level_script_42d070(platform, files, children);
}
SourceColdSceneResourcePrefixResult initialize_source_cold_scene_prefix_42e6e0(SourceColdSceneResourceContext &context)
{
    if (!context.platform)
        throw FormatError("cold scene prefix requires its retained platform");
    auto &platform = *context.platform;
    require_cold_menu(platform);
    // Lifetime/same-world admission BEFORE reset reaches any source store.
    (void)context.materials.registry_flags_for(platform.source_scene_resources());
    context.materials.reset_source_cold_scene_resources_42e510();  // 42E6E1
    load_source_cold_level_script_42d070(platform, context.files); // 42E6E6
    reset_view_matrix_cursor(platform.state().view_matrices);      // 42E6EB ->44FD60
    initialize_source_sprite_scene_4342c0(context.platform);       // 42E6F0, Models actual six allocations
    auto &world = platform.state();
    platform.source_scene_resources().load_source_requests_42f090(platform.source_allocations(), world.scene_mode,
                                                                  world.selection,
                                                                  world.cars); // 42E6F5, actual scene2 sentinel branch
    return {}; // 42E6FA renderer predicate has not run; no success/resume token for42E6E0
}
} // namespace sgn::retail
