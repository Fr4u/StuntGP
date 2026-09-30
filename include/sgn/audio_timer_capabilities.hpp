#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>

#if defined(_WIN32)
// NEW host representation uses the SDK object itself. Public SDK inclusion is
// intentional: all clients of AudioStreamState must be rebuilt consistently.
#pragma push_macro("NOMINMAX")
#pragma push_macro("WIN32_LEAN_AND_MEAN")
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#if defined(_MSC_VER)
#pragma warning(push)
#endif
#include <Windows.h>
// Match the SDK's normal multimedia include path: mmsystem.h includes
// timeapi.h under pack(1). Including timeapi.h directly first gives a
// different TIMECAPS alignment in this SDK and must fail the checks below.
#include <mmsystem.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#pragma pop_macro("WIN32_LEAN_AND_MEAN")
#pragma pop_macro("NOMINMAX")
#endif

namespace sgn::retail
{
class SourceMusicTimerCapsInstalled;

#if defined(_WIN32)
static_assert(std::is_same_v<decltype(TIMECAPS::wPeriodMin), std::uint32_t>);
static_assert(std::is_same_v<decltype(TIMECAPS::wPeriodMax), std::uint32_t>);
static_assert(std::is_standard_layout_v<TIMECAPS>);
static_assert(std::is_trivially_copyable_v<TIMECAPS>);
static_assert(sizeof(TIMECAPS) == 8 && alignof(TIMECAPS) == 1);
static_assert(offsetof(TIMECAPS, wPeriodMin) == 0);
static_assert(offsetof(TIMECAPS, wPeriodMax) == 4);
#endif

// Sole value storage for source ESI+3C/+40, not a mirror of separate words.
// SDK address access is private to the installed synchronous provider.
// No pointer members: copying the owner state copies values, never aliases.
class alignas(std::uint32_t) AudioTimerCapabilities final
{
#if defined(_WIN32)
    TIMECAPS native_{};
#else
    // Portable receiving representation only; never passed to a native API.
    struct Words
    {
        std::uint32_t wPeriodMin{}, wPeriodMax{};
    } native_;
#endif
    friend class SourceMusicTimerCapsInstalled;

  public:
    AudioTimerCapabilities() = default;
    std::uint32_t &minimum() noexcept
    {
        return native_.wPeriodMin;
    }
    const std::uint32_t &minimum() const noexcept
    {
        return native_.wPeriodMin;
    }
    std::uint32_t &maximum() noexcept
    {
        return native_.wPeriodMax;
    }
    const std::uint32_t &maximum() const noexcept
    {
        return native_.wPeriodMax;
    }
};
static_assert(std::is_standard_layout_v<AudioTimerCapabilities>);
static_assert(std::is_trivially_copyable_v<AudioTimerCapabilities>);
static_assert(sizeof(AudioTimerCapabilities) == 8);
static_assert(alignof(AudioTimerCapabilities) == alignof(std::uint32_t));
} // namespace sgn::retail
