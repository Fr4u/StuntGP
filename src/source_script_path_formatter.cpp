#include "sgn/source_script_path_formatter.hpp"
#include "sgn/source_boot_config.hpp"
#include "sgn/source_pmd_requests.hpp"

namespace sgn::retail
{
namespace
{
bool negative(std::uint32_t bits) noexcept
{
    return (bits & 0x80000000u) != 0;
}
[[noreturn]] void gap(SourceScriptPathCall &call, std::uint32_t site, std::uint32_t value, const char *reason)
{
    call.next_source_site = site;
    throw SourceScriptFormatError(site, value, reason);
}
std::uint8_t argument_byte(const SourceBootConfigState &config, const SourceApplicationLiteralRef &app,
                           std::uint32_t address, std::uint32_t site)
{
    // Reads actual fields at their retail addresses; no copied temporary C string.
    const auto location = static_cast<std::uint32_t>(address - 0x61b640u);
    if (location < config.game_location.size())
        return static_cast<std::uint8_t>(config.game_location[location]);
    const auto level = static_cast<std::uint32_t>(address - 0x589604u);
    if (level < config.level_script.size())
        return static_cast<std::uint8_t>(config.level_script[level]);
    const auto base = static_cast<std::uint32_t>(address - 0x48489cu);
    if (base < config.base_path.size())
        return static_cast<std::uint8_t>(config.base_path[base]);
    const auto graphics = static_cast<std::uint32_t>(address - 0x48491cu);
    if (graphics < config.graphics_path.size())
        return static_cast<std::uint8_t>(config.graphics_path[graphics]);
    const auto maximum = static_cast<std::uint32_t>(address - 0x48499cu);
    if (maximum < config.maximum_graphics_path.size())
        return static_cast<std::uint8_t>(config.maximum_graphics_path[maximum]);
    const auto sound = static_cast<std::uint32_t>(address - 0x484a1cu);
    if (sound < config.sound_path.size())
        return static_cast<std::uint8_t>(config.sound_path[sound]);
    return app.read_formatter_byte(address, site); // Includes actual CRT null-string owner.
}
std::uint8_t format_byte(SourceScriptPathCall &call, const SourceApplicationLiteralRef &app, std::uint32_t site,
                         std::optional<std::span<const std::uint8_t>> stack_format)
{
    if (!stack_format)
        return app.read_formatter_byte(call.format_cursor, site);
    if (call.format_cursor >= stack_format->size())
        gap(call, site, call.format_cursor,
            "unowned caller-stack format byte; value is relative offset, not PE address");
    return (*stack_format)[call.format_cursor]; // Live written local bytes, no copy/scan.
}
void emit_byte(SourceScriptPathCall &call, std::span<std::uint8_t> output, std::uint8_t byte)
{
    --call.file_remaining; // 464E86 happens before reaching destination store.
    if (negative(call.file_remaining))
    {
        // 46462A reads the uninitialized FILE descriptor field at464635 before
        // testing flags. This huge-output route needs a full raw FILE frame.
        gap(call, 0x464635, call.file_remaining, "unreconstructed CRT FILE descriptor read on count exhaustion");
    }
    call.next_source_site = 0x464e90;
    if (call.destination_offset >= output.size())
        gap(call, 0x464e90, call.destination_offset, "reached output store beyond admitted caller destination");
    output[call.destination_offset] = byte;
    ++call.destination_offset; // 464E92, only after successful store.
    call.initialized_prefix = call.destination_offset;
    ++call.output_count; // 464EB1, after pointer advance.
}
struct StackArgument
{
    std::uint32_t slot_offset;
    std::span<const std::uint8_t> bytes;
    std::uint32_t second_slot{};
    std::span<const std::uint8_t> second_bytes{};
    // 44D56F and camera401F45 bind borrowed strings with actual source DWORD addresses.
    // All caller-stack/native identities retain zero and cannot become integers.
    std::uint32_t raw_source_word_slot{};
    std::optional<SourceRequestName> camera_request;
};
bool borrowed_slot(std::optional<StackArgument> s, std::uint32_t slot)
{
    return s && (s->slot_offset == slot || (s->second_slot != 0 && s->second_slot == slot));
}

std::uint8_t string_argument_byte(SourceScriptPathCall &call, const SourceBootConfigState &config,
                                  const SourceApplicationLiteralRef &app, std::uint32_t offset, std::uint32_t site,
                                  std::optional<StackArgument> stack)
{
    if (borrowed_slot(stack, call.argument_slot_offset))
    {
        if (stack->camera_request)
            return stack->camera_request->read_formatter_byte(offset, site);
        const auto bytes = call.argument_slot_offset == stack->slot_offset ? stack->bytes : stack->second_bytes;
        if (offset >= bytes.size())
            gap(call, site, offset, "unwritten caller-stack string byte; value is relative offset");
        return bytes[offset];
    }
    return argument_byte(config, app, offset, site);
}
void emit_string(SourceScriptPathCall &call, const SourceBootConfigState &config,
                 const SourceApplicationLiteralRef &app, std::span<std::uint8_t> output,
                 std::optional<StackArgument> stack)
{
    // 464EE6: signed length<=0 performs no source or output access.
    if (call.string_length == 0 || negative(call.string_length))
        return;
    auto remaining = call.string_length;
    auto cursor = call.argument_pointer;
    do
    {
        call.next_source_site = 0x464efc;
        const auto byte = string_argument_byte(call, config, app, cursor, 0x464efc, stack);
        ++cursor; // 464F00 precedes464F06 writer call.
        emit_byte(call, output, byte);
        if (call.output_count == 0xffffffffu)
            return;
        --remaining;
    } while (remaining != 0);
}
void narrow_string(SourceScriptPathCall &call, const SourceBootConfigState &config,
                   const SourceApplicationLiteralRef &app, std::span<std::uint8_t> output,
                   std::span<const std::uint32_t> arguments, std::optional<StackArgument> stack)
{
    if (!call.conversion)
        gap(call, 0x46496c, 0, "narrow string entered without initialized conversion locals");
    const auto &conversion = *call.conversion;
    auto scan_limit = conversion.precision;
    if (scan_limit == 0xffffffffu)
        scan_limit = 0x7fffffffu;   // 46496F..464974.
    call.argument_slot_offset += 4; // 464F22 writes vararg cursor BEFORE464F27 reads slot.
    if (call.argument_slot_offset == 0 || call.argument_slot_offset / 4u > arguments.size())
        gap(call, 0x464f27, call.argument_slot_offset - 4, "reached unowned variadic stack word");
    const bool stack_pointer = borrowed_slot(stack, call.argument_slot_offset);
    // Stack argument is a borrowed identity, not a copied/truncated host pointer.
    // Its argument_pointer and scan cursor are relative offsets; zero is NOT NULL.
    call.argument_pointer = stack_pointer ? 0u : arguments[call.argument_slot_offset / 4u - 1u];
    if ((conversion.flags & 0x810u) != 0)
        gap(call, 0x464994, conversion.flags, "wide-string conversion remains unreconstructed");
    if (!stack_pointer && call.argument_pointer == 0)
    {
        call.next_source_site = 0x464b96;
        call.argument_pointer = app.read_formatter_word(0x4959f8, 0x464b96);
    }
    call.string_scan_cursor = call.argument_pointer;
    for (;;)
    {
        const auto before = scan_limit;
        --scan_limit; // 464BA3, even on precision zero.
        if (before == 0)
            break;
        call.next_source_site = 0x464ba8;
        if (string_argument_byte(call, config, app, call.string_scan_cursor, 0x464ba8, stack) == 0)
            break;
        ++call.string_scan_cursor;
    }
    call.string_length = call.string_scan_cursor - call.argument_pointer; // 464BB0/464D46.
    // The only accepted conversion producer is the actual state1 initializer:
    // zero flags/width/prefix/wide. Its464EB5 padding counts are nonpositive,
    // and its zero-length prefix464EE6 call accesses no source memory.
    // Unknown handlers stop at their real dispatch address before changing these.
    if (conversion.skip != 0)
        return;
    if (conversion.flags != 0 || conversion.width != 0 || conversion.prefix_length != 0 || conversion.wide != 0)
        gap(call, 0x464d53, conversion.flags, "non-plain string output formatting remains unreconstructed");
    emit_string(call, config, app, output, stack); // 464E27 ->464EE6, second pass after complete scan.
}
void signed_decimal(SourceScriptPathCall &call, std::span<std::uint8_t> output,
                    std::span<const std::uint32_t> arguments, std::optional<StackArgument> stack)
{
    if (!call.conversion)
        gap(call, 0x464c24, 0, "signed decimal entered without initialized conversion locals");
    auto &conversion = *call.conversion;
    conversion.flags |= 0x40u; // 464C24, signed conversion, before argument fetch.
    // The shared state1 route initializes zero flags/width/prefix/wide and
    // precisionFFFFFFFF. Zero-flag/literal-width handlers are admitted below.
    if ((conversion.flags & ~0x48u) != 0 || conversion.prefix_length != 0 || conversion.wide != 0)
        gap(call, 0x464c2f, conversion.flags, "non-plain signed-decimal modifiers remain unreconstructed");
    call.next_source_site = 0x464f22;
    call.argument_slot_offset += 4; // 464C72 ->464F1E; cursor store precedes read.
    if (call.argument_slot_offset == 0 || call.argument_slot_offset / 4u > arguments.size())
        gap(call, 0x464f27, call.argument_slot_offset - 4, "reached unowned integer variadic stack word");
    if (borrowed_slot(stack, call.argument_slot_offset) && stack->raw_source_word_slot != call.argument_slot_offset)
        gap(call, 0x464f27, call.argument_slot_offset - 4,
            "raw integer interpretation of caller-stack pointer is outside native identity contract");
    call.decimal_argument = arguments[call.argument_slot_offset / 4u - 1u];
    // 464C78 ->464C5A CDQ. Negative EDX takes464C92; the nonnegative
    // EDX/EAX path retains magnitude. Unsigned negation also covers80000000.
    call.decimal_magnitude = call.decimal_argument;
    if (negative(call.decimal_argument))
    {
        call.decimal_magnitude = 0u - call.decimal_argument; // 464C92 NEG EAX.
        conversion.flags |= 0x100u;                          // 464C9B, sign prefix flag in BH.
    }
    // 464CAD clears magnitude high DWORD for the non-I64 branch.
    if (negative(conversion.precision))
        conversion.precision = 1; // 464CB0..464CB6.
    else
        conversion.flags &= 0xfffffff7u; // 464CBF.
    if (call.decimal_magnitude == 0)
        conversion.prefix_length = 0; // 464CC3..464CC9.
    call.decimal_cursor = 9;          // 464CCD..464CD0: end at[EBP-49].
    for (;;)
    {
        const auto before = conversion.precision;
        --conversion.precision; // 464CD6, including terminating iteration.
        if ((before == 0 || negative(before)) && call.decimal_magnitude == 0)
            break;
        // 464CF1 calls46B660 remainder BEFORE464D03 calls46B5F0 quotient.
        // Divisor=10, divisorHigh=0, magnitudeHigh=0: both helpers take their
        // two-DIV low-divisor routes; first DIV is0/10, second uses this DWORD.
        const auto remainder = call.decimal_magnitude % 10u;
        const auto digit = remainder + 0x30u; // 464CFB, always<=39 in radix10.
        call.decimal_magnitude /= 10u;
        const auto destination = call.decimal_cursor;
        --call.decimal_cursor; // 464D17 precedes464D1A scratch store.
        if (destination >= call.decimal_digits.size())
            gap(call, 0x464d1a, destination, "decimal scratch reaches unowned caller local");
        call.decimal_digits[destination] = static_cast<std::uint8_t>(digit);
    }
    call.decimal_length = 9u - call.decimal_cursor; // 464D1E..464D21.
    ++call.decimal_cursor;                          // 464D24, first digit.
    if (conversion.skip != 0)
        return; // 464D49.
    if ((conversion.flags & 0x100u) != 0)
    {
        call.decimal_prefix = 0x2d;   // 464D60.
        conversion.prefix_length = 1; // 464D7A.
    }
    // 464D7D subtracts the sign and digit counts with source DWORD wrapping.
    const auto padding = conversion.width - conversion.prefix_length - call.decimal_length;
    const auto pad = [&](std::uint8_t value) {
        auto remaining = padding;
        while (remaining != 0 && !negative(remaining))
        {
            --remaining; // 464EBD/464EDE before reached464ECF writer
            emit_byte(call, output, value);
            if (call.output_count == 0xffffffffu)
                break;
        }
    };
    if ((conversion.flags & 8u) == 0)
        pad(0x20); // 464D88..464D99

    if (conversion.prefix_length != 0)
        emit_byte(call, output, call.decimal_prefix); // 464DAF ->464EE6 prefix first.
    if ((conversion.flags & 8u) != 0)
        pad(0x30); // 464DB7..464DCB, after sign
    // 464E27 ->464EE6 scans only generated digits.
    auto cursor = call.decimal_cursor;
    auto remaining = call.decimal_length;
    while (remaining != 0)
    {
        const auto byte = call.decimal_digits[cursor];
        ++cursor; // 464F00, before writer call.
        emit_byte(call, output, byte);
        if (call.output_count == 0xffffffffu)
            return;
        --remaining;
    }
}
} // namespace

std::string_view SourceScriptPathCall::completed_path() const
{
    if (!path_output || !returned || !terminated || initialized_prefix == 0)
        throw std::logic_error("formatter has not returned a terminated path");
    return {reinterpret_cast<const char *>(path.data()), initialized_prefix - 1};
}
static std::uint32_t format_script_buffer_46280c(
    const SourceBootConfigState &config, const SourceApplicationLiteralRef &app, SourceScriptPathCall &call,
    std::span<std::uint8_t> output, std::span<const std::uint32_t> arguments, std::uint32_t return_site,
    std::optional<std::span<const std::uint8_t>> stack_format = std::nullopt,
    std::optional<StackArgument> stack_argument = std::nullopt)
{
    if (call.entered)
        throw std::logic_error("use a fresh invocation frame; failed formatting is not restarted or rolled back");
    call.entered = true;
    // 46280C builds a separate FILE with countINT_MAX. The destination aliases
    // actual caller storage. No truncation, zero-fill or source pre-scan.
    call.next_source_site = 0x464750;
    auto character = format_byte(call, app, 0x464750, stack_format);
    ++call.format_cursor;
    for (;;)
    {
        if (character == 0 || negative(call.output_count))
            break; // 46475E/464776.
        auto eax = app.classify_u8_46477c(character);
        call.next_source_site = 0x464796;
        const auto branch = app.consume_state_464796(call.format_state, eax, call.format_state);
        if (branch.next_site == 0x4647ad)
        {
            call.next_source_site = 0x4647ad;
            const auto destination = app.read_formatter_word(0x464e60u + eax * 4u, 0x4647ad);
            switch (destination)
            {
            case 0x4647b4: // Full state1 initialization; no string output yet.
                call.conversion = SourceScriptConversion{0xffffffffu, 0, 0, 0, 0, 0, 0};
                break;
            case 0x4647cf:
                if (!call.conversion)
                    gap(call, 0x4647cf, character, "flag without conversion locals");
                if (character != '0')
                    gap(call, 0x4647cf, character, "selection extension admits only zero flag");
                call.conversion->flags |= 8u; // 4647EE
                break;
            case 0x46481b:
                if (!call.conversion)
                    gap(call, 0x46481b, character, "width without conversion locals");
                if (character < '0' || character > '9')
                    gap(call, 0x46481b, character, "dynamic width remains unreconstructed");
                call.conversion->width =
                    call.conversion->width * 10u + std::uint32_t(character) - 0x30u; // 464843..46484C
                break;
            case 0x4648de: { // State0, actual current CRT pointer and classification.
                call.next_source_site = 0x4648de;
                const auto ctype = app.read_formatter_word(0x495db0, 0x4648de);
                if (call.conversion)
                    call.conversion->wide = 0; // 4648E4.
                call.next_source_site = 0x4648ea;
                const auto classification =
                    app.read_formatter_byte(ctype + static_cast<std::uint32_t>(character) * 2u + 1u, 0x4648ea);
                if ((classification & 0x80u) != 0)
                {
                    emit_byte(call, output, character); // 4648FC, first byte remains if next read faults.
                    call.next_source_site = 0x464901;
                    character = format_byte(call, app, 0x464901, stack_format);
                    ++call.format_cursor;
                }
                emit_byte(call, output, character); // 464915, includes the second DBCS byte.
                break;
            }
            case 0x464922:
                // Actual dispatch: d via464A2B..464A36; i via464B4A..464B4D;
                // both enter464C24. Lower s reaches46496C via464B69.
                if (character == 'd' || character == 'i')
                    signed_decimal(call, output, arguments, stack_argument);
                else if (character == 's')
                    narrow_string(call, config, app, output, arguments, stack_argument);
                else
                    gap(call, 0x464922, character,
                        "formatter conversion other than narrow s or signed decimal remains unreconstructed");
                break;
            default:
                gap(call, destination, character,
                    "formatter flag/width/precision/modifier handler remains unreconstructed");
            }
        }
        // Both supported handler paths and JA-above return to the same source loop.
        call.next_source_site = 0x464e4a;
        character = format_byte(call, app, 0x464e4a, stack_format);
        ++call.format_cursor;
    }
    const auto result = call.output_count; // 464E58;462840 preserves it even if negative.
    --call.file_remaining;                 // 46283D, before terminator access.
    if (negative(call.file_remaining))
        gap(call, 0x464635, call.file_remaining, "unreconstructed FILE descriptor read in terminator slow path");
    call.next_source_site = 0x462847;
    if (call.destination_offset >= output.size())
        gap(call, 0x462847, call.destination_offset, "reached terminator RMW beyond admitted caller destination");
    // 462847 AND BYTE[ptr],0: old raw stack value is irrelevant to the result.
    // Keep the mapped-byte check but never read an indeterminate C++ scalar.
    output[call.destination_offset] = 0;
    call.initialized_prefix = static_cast<std::size_t>(call.destination_offset) + 1;
    call.terminated = true;
    call.returned = true;
    call.next_source_site = return_site;
    return result;
}
} // namespace sgn::retail

