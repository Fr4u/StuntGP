#include "sgn/source_cold_time.hpp"
#include <Windows.h>
#include <algorithm>
#include <bit>
#include <cstddef>
#include <new>
#include <utility>

namespace sgn::retail
{
static_assert(sizeof(SYSTEMTIME) == sizeof(SourceSystemTime));
static_assert(sizeof(TIME_ZONE_INFORMATION) == sizeof(SourceTimeZoneInformation));
#define SGN_TIME_OFFSET(source, native)                                                                                \
    static_assert(offsetof(SourceSystemTime, source) == offsetof(SYSTEMTIME, native))
SGN_TIME_OFFSET(year, wYear);
SGN_TIME_OFFSET(month, wMonth);
SGN_TIME_OFFSET(day_of_week, wDayOfWeek);
SGN_TIME_OFFSET(day, wDay);
SGN_TIME_OFFSET(hour, wHour);
SGN_TIME_OFFSET(minute, wMinute);
SGN_TIME_OFFSET(second, wSecond);
SGN_TIME_OFFSET(milliseconds, wMilliseconds);
#undef SGN_TIME_OFFSET
#define SGN_ZONE_OFFSET(source, native)                                                                                \
    static_assert(offsetof(SourceTimeZoneInformation, source) == offsetof(TIME_ZONE_INFORMATION, native))
SGN_ZONE_OFFSET(bias, Bias);
SGN_ZONE_OFFSET(standard_name, StandardName);
SGN_ZONE_OFFSET(standard_date, StandardDate);
SGN_ZONE_OFFSET(standard_bias, StandardBias);
SGN_ZONE_OFFSET(daylight_name, DaylightName);
SGN_ZONE_OFFSET(daylight_date, DaylightDate);
SGN_ZONE_OFFSET(daylight_bias, DaylightBias);
#undef SGN_ZONE_OFFSET
static_assert(sizeof(wchar_t) == sizeof(char16_t));
Win32SourceColdTimePlatform::Win32SourceColdTimePlatform(std::optional<std::string> tz_snapshot)
    : tz_snapshot_(std::move(tz_snapshot))
{
    if (tz_snapshot_ && tz_snapshot_->find('\0') != std::string::npos)
        throw std::invalid_argument("TZ snapshot contains embedded NUL");
}
void Win32SourceColdTimePlatform::get_local_time(SourceSystemTime &output)
{
    SYSTEMTIME native{};
    ::GetLocalTime(&native);
    output = std::bit_cast<SourceSystemTime>(native);
}
void Win32SourceColdTimePlatform::get_system_time(SourceSystemTime &output)
{
    SYSTEMTIME native{};
    ::GetSystemTime(&native);
    output = std::bit_cast<SourceSystemTime>(native);
}
std::uint32_t Win32SourceColdTimePlatform::get_time_zone_information(SourceTimeZoneInformation &output)
{
    auto native = std::bit_cast<TIME_ZONE_INFORMATION>(output);
    const DWORD result = ::GetTimeZoneInformation(&native);
    // Preserve every actual OS output write even on failure, including retained
    // fields untouched by the OS. No aliasing a foreign structure through a cast.
    output = std::bit_cast<SourceTimeZoneInformation>(native);
    return static_cast<std::uint32_t>(result);
}
const char *Win32SourceColdTimePlatform::read_tz_46c764()
{
    return tz_snapshot_ ? tz_snapshot_->c_str() : nullptr;
}
std::unique_ptr<char[]> Win32SourceColdTimePlatform::allocate_tz_copy(std::size_t bytes)
{
    return std::unique_ptr<char[]>(new (std::nothrow) char[bytes]);
}
SourceNameConversion Win32SourceColdTimePlatform::convert_zone_name(std::uint32_t code_page,
                                                                    const std::array<char16_t, 32> &input,
                                                                    std::array<char, 64> &output)
{
    if (std::find(input.begin(), input.end(), u'\0') == input.end())
        throw std::invalid_argument("Unterminated time-zone name");
    std::array<wchar_t, 32> native{};
    std::transform(input.begin(), input.end(), native.begin(), [](char16_t c) { return static_cast<wchar_t>(c); });
    BOOL used_default = FALSE;
    const int result =
        ::WideCharToMultiByte(code_page, 0x220u, native.data(), -1, output.data(), 63, nullptr, &used_default);
    return {result, used_default != FALSE};
}
} // namespace sgn::retail
