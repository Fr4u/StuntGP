#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace sgn::retail
{
struct VehicleRandom;
class RaceSession;
class RaceSessionPlatform;
struct SourceSystemTime
{
    std::uint16_t year{}, month{}, day_of_week{}, day{}, hour{}, minute{}, second{}, milliseconds{};
    bool operator==(const SourceSystemTime &) const = default;
};
struct SourceTimeZoneInformation
{
    std::int32_t bias{};
    std::array<char16_t, 32> standard_name{};
    SourceSystemTime standard_date;
    std::int32_t standard_bias{};
    std::array<char16_t, 32> daylight_name{};
    SourceSystemTime daylight_date;
    std::int32_t daylight_bias{};
};
static_assert(sizeof(SourceSystemTime) == 16);
static_assert(sizeof(SourceTimeZoneInformation) == 172);
struct SourceTimeTransition
{
    std::uint32_t year{0xffffffffu}, day{}, milliseconds{};
};
// Sole missing CRT source owners, retained by RaceSessionPlatform::Storage::world.
// PE startup values. This is not a second RNG and not a serialized x86 memory image.
struct SourceColdTimeState
{
    std::uint32_t cached_dst{};                        // 636580
    SourceSystemTime cached_utc;                       // 636588..636597
    std::uint32_t timezone_seconds{28800u};            // 496150
    std::uint32_t daylight{1u};                        // 496154, not necessarily Boolean
    std::uint32_t dst_bias_seconds{0xfffff1f0u};       // 496158: -3600
    std::array<char, 64> standard_name{'P', 'S', 'T'}; // 49615C; pointer4961DC aliases this
    std::array<char, 64> daylight_name{'P', 'D', 'T'}; // 49619C; pointer4961E0 aliases this
    SourceTimeTransition start;                        // 4961E8/EC/F0
    SourceTimeTransition end;                          // 4961F8/FC/6200
    std::uint32_t have_os_rules{};                     // 6365F8
    SourceTimeZoneInformation retained_zone;           // 636600..6366AB
    std::unique_ptr<char[]> cached_tz;                 // 6366AC, host-owned allocation
    std::uint32_t timezone_initialized{};              // 6366B0
    std::uint32_t code_page{};                         // 636840, PE CP_ACP
};
class SourceTimeBoundaryError final : public std::runtime_error
{
  public:
    SourceTimeBoundaryError(std::uint32_t instruction, const char *reason)
        : std::runtime_error(reason), instruction_(instruction)
    {
    }
    [[nodiscard]] std::uint32_t instruction() const noexcept
    {
        return instruction_;
    }

  private:
    std::uint32_t instruction_;
};
struct SourceNameConversion
{
    int result{};
    bool used_default{};
};
// External services. TZ must be a stable NUL-terminated ASCII C-locale snapshot;
// nullptr means absent, "" means present/empty. Original CRT bootstrap is not ported.
// Native calls and allocation may fail; no success-only replacement services.
class SourceColdTimePlatform
{
  public:
    virtual ~SourceColdTimePlatform() = default;
    virtual void get_local_time(SourceSystemTime &) = 0;
    virtual void get_system_time(SourceSystemTime &) = 0;
    virtual std::uint32_t get_time_zone_information(SourceTimeZoneInformation &) = 0;
    virtual const char *read_tz_46c764() = 0;
    virtual std::unique_ptr<char[]> allocate_tz_copy(std::size_t) = 0;
    // Native arguments fixed at flags=0x220, input=-1, output=63, default=null.
    virtual SourceNameConversion convert_zone_name(std::uint32_t code_page, const std::array<char16_t, 32> &input,
                                                   std::array<char, 64> &output) = 0;
};
void initialize_source_timezone_469b1f(SourceColdTimeState &, SourceColdTimePlatform &);
void ensure_source_timezone_469b0a(SourceColdTimeState &, SourceColdTimePlatform &);
std::uint32_t convert_source_local_time_468374(SourceColdTimeState &, SourceColdTimePlatform &,
                                               const SourceSystemTime &local, std::uint32_t is_dst);
std::uint32_t read_source_time_4632a7(SourceColdTimeState &, SourceColdTimePlatform &, std::uint32_t *output = nullptr);
void initialize_source_random_time_42f1a0(SourceColdTimeState &, VehicleRandom &, SourceColdTimePlatform &);
// Explicit pre-Session binding; the actual platform world owns all CRT
// caches and its RNG. No snapshot/default creation or source reset here.
void initialize_platform_source_random_time_42f1a0(RaceSessionPlatform &, SourceColdTimePlatform &);
// Compatibility entry delegates once to the platform binding.
void initialize_session_source_random_time_42f1a0(RaceSession &, SourceColdTimePlatform &);

// Explicit construction: caller supplies its admitted CRT TZ snapshot. A present
// empty string is distinct from absence. No constructor invokes source cold-start.
class Win32SourceColdTimePlatform final : public SourceColdTimePlatform
{
  public:
    explicit Win32SourceColdTimePlatform(std::optional<std::string> tz_snapshot);
    void get_local_time(SourceSystemTime &) override;
    void get_system_time(SourceSystemTime &) override;
    std::uint32_t get_time_zone_information(SourceTimeZoneInformation &) override;
    const char *read_tz_46c764() override;
    std::unique_ptr<char[]> allocate_tz_copy(std::size_t) override;
    SourceNameConversion convert_zone_name(std::uint32_t, const std::array<char16_t, 32> &,
                                           std::array<char, 64> &) override;

  private:
    std::optional<std::string> tz_snapshot_;
};
} // namespace sgn::retail
