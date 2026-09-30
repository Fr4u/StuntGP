#include "sgn/source_outer_scheduler.hpp"
#include "sgn/race_session.hpp"
#include <windows.h>

namespace sgn::retail
{
std::uint64_t Win32SourceOuterCounter::counter()
{
    LARGE_INTEGER value{};
    if (!QueryPerformanceCounter(&value))
        throw FormatError("source counter OS acquisition failed");
    return static_cast<std::uint64_t>(value.QuadPart);
}
std::uint64_t Win32SourceOuterCounter::frequency()
{
    LARGE_INTEGER value{};
    if (!QueryPerformanceFrequency(&value))
        throw FormatError("source counter frequency unavailable");
    return static_cast<std::uint64_t>(value.QuadPart);
}
void initialize_platform_source_clock_434840(RaceSessionPlatform &platform, FrontendPerformanceCounter &source)
{
    platform.require_owner_thread();
    auto &s = platform.state();
    initialize_frontend_clock_434840(s.tick_clock, s.source_scheduler.frequency, source);
}
void reset_platform_source_clock_442100(RaceSessionPlatform &platform, FrontendPerformanceCounter &source)
{
    platform.require_owner_thread();
    reset_frontend_clock_442100(platform.state().tick_clock, source);
}
void initialize_session_source_clock_434840(RaceSession &session, FrontendPerformanceCounter &source)
{
    initialize_platform_source_clock_434840(*session.shared_platform(), source);
}
} // namespace sgn::retail
