#include "sgn/source_music_timer_caps_installed.hpp"
#include "sgn/source_music_installed.hpp"
#include "sgn/source_music_drive_type_prefix.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session.hpp"
#include <memory>
#include <utility>

#if !defined(_WIN32)
#error The installed music timer bridge requires the Windows SDK and WinMM.
#endif

namespace sgn::retail
{
// Definition kept inside this translation unit. The public receiving seam
// cannot obtain the SDK address or manufacture an installed provider.
class SourceMusicTimerCapsInstalled final : public SourceMusicTimerCapsReceiving
{
  public:
    std::uint32_t time_get_dev_caps(SourceMusicTimerCapsAccess output, std::uint32_t bytes) override
    {
        // Guard scope/thread before selecting the real SDK subobject. Both
        // this local owner and the prefix owner retain it across the call.
        const auto owner = output.checked_owner();
        if (bytes != sizeof(TIMECAPS))
            throw std::invalid_argument("music timer capability size must be eight bytes");
        auto &actual = owner->state().audio_stream.timer_capabilities.native_;
        // PE4a1d234c...,45FDAE PUSH8;45FDB0/45FDB7 ESI+3C;
        // 45FDBB IAT46E1AC = WINMM.DLL!timeGetDevCaps.
        // A real TIMECAPS* points into retained state. No cast, temporary,
        // copy-back, success filter, allocation or rollback surrounds WinMM.
        return ::timeGetDevCaps(std::addressof(actual), static_cast<UINT>(sizeof(TIMECAPS)));
    }
};

SourceMusicTimerPrefixObservation receive_installed_source_music_timer_prefix_45fda0(
    std::shared_ptr<RaceSessionPlatform> owner)
{
    SourceMusicTimerCapsInstalled installed;
    return receive_source_music_timer_prefix_45fda0(std::move(owner), installed);
}

// Private installed provider; receiving users cannot obtain the native root.
class SourceMusicDriveTypeInstalled final : public SourceMusicDriveTypeReceiving
{
  public:
    std::uint32_t get_drive_type_a(SourceMusicDriveRootAccess root) override
    {
        const auto owner = root.checked_owner();
        (void)owner; // retain the actual platform throughout the synchronous API
        // PE45FE17 -> KERNEL32!GetDriveTypeA, one LPCSTR to the live five-byte
        // local. Explicit A suffix; no Unicode/path repair/default/error mapping.
        return ::GetDriveTypeA(root.checked_native_root());
    }
};
SourceMusicDriveTypePrefixObservation receive_installed_source_music_drive_type_prefix_45fda0(
    std::shared_ptr<RaceSessionPlatform> owner)
{
    SourceMusicTimerCapsInstalled caps;
    SourceMusicDriveTypeInstalled drive;
    return receive_source_music_drive_type_prefix_45fda0(std::move(owner), caps, drive);
}
} // namespace sgn::retail

namespace sgn::retail
{
std::shared_ptr<SourceMusicTimerCapsReceiving> make_installed_music_caps_receiving()
{
    return std::make_shared<SourceMusicTimerCapsInstalled>();
}
std::shared_ptr<SourceMusicDriveTypeReceiving> make_installed_music_drive_receiving()
{
    return std::make_shared<SourceMusicDriveTypeInstalled>();
}
} // namespace sgn::retail
