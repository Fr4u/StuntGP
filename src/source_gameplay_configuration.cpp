#include "sgn/source_gameplay_configuration.hpp"
#include "sgn/source_configuration_file.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_csv.hpp"
#include "sgn/retail_gameplay_tables.hpp"
#include "sgn/source_boot_config.hpp"
#include "sgn/source_decimal_conversion.hpp"
#include "sgn/source_native_crt.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <string>

// Actual owner pointers, not float arguments whose ABI could round early.
extern "C" void sgn_source_gameplay_multiply_store_x87(const std::uint64_t *, const std::uint32_t *, float *) noexcept;

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;

std::uint64_t gameplay_number_4457c0(RaceSessionPlatform &platform, NativeSourceMenuCrt &crt, GameplayRules &rules,
                                     std::span<const std::uint8_t> written_value)
{
    rules.remaining_bytes -= 4u; // 4457C0..4457C5, source DWORD arithmetic
    // 4457C9 loads source float46E1EC=+0 on budget exhaustion. Represent
    // that value as binary64+0 for the same final FLD/FMUL/FSTP leaf.
    if (std::bit_cast<std::int32_t>(rules.remaining_bytes) < 0)
        return 0;                                                                  // 4457C7 JNS
    return convert_source_local_atof_bounded_4631a7(platform, crt, written_value); // 4457D1
}

void apply_gameplay_row_4456f0(RaceSessionPlatform &platform, NativeSourceMenuCrt &crt, GameplayRules &rules,
                               std::size_t &remaining, std::string_view name, std::span<const std::uint8_t> value)
{
    for (std::size_t index = 0; index < remaining; ++index)
    {
        const auto identity = rules.descriptor_order[index];
        if (identity >= gameplay_tables::gameplay.size())
            throw SourceBootConfigError(0x44570eu, "Gameplay descriptor identity is outside the retained source table");
        const auto &rule = gameplay_tables::gameplay[identity];
        if (rule.name != name)
            continue;
        const auto parsed = gameplay_number_4457c0(platform, crt, rules, value); // before index/scale reads
        // 44576A reads index,445773 multiplies,445779 stores directly to the
        // same world60F2D0 array. Real x87 retains the caller's current PC/RC.
        sgn_source_gameplay_multiply_store_x87(&parsed, &rule.scale_bits, &rules.values[rule.index]);
        --remaining; // 445780; remove only from the active descriptor prefix
        for (std::size_t at = index; at < remaining; ++at)
            rules.descriptor_order[at] = rules.descriptor_order[at + 1]; // 445797..4457A4
        return; // physical inactive tail is not cleared or reinitialized
    }
}

void load_gameplay_445620(RaceSessionPlatform &platform, SourceBootFilePlatform &files, NativeSourceMenuCrt &crt)
{
    auto &world = platform.state();
    auto &rules = world.gameplay;
    rules.values.fill(0); // 44562C..445638:31DWORD, before every file operation
    // The existing host table stores physical row identities with an implicit
    // terminal after30. Count reads do not restore order or remaining byte budget.
    std::size_t remaining = rules.descriptor_order.size();
    for (const auto identity : rules.descriptor_order)
        if (identity >= gameplay_tables::gameplay.size())
            throw SourceBootConfigError(0x44563au, "Source gameplay descriptor count reached an unrepresented row");
    Word length = 0;
    auto bytes = load_source_configuration_file_445ca0(world.boot_config, files, SourceConfigurationFile::gameplay,
                                                       length); // 445661
    const std::string_view text(reinterpret_cast<const char *>(bytes.get()), length);
    if (text.find('\0') != text.npos)
        throw SourceBootConfigError(0x445671u, "Embedded gameplay CSV NUL is outside the retained token-reader domain");
    std::size_t cursor = 0;
    auto name = read_retail_csv_token(text, cursor, 64); // 445671:EBP-44..-5
    while (!name.empty())
    {
        const auto value = read_retail_csv_token(text, cursor, 256); // 445686:EBP-144..-45
        if (value.empty())
            break; // 44568D skips row, frees input then clears pending
        // Use only the actual written caller-local token and its terminator.
        // The remaining uninitialized stack bytes are not exposed to4631A7.
        std::array<std::uint8_t, 256> local_value;
        std::memcpy(local_value.data(), value.data(), value.size());
        local_value[value.size()] = 0;
        skip_retail_csv_line(text, cursor); // 44568F..4456AD, before matching the name
        apply_gameplay_row_4456f0(platform, crt, rules, remaining, name,
                                  {local_value.data(), value.size() + 1u}); // 4456BC
        name = read_retail_csv_token(text, cursor, 64);                     // 4456CC
    }
    bytes.reset();     // 4456D6 source free, before pending publication
    rules.pending = 0; // 4456DE, original482F38 word, no separate host flag
}
} // namespace
void ensure_source_gameplay_configuration_4457e0(RaceSessionPlatform &platform, SourceBootFilePlatform &files,
                                                 NativeSourceMenuCrt &crt)
{
    platform.require_owner_thread();
    if (platform.state().gameplay.pending == 0)
        return;                                 // 4457E0..4457E7; zero alone skips loading
    crt.require_platform(platform);             // NEW identity guard before clear/read/store.
    load_gameplay_445620(platform, files, crt); // 4457E9 tail445620
}
} // namespace sgn::retail
