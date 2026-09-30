#include "sgn/source_script_path_formatter.hpp"
#include "sgn/race_session.hpp"
namespace sgn::retail
{
void format_rolling_demo_name_42d01a(RaceSessionPlatform &platform, std::span<const std::uint8_t> written_local,
                                     std::uint32_t destination_offset, SourceScriptPathCall &call)
{
    platform.require_owner_thread();
    format_rolling_demo_name_42d01a(platform.state().boot_config, platform.source_application_literals(), written_local,
                                    destination_offset, call);
}
void format_rolling_demo_name_42d01a(RaceSessionPlatform &platform, std::span<const std::uint8_t> written_local,
                                     std::uint32_t destination_offset)
{
    SourceScriptPathCall call;
    format_rolling_demo_name_42d01a(platform, written_local, destination_offset, call);
}
} // namespace sgn::retail
