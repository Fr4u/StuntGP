#include "sgn/source_music_installed.hpp"
#include "sgn/source_music_period_host.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace sgn::retail
{
class SourceMusicDiskSpaceInstalled final : public SourceMusicDiskFreeSpaceReceiving
{
  public:
    std::uint32_t get_disk_free_space_a(SourceMusicDriveRootAccess root, SourceMusicDiskDwordAccess sectors,
                                        SourceMusicDiskDwordAccess bytes, SourceMusicDiskDwordAccess free,
                                        SourceMusicDiskDwordAccess total) override
    {
        const auto owner = root.checked_owner();
        (void)owner;
        if (!sectors.aliases(free) || !sectors.aliases(total) || sectors.aliases(bytes))
            throw std::logic_error("45FE45 disk output alias topology differs");
        // The native call sees precisely two DWORD cells, with the first
        // passed three times. Host scratch seeds are never source observations.
        DWORD argument = 0, sector_bytes = 0;
        const auto result =
            ::GetDiskFreeSpaceA(root.checked_native_root(), &argument, &sector_bytes, &argument, &argument);
        if (result == 0)
            throw SourceMusicWaveUnknown("45FE45 failed native output writes remain unqualified");
        // Synchronous SDK output call has returned; publish final values into
        // the receiving cells. No callback observes intermediate native stores.
        sectors.write(argument);
        bytes.write(sector_bytes);
        return static_cast<std::uint32_t>(result);
    }
};
void install_source_music_windows(std::shared_ptr<RaceSessionPlatform> p)
{
    if (!p)
        throw std::invalid_argument("source music installation requires platform");
    p->require_owner_thread();
    if (!p->source_construction_only_entry())
        throw std::logic_error("source music installation on preview platform");
    if (p->source_music_calls())
        throw std::logic_error("source music installation already complete");
    (void)p->source_application_literals(); // require existing weak App binding, no new data owner
    // NEW HOST (audio-r36): relocate the live source base (GAMELOCATION, e.g. "R:\SGP") to the local retail
    // folder exactly like the application core's file platform. Without it 44B3B0 of "<base>\Sound\Music\*.wav"
    // looked for a nonexistent drive, returned size 0 and 45F8F0 silently skipped every music track.
    auto files = std::make_shared<NativeSourceBootFilePlatform>(p->retail_root(), p->state().boot_config);
    auto caps = make_installed_music_caps_receiving();
    auto drive = make_installed_music_drive_receiving();
    auto disk = std::make_shared<SourceMusicDiskSpaceInstalled>();
    (void)prepare_installed_music_period_owner(p); // actual existing accounting owner, no Begin call
    SourceMusicCallBinding::install(std::move(p), std::move(files), std::move(caps), std::move(drive), std::move(disk));
}
} // namespace sgn::retail