namespace sgn::retail
{
std::uint32_t format_level_script_path_42d08c(const SourceBootConfigState &config,
                                              const SourceApplicationLiteralRef &app, SourceScriptPathCall &call)
{
    const std::array<std::uint32_t, 1> arguments{0x48489cu};
    return format_script_buffer_46280c(config, app, call, call.path, arguments, 0x42d094u);
}
std::uint32_t format_selected_script_445306(SourceBootConfigState &config, const SourceApplicationLiteralRef &app,
                                            std::uint32_t format_pointer, SourceScriptPathCall &call)
{
    if (call.entered)
        throw std::logic_error("selected-script formatter needs a fresh invocation frame");
    call.format_cursor = format_pointer;
    call.path_output = false;
    // Unsigned char representation access directly aliases the real589604 field.
    auto output = std::span<std::uint8_t>(reinterpret_cast<std::uint8_t *>(config.level_script.data()),
                                          config.level_script.size());
    return format_script_buffer_46280c(config, app, call, output, {}, 0x44530bu);
}
} // namespace sgn::retail

namespace sgn::retail
{
std::uint32_t format_integer_key_46280c(const SourceBootConfigState &config, const SourceApplicationLiteralRef &app,
                                        std::uint32_t format_pointer, std::uint32_t argument,
                                        std::span<std::uint8_t, 64> output, SourceScriptPathCall &call,
                                        std::uint32_t return_site)
{
    if (call.entered)
        throw std::logic_error("integer key formatter needs a fresh invocation frame");
    call.format_cursor = format_pointer;
    call.path_output = false;
    const std::array<std::uint32_t, 1> arguments{argument};
    return format_script_buffer_46280c(config, app, call, output, arguments, return_site);
}
} // namespace sgn::retail

