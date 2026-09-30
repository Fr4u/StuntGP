#pragma once
#include "sgn/source_application_literals.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>

namespace sgn::retail
{
struct SourceBootConfigState;
// 40BDFF local128 path and40B650/40B7F0 local16/32 keys, same source formatter.
void format_source_vehicle_config_path_40bdff(const SourceBootConfigState &, const SourceApplicationLiteralRef &,
                                              std::uint32_t configuration, struct SourceScriptPathCall &);
void format_source_vehicle_config_key_46280c(const SourceBootConfigState &, const SourceApplicationLiteralRef &,
                                             std::uint32_t format, std::uint32_t index, std::span<std::uint8_t> output,
                                             std::uint32_t return_site, struct SourceScriptPathCall &);
class RaceSessionPlatform;
class SourceScriptFormatError final : public std::runtime_error
{
  public:
    const std::uint32_t source_site, source_value;
    SourceScriptFormatError(std::uint32_t site, std::uint32_t value, const char *message)
        : std::runtime_error(message), source_site(site), source_value(value)
    {
    }
};
struct SourceScriptConversion
{
    std::uint32_t precision, unknown_34, skip, width, prefix_length, flags, wide;
};
// One actual caller invocation, retained on failure to expose partial writes.
// The128 output bytes are deliberately not initialized or pre-scanned.
struct SourceScriptPathCall
{
    std::array<std::uint8_t, 128> path;
    std::size_t initialized_prefix{};          // NEW host observation, not a source local.
    std::uint32_t destination_offset{};        // FILE._ptr relative to the caller buffer.
    std::uint32_t file_remaining{0x7fffffffu}; // 46282D, not buffer capacity.
    std::uint32_t file_flags{0x42u};           // 46281F.
    std::uint32_t output_count{};              // 464758.
    std::uint32_t format_cursor{0x47796cu};
    std::uint32_t format_state{}; // 46474E and actual stored[EBP-30].
    std::uint32_t argument_slot_offset{};
    std::uint32_t argument_pointer{};
    std::uint32_t string_scan_cursor{}, string_length{};
    std::optional<SourceScriptConversion> conversion;
    // Used signed32 decimal scratch at the tail ending[EBP-49]. Unwritten
    // bytes are not initialized, scanned or emitted. Cursor is relative to10 bytes.
    std::array<std::uint8_t, 10> decimal_digits;
    std::uint32_t decimal_argument{}, decimal_magnitude{};
    std::uint32_t decimal_cursor{}, decimal_length{};
    std::uint8_t decimal_prefix{};
    bool entered{}, returned{}, terminated{};
    bool path_output{true}; // NEW observation: selected-script output belongs to config.
    std::uint32_t next_source_site{0x42d08cu};
    std::span<const std::uint8_t> written_prefix() const
    {
        if (!path_output)
            throw std::logic_error("output belongs to the external caller destination");
        return {path.data(), initialized_prefix};
    }
    std::string_view completed_path() const;
};
// Used46280C string-buffer route: state dispatch, literals/DBCS, narrow%s and plain signed%d/%i.
// Other conversion/flag/modifier handlers are explicit gaps with retained effects.
// No CRT sprintf, std::format, concatenation, parser or length preflight substitute.
std::uint32_t format_level_script_path_42d08c(const SourceBootConfigState &, const SourceApplicationLiteralRef &,
                                              SourceScriptPathCall &);
// Direct binding to existing platform-owned config and weak App literal identity.
std::uint32_t format_platform_level_script_path_42d08c(RaceSessionPlatform &, SourceScriptPathCall &);
} // namespace sgn::retail

namespace sgn::retail
{
// 445306 passes a live format pointer and NO variadic words. Writes directly
// into589604..589623. Frame exposes counters; output remains on config.
std::uint32_t format_selected_script_445306(SourceBootConfigState &, const SourceApplicationLiteralRef &,
                                            std::uint32_t format_pointer, SourceScriptPathCall &);
} // namespace sgn::retail

namespace sgn::retail
{
// One actual integer variadic word; output aliases the caller's64-byte key local.
std::uint32_t format_integer_key_46280c(const SourceBootConfigState &, const SourceApplicationLiteralRef &,
                                        std::uint32_t format_pointer, std::uint32_t argument,
                                        std::span<std::uint8_t, 64> output, SourceScriptPathCall &,
                                        std::uint32_t return_site);
} // namespace sgn::retail

namespace sgn::retail
{
// 442010 uses two actual pointer words and the App's sole612BF8 destination.
// The returned frame exposes reached effects; the version bytes stay on App.
void initialize_source_version_442010(const SourceBootConfigState &, const SourceApplicationLiteralRef &,
                                      SourceScriptPathCall &);
void initialize_platform_source_version_442010(RaceSessionPlatform &);
void initialize_platform_source_version_442010(RaceSessionPlatform &, SourceScriptPathCall &);
} // namespace sgn::retail

