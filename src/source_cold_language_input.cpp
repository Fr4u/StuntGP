#include "sgn/source_cold_language_input.hpp"
#include "sgn/source_cold_language.hpp"
#include "sgn/race_session.hpp"
#include "source_cold_language_input_order.hpp"

namespace sgn::retail
{
SourceColdLanguageInputObservation initialize_platform_source_language_input_44215c(RaceSessionPlatform &platform,
                                                                                    SourceBootFilePlatform &files,
                                                                                    InputWindowHandles handles)
{
    platform.require_owner_thread();
    if (!platform.source_construction_only_entry())
        throw FormatError("source cold language/input requires the existing source-construction platform");
    auto &world = platform.state();
    // Host admission only: must find the SAME existing real owner before any
    // language writes. window_input never attaches, replaces or initializes it.
    auto &input = platform.window_input(handles.instance, handles.window);
    return cold_language_input_detail::entered_44215c(
        world.source_scheduler.fixed_mode_b, [&] { refresh_session_source_language_445f80(platform, files); },
        [&] { return input.initialize_4249b0(world.source_input.acquisition, handles); });
    // initialize_4249b0 revalidates the HWND AFTER language I/O. A destroyed
    // window or phase error retains completed language writes, with no442166.
}
} // namespace sgn::retail