namespace sgn::retail
{
void initialize_source_version_442010(const SourceBootConfigState &config, const SourceApplicationLiteralRef &app,
                                      SourceScriptPathCall &call)
{
    if (call.entered)
        throw std::logic_error("version initializer needs a fresh CRT invocation frame");
    // 442010/442015 push time then date. Varargs are read in forward order:
    // first4826A4 (date), then482698 (time), NOT copied pointee snapshots.
    const std::array<std::uint32_t, 2> arguments{0x4826a4u, 0x482698u};
    call.format_cursor = 0x4826b0u; // 44201A PUSH actual format address.
    call.path_output = false;
    call.next_source_site = 0x44201f;
    const auto output = app.version_destination_44201f(); // same App, no clear.
    (void)format_script_buffer_46280c(config, app, call, output, arguments, 0x442029u);
    call.next_source_site = 0x44202c; // 442029 discards four stack words,44202C RET.
}
} // namespace sgn::retail

namespace sgn::retail
{
void format_rolling_demo_name_42d01a(const SourceBootConfigState &config, const SourceApplicationLiteralRef &app,
                                     std::span<const std::uint8_t> written_local, std::uint32_t destination_offset,
                                     SourceScriptPathCall &call)
{
    if (call.entered)
        throw std::logic_error("rolling-demo formatter needs a fresh CRT invocation frame");
    if (written_local.size() > 64)
        throw std::invalid_argument("rolling-demo format belongs to a64-byte caller local");
    call.path_output = false;
    call.format_cursor = 0;                       // Relative caller-stack offset; no invented absolute VA.
    call.destination_offset = destination_offset; // 6129A4+ESI offset, NOT a subspan precheck.
    call.next_source_site = 0x42d01a;
    auto &storage = app.rolling_demo_storage();
    // No destination bounds check here. The shared formatter first reads the
    // actual stack format;464E90/462847 check only the reached names store.
    (void)format_script_buffer_46280c(config, app, call, storage.names, {}, 0x42d01fu, written_local);
    // 42D01F pointer publication and42D032 strlen of LOCAL belong to Loader.
}
} // namespace sgn::retail

