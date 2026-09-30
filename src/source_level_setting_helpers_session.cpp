#include "sgn/source_script_path_formatter.hpp"
#include "sgn/source_level_kind_compare.hpp"
#include "sgn/race_session.hpp"
namespace sgn::retail
{
void format_level_track_texture_42cc62(RaceSessionPlatform &platform, std::span<const std::uint8_t> local,
                                       SourceScriptPathCall &call)
{
    platform.require_owner_thread();
    auto &state = platform.state();
    auto output = std::span<std::uint8_t, 64>(reinterpret_cast<std::uint8_t *>(state.level.track_texture.data()),
                                              state.level.track_texture.size());
    format_level_track_texture_42cc62(state.boot_config, platform.source_application_literals(), local, output, call);
}
void format_level_track_texture_42cc62(RaceSessionPlatform &platform, std::span<const std::uint8_t> local)
{
    SourceScriptPathCall call;
    format_level_track_texture_42cc62(platform, local, call);
}
std::int32_t compare_source_level_kind_46dd90(RaceSessionPlatform &platform, std::span<const std::uint8_t> local,
                                              std::uint32_t literal)
{
    platform.require_owner_thread();
    return compare_source_level_kind_46dd90(platform.source_application_literals(), local, literal);
}
} // namespace sgn::retail
