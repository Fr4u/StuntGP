#include "sgn/source_cold_language.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
void refresh_session_source_language_445f80(RaceSessionPlatform &platform, SourceBootFilePlatform &files)
{
    platform.require_owner_thread();
    auto &world = platform.state();
    const auto requested = world.menu.requested_language; // 445F80: capture once
    if (requested == world.game_text.language)
        return; // 445F94: bypass even invalid input
    load_game_text(world.boot_config, files, requested, world.game_text);
}
} // namespace sgn::retail