namespace sgn::retail
{
void format_level_track_texture_42cc62(const SourceBootConfigState &config, const SourceApplicationLiteralRef &app,
                                       std::span<const std::uint8_t> written_local, std::span<std::uint8_t, 64> output,
                                       SourceScriptPathCall &call)
{
    if (call.entered)
        throw std::logic_error("track texture formatter needs a fresh CRT invocation frame");
    if (written_local.size() > 128)
        throw std::invalid_argument("track texture format belongs to a128-byte caller local");
    call.path_output = false;
    call.format_cursor = 0; // Caller-stack offset, not a fabricated PE VA.
    call.next_source_site = 0x42cc62;
    // 42CC5C/5D supply local format and actual60DFAC destination, no varargs.
    // No pre-scan, destination zero-fill, staging or sprintf replacement.
    (void)format_script_buffer_46280c(config, app, call, output, {}, 0x42cc67u, written_local);
}
} // namespace sgn::retail

namespace sgn::retail
{
void format_track_replay_name_442f19(const SourceBootConfigState &config, const SourceApplicationLiteralRef &app,
                                     std::span<std::uint8_t, 16> output, SourceScriptPathCall &call)
{
    call.format_cursor = 0x482714;
    call.path_output = false;
    const std::array<std::uint32_t, 1> arguments{0x589604};
    (void)format_script_buffer_46280c(config, app, call, output, arguments, 0x442f1e);
}
void format_replay_file_path_419a0b(const SourceBootConfigState &config, const SourceApplicationLiteralRef &app,
                                    std::span<const std::uint8_t> written_name, SourceScriptPathCall &call)
{
    call.format_cursor = 0x474698;
    // Second slot is tagged as an actual borrowed caller-stack identity. The0
    // is never consumed as a raw source address or scalar variadic value.
    const std::array<std::uint32_t, 2> arguments{0x48489c, 0};
    (void)format_script_buffer_46280c(config, app, call, call.path, arguments, 0x419a10, std::nullopt,
                                      StackArgument{8, written_name});
}
} // namespace sgn::retail

