#include "sgn/source_selected_script.hpp"
#include "sgn/source_cold_language.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
void select_source_level_script_4452e0(RaceSessionPlatform &platform, SourceScriptPathCall &call)
{
    platform.require_owner_thread(); // NEW host lifetime/thread invariant.
    auto &state = platform.state();
    if (state.options.selection_phase != 1u)
        return; // 4452E0..4452E7 ->44530E.
    // 4452E9 loads60D744;4452EF calls445F70, whose445F70 writes4835E0.
    // NEW repair (PORT-FIX-013): 60D744 is 43A751's copy of the setup language
    // 5DF5F0, which stays 0 when the Language page was left by 43F610's idle
    // return (fresh install: 452CEF 445AD0 returns 0). 445F80 would then reach
    // 445D70 with EBX=0, which skips every value column (445DD7 JZ 445DF3) and
    // interns the never-written stack buffer EBP-148. Only this unset 0 keeps
    // 4835E0 unchanged; every other value is stored exactly as the original.
    if (state.frontend_launch.setting_60d744 != 0u)
        set_source_language_445f70(state.menu.requested_language, state.frontend_launch.setting_60d744);
    const auto index = state.frontend_launch.track_index;    // 4452F4, AFTER language store.
    const auto app = platform.source_application_literals(); // Same weak App identity.
    call.next_source_site = 0x4452f9;
    const auto format = app.read_selected_script_pointer_4452f9(0x482b88u + index * 4u);
    // 445300..445306: format pointer, destination589604, no variadic words.
    (void)format_selected_script_445306(state.boot_config, app, format, call);
    call.next_source_site = 0x44530e; // 44530B discards two arguments;44530E RET.
}
void select_source_level_script_4452e0(RaceSessionPlatform &platform)
{
    SourceScriptPathCall call;
    select_source_level_script_4452e0(platform, call);
}
} // namespace sgn::retail