namespace sgn::retail
{
// 42D01A uses the live written prefix of a64-byte STACK format and zero varargs.
// Output aliases App.names starting at the given offset, checked only at stores.
// Frame format_cursor is a relative stack offset; destination_offset and
// initialized_prefix are offsets/extents in the full App names region.
void format_rolling_demo_name_42d01a(const SourceBootConfigState &, const SourceApplicationLiteralRef &,
                                     std::span<const std::uint8_t> written_local, std::uint32_t destination_offset,
                                     SourceScriptPathCall &);
void format_rolling_demo_name_42d01a(RaceSessionPlatform &, std::span<const std::uint8_t> written_local,
                                     std::uint32_t destination_offset);
void format_rolling_demo_name_42d01a(RaceSessionPlatform &, std::span<const std::uint8_t> written_local,
                                     std::uint32_t destination_offset, SourceScriptPathCall &);
} // namespace sgn::retail

namespace sgn::retail
{
void format_level_track_texture_42cc62(const SourceBootConfigState &, const SourceApplicationLiteralRef &,
                                       std::span<const std::uint8_t> written_local, std::span<std::uint8_t, 64> output,
                                       SourceScriptPathCall &);
void format_level_track_texture_42cc62(RaceSessionPlatform &, std::span<const std::uint8_t> written_local,
                                       SourceScriptPathCall &);
void format_level_track_texture_42cc62(RaceSessionPlatform &, std::span<const std::uint8_t> written_local);
} // namespace sgn::retail

namespace sgn::retail
{
// Actual442F19 local16 and419A0B local128; latter borrows former stack string.
void format_track_replay_name_442f19(const SourceBootConfigState &, const SourceApplicationLiteralRef &,
                                     std::span<std::uint8_t, 16>, SourceScriptPathCall &);
void format_replay_file_path_419a0b(const SourceBootConfigState &, const SourceApplicationLiteralRef &,
                                    std::span<const std::uint8_t> written_name, SourceScriptPathCall &);
} // namespace sgn::retail

namespace sgn::retail
{
// 43FFDD: live NUL-inclusive language borrow, one raw player word, original256-byte local.
std::uint32_t format_unavailable_message_43ffdd(const SourceBootConfigState &, const SourceApplicationLiteralRef &,
                                                std::span<const std::uint8_t> format, std::uint32_t player,
                                                std::span<std::uint8_t, 256> output, SourceScriptPathCall &);
} // namespace sgn::retail

namespace sgn::retail
{
// 42E400..42E414: one live61B640 pointer, mutable App format477C34 and
// the actual128-byte48491C destination. Frame retains reached partial effects.
std::uint32_t format_source_graphics_root_42e40f(SourceBootConfigState &, const SourceApplicationLiteralRef &,
                                                 SourceScriptPathCall &);
} // namespace sgn::retail

namespace sgn::retail
{
// Actual43D5B9 /45D0F6: borrowed format bytes, source words, existing output owner.
std::uint32_t format_selection_numbers_46280c(const SourceBootConfigState &, const SourceApplicationLiteralRef &,
                                              std::span<const std::uint8_t>, std::span<const std::uint32_t>,
                                              std::span<std::uint8_t>, std::uint32_t return_site,
                                              SourceScriptPathCall &);
} // namespace sgn::retail

namespace sgn::retail
{
struct SourceSaveStringBorrow
{
    std::uint32_t slot_offset{};
    std::span<const std::uint8_t> bytes{};
};
std::uint32_t format_save_ui_46280c(const SourceBootConfigState &, const SourceApplicationLiteralRef &,
                                    std::span<const std::uint8_t>, std::span<const std::uint32_t>,
                                    std::span<std::uint8_t>, std::uint32_t return_site, SourceScriptPathCall &,
                                    SourceSaveStringBorrow first = {}, SourceSaveStringBorrow second = {});
} // namespace sgn::retail

namespace sgn::retail
{
class SourceRequestName;
void format_camera_path_401f45(const SourceBootConfigState &, const SourceApplicationLiteralRef &,
                               const SourceRequestName &captured_name, SourceScriptPathCall &);
} // namespace sgn::retail

namespace sgn::retail
{
// Actual4012FE and443F10 calls, borrowed App format pointer and real destination.
std::uint32_t format_race_lifecycle_46280c(const SourceBootConfigState &, const SourceApplicationLiteralRef &,
                                           std::uint32_t format_address, std::span<const std::uint32_t> arguments,
                                           std::span<std::uint8_t> output, std::uint32_t return_site,
                                           SourceScriptPathCall &);
} // namespace sgn::retail

namespace sgn::retail
{
// Actual41869F/4191C2 stack destination and borrowed existing vehicle/language bytes.
void format_respawn_notice_46280c(const SourceBootConfigState &, const SourceApplicationLiteralRef &,
                                  std::uint32_t format_address, std::span<const std::uint8_t> name,
                                  std::optional<std::span<const std::uint8_t>> language,
                                  std::span<std::uint8_t, 64> output, std::uint32_t return_site,
                                  SourceScriptPathCall &);
} // namespace sgn::retail

namespace sgn::retail
{
class FrontendState;
// 446A0E/446A32: same128-byte caller local reused; exact source arguments.
void format_source_level_configuration_4469f0(FrontendState &, bool fallback, std::span<std::uint8_t, 128> caller_local,
                                              SourceScriptPathCall &);
} // namespace sgn::retail