namespace sgn::retail
{
std::uint32_t format_unavailable_message_43ffdd(const SourceBootConfigState &config,
                                                const SourceApplicationLiteralRef &app,
                                                std::span<const std::uint8_t> format, std::uint32_t player,
                                                std::span<std::uint8_t, 256> output, SourceScriptPathCall &call)
{
    if (call.entered)
        throw std::logic_error("unavailable message needs a fresh invocation frame");
    call.format_cursor = 0; // Relative cursor into the borrowed language bytes.
    call.path_output = false;
    const std::array<std::uint32_t, 1> arguments{player};
    return format_script_buffer_46280c(config, app, call, output, arguments, 0x43ffe2u, format);
}
} // namespace sgn::retail

namespace sgn::retail
{
std::uint32_t format_source_graphics_root_42e40f(SourceBootConfigState &config, const SourceApplicationLiteralRef &app,
                                                 SourceScriptPathCall &call)
{
    if (call.entered)
        throw std::logic_error("graphics root formatter needs a fresh CRT invocation frame");
    call.path_output = false;
    call.format_cursor = 0x477c34; // 42E405 PUSH, actual mutable App region.
    call.next_source_site = 0x42e40f;
    const std::array<std::uint32_t, 1> arguments{0x61b640}; // 42E400 PUSH, live config owner.
    auto output = std::span<std::uint8_t>(reinterpret_cast<std::uint8_t *>(config.graphics_path.data()),
                                          config.graphics_path.size());
    // Same46280C/46473F dispatch, string scan, emit and terminator paths.
    // Only reached stores alter48491C. No pre-scan, copy, clear or rollback;
    // an overflow rejects at the reached store before crossing into48499C.
    return format_script_buffer_46280c(config, app, call, output, arguments, 0x42e414);
}
} // namespace sgn::retail

