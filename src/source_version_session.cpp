#include "sgn/source_script_path_formatter.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
void initialize_platform_source_version_442010(RaceSessionPlatform &platform, SourceScriptPathCall &call)
{
    platform.require_owner_thread();
    initialize_source_version_442010(platform.state().boot_config, platform.source_application_literals(), call);
}
void initialize_platform_source_version_442010(RaceSessionPlatform &platform)
{
    SourceScriptPathCall call;
    initialize_platform_source_version_442010(platform, call);
}
} // namespace sgn::retail
