#include "sgn/source_cold_time.hpp"
#include "sgn/source_random_seed.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
using sgn::bit_reinterpret::compile_time::as_signed;
using sgn::bit_reinterpret::compile_time::as_unsigned;
// Source table indices1..13, exact496564..496594 and496598..4965C8.
constexpr std::array<Word, 13> leap_months{0xffffffffu, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};
constexpr std::array<Word, 13> ordinary_months{0xffffffffu, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364};
Word month_before(const std::array<Word, 13> &table, Word month, Word instruction)
{
    if (month < 1u || month > 12u)
        throw SourceTimeBoundaryError(instruction, "Source month outside admitted Win32 month domain");
    return table[month - 1u];
}
bool digit(unsigned char c) noexcept
{
    return c >= '0' && c <= '9';
}
bool space(unsigned char c) noexcept
{
    return c == ' ' || (c >= '\t' && c <= '\r');
}
// 4631F4, pinned single-byte C-locale ctype classes; arithmetic wraps in DWORDs.
Word source_atoi(const char *cursor)
{
    while (space(static_cast<unsigned char>(*cursor)))
        ++cursor;
    const bool negative = *cursor == '-';
    if (negative || *cursor == '+')
        ++cursor;
    Word value = 0;
    while (digit(static_cast<unsigned char>(*cursor)))
    {
        value = value * 10u + static_cast<unsigned char>(*cursor) - static_cast<unsigned char>('0');
        ++cursor;
    }
    return negative ? 0u - value : value;
}
void copy_three(std::array<char, 64> &output, const char *input)
{
    bool ended = false;
    for (std::size_t i = 0; i < 3; ++i)
    {
        if (!ended && *input == 0)
            ended = true;
        output[i] = ended ? 0 : *input++;
    }
    output[3] = 0;
}
void convert_name(SourceColdTimeState &state, SourceColdTimePlatform &platform, const std::array<char16_t, 32> &input,
                  std::array<char, 64> &output, Word instruction)
{
    if (std::find(input.begin(), input.end(), u'\0') == input.end())
        throw SourceTimeBoundaryError(instruction, "Unterminated Win32 time-zone name outside admitted domain");
    const auto result = platform.convert_zone_name(state.code_page, input, output);
    if (result.result == 0 || result.used_default)
        output[0] = 0;
    else
        output[63] = 0;
}
void build_transition_469f29(SourceColdTimeState &state, bool start, Word year, const SourceSystemTime &rule)
{
    const auto &table = (year & 3u) == 0 ? leap_months : ordinary_months;
    Word day = month_before(table, rule.month, 0x469f4cu);
    if (rule.year == 0)
    {
        const Word first = day + 1u;
        // All callers admitted here have year_since1900 in70..138, so SAR2=(year-1)/4.
        const Word weekday = as_unsigned(as_signed(year * 365u + first + (year - 1u) / 4u - 0x63dbu) % 7);
        day = first + static_cast<Word>(rule.day) * 7u - weekday + rule.day_of_week;
        if (as_signed(weekday) < static_cast<std::int32_t>(rule.day_of_week))
            day -= 7u;
        if (rule.day == 5 && as_signed(day) > as_signed(table[rule.month]))
            day -= 7u;
    }
    else
        day += rule.day;
    if (start)
    {
        state.start.day = day; // 469FE8
        const Word minutes = static_cast<Word>(rule.hour) * 60u + rule.minute;
        state.start.year = year;                                                              // 469FF4
        state.start.milliseconds = (minutes * 60u + rule.second) * 1000u + rule.milliseconds; // 46A009
    }
    else
    {
        state.end.day = day; // 46A013
        Word ms =
            ((static_cast<Word>(rule.hour) * 60u + rule.minute) * 60u + state.dst_bias_seconds + rule.second) * 1000u +
            rule.milliseconds;
        state.end.milliseconds = ms; // 46A034
        if (as_signed(ms) < 0)
        {
            ms += 86400000u;
            --day;
            state.end.milliseconds = ms;
            state.end.day = day; // 46A041,46A059
        }
        else if (as_signed(ms) >= 86400000)
        {
            ms -= 86400000u;
            ++day;
            state.end.milliseconds = ms;
            state.end.day = day; // 46A054,46A059
        }
        state.end.year = year; // 46A05F, after normalization
    }
}
bool source_is_dst_469d7d(SourceColdTimeState &state, Word year, Word day)
{
    if (state.daylight == 0)
        return false;
    if (year != state.start.year || year != state.end.year)
    {
        if (state.have_os_rules != 0)
        {
            build_transition_469f29(state, true, year, state.retained_zone.daylight_date);
            build_transition_469f29(state, false, year, state.retained_zone.standard_date);
        }
        else
        {
            build_transition_469f29(state, true, year, {0, 4, 0, 1, 2, 0, 0, 0});
            build_transition_469f29(state, false, year, {0, 10, 0, 5, 2, 0, 0, 0});
        }
    }
    const auto begin = as_signed(state.start.day), end = as_signed(state.end.day), today = as_signed(day);
    if (begin < end)
    {
        if (today < begin || today > end)
            return false;
        if (today > begin && today < end)
            return true;
    }
    else
    {
        if (today < end || today > begin)
            return true;
        if (today > end && today < begin)
            return false;
    }
    // 468374 never writes tm_min or tm_sec. Do not substitute current local time.
    // The first reached uninitialized read is469EFB; transition writes already survive.
    throw SourceTimeBoundaryError(0x469efbu, "Original DST boundary reads unwritten tm_min, then tm_sec");
}
} // namespace
void initialize_source_timezone_469b1f(SourceColdTimeState &state, SourceColdTimePlatform &platform)
{
    state.have_os_rules = 0;        // 469B30
    state.end.year = 0xffffffffu;   // 469B36
    state.start.year = 0xffffffffu; // 469B3C
    const char *env = platform.read_tz_46c764();
    if (env == nullptr)
    {
        if (platform.get_time_zone_information(state.retained_zone) == 0xffffffffu)
            return; // 469B57
        const auto &zone = state.retained_zone;
        state.timezone_seconds = as_unsigned(zone.bias) * 60u; // 469B7D
        state.have_os_rules = 1;                               // 469B82
        if (zone.standard_date.month != 0)
            state.timezone_seconds += as_unsigned(zone.standard_bias) * 60u;
        if (zone.daylight_date.month != 0 && zone.daylight_bias != 0)
        {
            state.daylight = 1;
            state.dst_bias_seconds = (as_unsigned(zone.daylight_bias) - as_unsigned(zone.standard_bias)) * 60u;
        }
        else
        {
            state.daylight = 0;
            state.dst_bias_seconds = 0;
        }
        convert_name(state, platform, zone.standard_name, state.standard_name, 0x469becu);
        convert_name(state, platform, zone.daylight_name, state.daylight_name, 0x469c26u);
        return;
    }
    if (*env == 0)
        return; // 469C48
    if (state.cached_tz && std::strcmp(env, state.cached_tz.get()) == 0)
        return;
    state.cached_tz.reset(); // 469C71: release before allocation
    const auto length = std::strlen(env);
    if (length >= std::numeric_limits<Word>::max())
        throw SourceTimeBoundaryError(0x469c7cu, "TZ length exceeds x86 allocation domain");
    state.cached_tz = platform.allocate_tz_copy(length + 1u); // 469C7E/88, null is retained
    if (!state.cached_tz)
        return;
    std::memcpy(state.cached_tz.get(), env, length + 1u); // 469C95, source owned copy
    copy_three(state.standard_name, env);                 // 469CA3/CB3
    if (length < 3u)
        throw SourceTimeBoundaryError(0x469cb7u, "Original TZ+3 read is outside string allocation");
    for (std::size_t i = 0; i < length; ++i)
        if (static_cast<unsigned char>(env[i]) >= 128u)
            throw SourceTimeBoundaryError(0x4631f4u, "Non-ASCII CRT locale is outside admitted TZ domain");
    const char *cursor = env + 3;
    const bool negative = *cursor == '-';
    if (negative)
        ++cursor;
    state.timezone_seconds = source_atoi(cursor) * 3600u; // 469CD1
    while (*cursor == '+' || digit(static_cast<unsigned char>(*cursor)))
        ++cursor;
    if (*cursor == ':')
    {
        ++cursor;
        state.timezone_seconds += source_atoi(cursor) * 60u; // 469D00
        while (digit(static_cast<unsigned char>(*cursor)))
            ++cursor;
        if (*cursor == ':')
        {
            ++cursor;
            state.timezone_seconds += source_atoi(cursor); // 469D28
            while (digit(static_cast<unsigned char>(*cursor)))
                ++cursor;
        }
    }
    if (negative)
        state.timezone_seconds = 0u - state.timezone_seconds; // 469D41
    state.daylight = static_cast<unsigned char>(*cursor);     // 469D4C, ASCII MOVSX equals byte value
    if (state.daylight != 0)
        copy_three(state.daylight_name, cursor);
    else
        state.daylight_name[0] = 0;
    // No dst_bias_seconds write anywhere in the TZ environment branch.
}
void ensure_source_timezone_469b0a(SourceColdTimeState &state, SourceColdTimePlatform &platform)
{
    if (state.timezone_initialized == 0)
    {
        initialize_source_timezone_469b1f(state, platform);
        ++state.timezone_initialized; // 469B18, even normal child error return
    }
}
std::uint32_t convert_source_local_time_468374(SourceColdTimeState &state, SourceColdTimePlatform &platform,
                                               const SourceSystemTime &local, Word is_dst)
{
    const Word year = static_cast<Word>(local.year) - 1900u;
    if (as_signed(year) < 70 || as_signed(year) > 138)
        return 0xffffffffu;
    Word day = month_before(ordinary_months, local.month, 0x46839eu) + local.day;
    if ((year & 3u) == 0 && local.month > 2)
        ++day;
    ensure_source_timezone_469b0a(state, platform); // 4683B3, before arithmetic/DST
    const Word days = year * 365u + day + (year - 1u) / 4u;
    Word result = ((static_cast<Word>(local.hour) + days * 24u) * 60u + local.minute) * 60u + state.timezone_seconds +
                  0x7c558180u + local.second;
    if (is_dst == 1u || (is_dst == 0xffffffffu && state.daylight != 0 && source_is_dst_469d7d(state, year, day)))
        result += state.dst_bias_seconds;
    return result;
}
std::uint32_t read_source_time_4632a7(SourceColdTimeState &state, SourceColdTimePlatform &platform, Word *output)
{
    SourceSystemTime local{}, utc{};
    platform.get_local_time(local); // 4632B4
    platform.get_system_time(utc);  // 4632BE
    const auto &cached = state.cached_utc;
    if (utc.minute != cached.minute || utc.hour != cached.hour || utc.day != cached.day || utc.month != cached.month ||
        utc.year != cached.year)
    {
        SourceTimeZoneInformation zone{};
        const Word status = platform.get_time_zone_information(zone); // 463313, distinct from lazy tzset
        const Word dst = status == 0xffffffffu
                             ? 0xffffffffu
                             : (status == 2u && zone.daylight_date.month != 0 && zone.daylight_bias != 0 ? 1u : 0u);
        state.cached_utc = utc; // 46333C..48, all16 bytes beforeDST
        state.cached_dst = dst; // 46334B
    }
    const Word result = convert_source_local_time_468374(state, platform, local, state.cached_dst);
    if (output != nullptr)
        *output = result; // 46337F, only after child returns
    return result;
}
void initialize_source_random_time_42f1a0(SourceColdTimeState &state, VehicleRandom &random,
                                          SourceColdTimePlatform &platform)
{
    const Word time = read_source_time_4632a7(state, platform, nullptr); // 42F1A0: null time output
    seed_source_random_42f190(random, time);                             // 42F1AC tail, SAME4959C0 owner
}
} // namespace sgn::retail