namespace sgn::retail
{
std::uint32_t format_selection_numbers_46280c(const SourceBootConfigState &config,
                                              const SourceApplicationLiteralRef &app,
                                              std::span<const std::uint8_t> format,
                                              std::span<const std::uint32_t> arguments, std::span<std::uint8_t> output,
                                              std::uint32_t return_site, SourceScriptPathCall &call)
{
    if (call.entered)
        throw std::logic_error("selection format needs a fresh invocation frame");
    if (return_site != 0x43d5be && return_site != 0x45d0fb && return_site != 0x44cefa)
        throw std::logic_error("unbound selection formatter callsite");
    call.format_cursor = 0;
    call.path_output = false;
    return format_script_buffer_46280c(config, app, call, output, arguments, return_site, format);
}
} // namespace sgn::retail

namespace sgn::retail
{
std::uint32_t format_save_ui_46280c(const SourceBootConfigState &config, const SourceApplicationLiteralRef &app,
                                    std::span<const std::uint8_t> format, std::span<const std::uint32_t> args,
                                    std::span<std::uint8_t> output, std::uint32_t return_site,
                                    SourceScriptPathCall &call, SourceSaveStringBorrow first,
                                    SourceSaveStringBorrow second)
{
    if (call.entered)
        throw std::logic_error("save UI formatter requires fresh source frame");
    switch (return_site)
    {
    case 0x4406be:
    case 0x44052c:
    case 0x44067e:
    case 0x4403fc:
    case 0x440603:
    case 0x458379:
    case 0x45839b:
    case 0x45866f:
    case 0x458145:
    case 0x4392f3:
    case 0x43d5be:
    case 0x457b78:
    case 0x434e3c:
    case 0x434ee3:
    case 0x434f90:
    case 0x43577a:
    case 0x4357de:
    case 0x4358a0:
    case 0x4358e5:
    case 0x4359ac:
    case 0x435a51:
    case 0x435ad0:
    case 0x435e5a:
    case 0x435fc9:
    case 0x450ef9:
    case 0x45109f:
    case 0x4510da: // PageCampaignMenu heading and row numeric columns
    case 0x45ab40:
    case 0x45b0c6:
    case 0x45b1e6: // PageSlot duration, sponsor text, race number
    case 0x44c665:
    case 0x44c693:
    case 0x44c861:
    case 0x44c93a: // Campaign signed award/duration values
    case 0x45405e: // PageModify signed remaining cash, borrowed numeric string
    case 0x44d56f: // StuntChallenge: captured43D690 source address plus its live string owner
    case 0x45dcc6:
    case 0x45dcee:
    case 0x45dd9a: // PageUnlock borrowed-name formats
    case 0x45a0d8:
    case 0x45a145:
    case 0x45a242: // PageArcadeScore %d, %07d, +%d
    case 0x4595e8:
    case 0x4596d5:
    case 0x4596ec:
        break; // Records rank, %03d, %07d
    default:
        throw std::logic_error("unbound save UI format callsite");
    }
    if (second.slot_offset != 0 && first.slot_offset == 0)
        throw std::logic_error("second borrowed string without first");
    call.format_cursor = 0;
    call.path_output = false;
    std::optional<StackArgument> strings;
    if (first.slot_offset != 0)
        strings = StackArgument{first.slot_offset, first.bytes, second.slot_offset, second.bytes};
    if (return_site == 0x44d56f)
    {
        // 44D554->43D690 captures[479668];44D55E pushes that returned raw DWORD
        //(initially47966C); the score was already pushed at44D54E. The borrow
        // aliases that address's four actual bytes, never an opaque host pointer.
        if (!strings || first.slot_offset != 4 || first.bytes.size() != 4 || second.slot_offset != 0 ||
            args.size() != 2)
            throw std::logic_error("44D56F requires its source-address/initials borrow and score");
        strings->raw_source_word_slot = 4;
    }
    return format_script_buffer_46280c(config, app, call, output, args, return_site, format, strings);
}
} // namespace sgn::retail

