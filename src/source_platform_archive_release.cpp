#include "sgn/source_boot_config.hpp"
#include "sgn/race_session.hpp"
namespace sgn::retail
{
void release_platform_source_archive_42e749(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    release_source_archive_44b1a0(platform.state().boot_config.files);
    // No material image/reload reset, bank release, font init or publication.
}
} // namespace sgn::retail
