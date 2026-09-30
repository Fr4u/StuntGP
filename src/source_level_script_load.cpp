#include "sgn/source_level_script_load.hpp"
#include "sgn/source_level_settings.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/source_selected_script.hpp"
#include "sgn/source_script_search.hpp"
#include "sgn/source_car_key_formatter.hpp"
#include "sgn/source_rolling_demo.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/scene_resources.hpp"

namespace sgn::retail
{
std::uint32_t format_platform_level_script_path_42d08c(RaceSessionPlatform &platform, SourceScriptPathCall &call)
{
    // NEW host binding only: the source formatter reads the existing config
    // and the same application's literal owner, with its real caller frame.
    platform.require_owner_thread();
    return format_level_script_path_42d08c(platform.state().boot_config, platform.source_application_literals(), call);
}
void load_source_level_script_42d070(RaceSessionPlatform &platform, SourceBootFilePlatform &files,
                                     SourceLevelScriptChildren &children)
{
    platform.require_owner_thread();                                // NEW host ownership admission only.
    SourceScriptPathCall path;                                      // 42D073: sole local128, retained through read.
    (void)format_platform_level_script_path_42d08c(platform, path); // 42D08C
    select_source_level_script_4452e0(platform);                    // 42D094
    auto &config = platform.state().boot_config;
    // Existing full router owns the same raw WAD cache. This C-string
    // transport admits only a written, terminated path (NEW host boundary).
    const auto length = source_boot_file_size_44b3b0(config, files, path.completed_path());
    auto &bank = platform.source_allocations();
    const std::uint32_t reserved = length + std::uint32_t{0x20}; // 42D0A8, wraps.
    const auto script = bank.allocate_42e8d0(reserved, 0);       // 42D0AB
    // No bytes(script) here: null is transported through the router until
    // a real memory access. Zero length does not bypass this source call.
    std::size_t copied = 0; // NEW observation, ignored by the source caller.
    (void)source_boot_file_read_bank_44b260(config, files, bank, path.completed_path(), length, script, copied);
    // 42D0C7 ->42D060 is exactly RET. Its pointer arguments are not read.
    const SourceLevelScriptPointer original{script, 0, !static_cast<bool>(script)};
    config.boundary = {};                                             // 42D0D0
    load_source_rolling_demo_42cfa0(platform, original);              // 42D0D6
    config.boundary = {};                                             // 42D0E5
    auto cursor = original;                                           // 42D0EB: separate local.
    (void)select_source_level_key_42d0ee(platform, original, cursor); // 42D0EE
    const auto end = find_source_level_end_42d0fc(platform, cursor);  // 42D0FC
    config.boundary = end;                                            // 42D107
    // Three existing stores to the concrete request owner, in source order:
    // append58C234=names58C238; records[0].name=null; count58C808=0.
    // No name-array wipe, new request bank, or model/resource reset.
    (void)platform.source_scene_resources().models().initialize_source_requests_42d070_prefix();
    SourceCarKeyFormatterImpl car_crt; // Stateless adapter; same live App/CRT owners.
    platform.source_scene_resources().models().produce_source_level_assets_42c740(platform, cursor); // 42D122
    configure_source_cars_42c9b0(platform, cursor, car_crt);                                         // 42D12A
    configure_source_replays_42cb00(platform, cursor, car_crt);                                      // 42D132
    configure_source_ai_42cb70(platform, cursor, car_crt);                                           // 42D13A
    configure_source_tyres_42ca20(platform, cursor, car_crt);                                        // 42D142
    configure_source_liveries_42ca90(platform, cursor, car_crt);                                     // 42D14A
    configure_source_level_42cc20(platform, cursor);                                                 // 42D152
    children.apply_overrides_4453a0(platform);                                                       // 42D157
    (void)bank.release_42e980(script);                                                               // 42D15E
    // 589624 retains its weak script alias after release, exactly as source.
    //  Children receive cursor by value: their local changes cannot replace
    //  this caller's [EBP-4]. No RAII free is added on a child exception.
}
} // namespace sgn::retail