namespace sgn::retail
{
void format_camera_path_401f45(const SourceBootConfigState &config, const SourceApplicationLiteralRef &app,
                               const SourceRequestName &captured_name, SourceScriptPathCall &call)
{
    call.format_cursor = 0;
    call.next_source_site = 0x401f45;
    const std::array<std::uint32_t, 2> arguments{0x48489c, captured_name.source_address_value_462890()};
    std::optional<StackArgument> name;
    if (captured_name)
    {
        name = StackArgument{8, {}};
        name->raw_source_word_slot = 8;
        name->camera_request = captured_name;
    }
    (void)format_script_buffer_46280c(config, app, call, call.path, arguments, 0x401f4a,
                                      app.camera_startup_literal(0x472ad8), name);
}
} // namespace sgn::retail

namespace sgn::retail
{
std::uint32_t format_race_lifecycle_46280c(const SourceBootConfigState &config, const SourceApplicationLiteralRef &app,
                                           std::uint32_t format_address, std::span<const std::uint32_t> arguments,
                                           std::span<std::uint8_t> output, std::uint32_t return_site,
                                           SourceScriptPathCall &call)
{
    switch (return_site)
    {
    case 0x401303:
    case 0x443f88:
    case 0x443fb8:
    case 0x444002:
    case 0x444012:
    case 0x444032:
        break;
    default:
        throw std::logic_error("unbound source lifecycle formatter site");
    }
    call.format_cursor = format_address;
    call.path_output = false;
    return format_script_buffer_46280c(config, app, call, output, arguments, return_site);
}
} // namespace sgn::retail

namespace sgn::retail
{
void format_respawn_notice_46280c(const SourceBootConfigState &config, const SourceApplicationLiteralRef &app,
                                  std::uint32_t format_address, std::span<const std::uint8_t> name,
                                  std::optional<std::span<const std::uint8_t>> language,
                                  std::span<std::uint8_t, 64> output, std::uint32_t return_site,
                                  SourceScriptPathCall &call)
{
    if (return_site != 0x4186a4 && return_site != 0x4191c7 && return_site != 0x40d747 && return_site != 0x419157)
        throw std::logic_error("unbound vehicle notice formatter site");
    call.format_cursor = format_address;
    call.path_output = false;
    const std::array<std::uint32_t, 2> args{0, 0};
    StackArgument strings{4, name};
    if (language)
    {
        strings.second_slot = 8;
        strings.second_bytes = *language;
    }
    (void)format_script_buffer_46280c(config, app, call, output, args, return_site, std::nullopt, strings);
}
} // namespace sgn::retail

namespace sgn::retail
{
void format_source_vehicle_config_path_40bdff(const SourceBootConfigState &config,
                                              const SourceApplicationLiteralRef &app, std::uint32_t configuration,
                                              SourceScriptPathCall &call)
{
    if (call.entered)
        throw std::logic_error("40BDFF requires a fresh caller128 frame");
    call.format_cursor = 0;
    call.next_source_site = 0x40bdff;
    const std::array<std::uint32_t, 2> arguments{0x48489c, configuration};
    (void)format_script_buffer_46280c(config, app, call, call.path, arguments, 0x40be04,
                                      app.vehicle_configuration_literal(0x4738e4));
}
void format_source_vehicle_config_key_46280c(const SourceBootConfigState &config,
                                             const SourceApplicationLiteralRef &app, std::uint32_t format,
                                             std::uint32_t index, std::span<std::uint8_t> output,
                                             std::uint32_t return_site, SourceScriptPathCall &call)
{
    if (call.entered)
        throw std::logic_error("vehicle key requires a fresh source invocation frame");
    if (!((return_site == 0x40b655 && format == 0x47355c && output.size() == 16) ||
          (return_site == 0x40b7f5 && format == 0x473614 && output.size() == 32)))
        throw std::logic_error("unbound vehicle key source call/local extent");
    call.format_cursor = 0;
    call.path_output = false;
    const std::array<std::uint32_t, 1> arguments{index};
    (void)format_script_buffer_46280c(config, app, call, output, arguments, return_site,
                                      app.vehicle_configuration_literal(format));
}
} // namespace sgn::retail

#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session.hpp"
namespace sgn::retail
{
void format_source_level_configuration_4469f0(FrontendState &f, bool fallback, std::span<std::uint8_t, 128> local,
                                              SourceScriptPathCall &call)
{
    const auto p = f.shared_platform();
    p->require_owner_thread();
    if (!p->source_construction_only_entry() || &p->state() != &f.world())
        throw FormatError("level configuration formatter requires actual source frontend/platform");
    if (call.entered)
        throw std::logic_error("4469F0 formatter requires a fresh invocation frame");
    const std::uint32_t address = fallback ? 0x4850f0u : 0x4850d8u;
    const auto format = f.source.source_bytes(address, 0x485110u - address);
    const std::array<std::uint32_t, 2> arguments{0x48489c, 0x589604};
    call.format_cursor = 0;
    call.path_output = false;
    call.next_source_site = fallback ? 0x446a32u : 0x446a0eu;
    (void)format_script_buffer_46280c(p->state().boot_config, p->source_application_literals(), call, local,
                                      std::span<const std::uint32_t>{arguments}.first(fallback ? 1u : 2u),
                                      fallback ? 0x446a37u : 0x446a13u, format);
}
} // namespace sgn::retail
