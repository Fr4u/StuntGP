#include "sgn/source_score_configuration.hpp"
#include "sgn/source_configuration_file.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_csv.hpp"
#include "sgn/retail_gameplay_tables.hpp"
#include "sgn/source_boot_config.hpp"
#include "sgn/source_script_number.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <string>

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;

Word score_integer_443d50(RaceSessionPlatform &platform, ScoreRules &rules, std::span<const std::uint8_t> written_value)
{
    rules.remaining_bytes -= 4u; // 443D50..443D58, source DWORD arithmetic
    if (std::bit_cast<std::int32_t>(rules.remaining_bytes) < 0)
        return 0;                                                         // 443D5D JNS
    return parse_source_local_integer_4631f4(platform, written_value, 0); // 443D63
}

void apply_score_row_443ca0(RaceSessionPlatform &platform, ScoreRules &rules, std::size_t &remaining,
                            std::string_view name, std::span<const std::uint8_t> value)
{
    for (std::size_t index = 0; index < remaining; ++index)
    {
        const auto identity = rules.descriptor_order[index];
        if (identity >= gameplay_tables::scores.size())
            throw SourceBootConfigError(0x443cc4u, "Score descriptor identity is outside the retained source table");
        const auto &rule = gameplay_tables::scores[identity];
        if (rule.name != name)
            continue;
        const Word parsed = score_integer_443d50(platform, rules, value); // before destination read/store
        rules.values[rule.index] = parsed;                                // 443D19, same world60D688 array
        --remaining; // 443D20; remove only from the active descriptor prefix
        for (std::size_t at = index; at < remaining; ++at)
            rules.descriptor_order[at] = rules.descriptor_order[at + 1]; // 443D32..443D41
        return; // physical inactive tail is not cleared or reinitialized
    }
}

void load_scores_443bd0(RaceSessionPlatform &platform, SourceBootFilePlatform &files)
{
    auto &world = platform.state();
    auto &rules = world.score_rules;
    rules.values.fill(0); // 443BDC..443BE8:37DWORD, before every file operation
    // The existing host table stores physical row identities with an implicit
    // terminal after36. Count reads do not restore order or remaining byte budget.
    std::size_t remaining = rules.descriptor_order.size();
    for (const auto identity : rules.descriptor_order)
        if (identity >= gameplay_tables::scores.size())
            throw SourceBootConfigError(0x443beau, "Source score descriptor count reached an unrepresented row");
    Word length = 0;
    auto bytes = load_source_configuration_file_445ca0(world.boot_config, files, SourceConfigurationFile::scores,
                                                       length); // 443C11
    const std::string_view text(reinterpret_cast<const char *>(bytes.get()), length);
    if (text.find('\0') != text.npos)
        throw SourceBootConfigError(0x443c21u, "Embedded score CSV NUL is outside the retained token-reader domain");
    std::size_t cursor = 0;
    auto name = read_retail_csv_token(text, cursor, 64); // 443C21:EBP-44..-5
    while (!name.empty())
    {
        const auto value = read_retail_csv_token(text, cursor, 256); // 443C36:EBP-144..-45
        if (value.empty())
            break; // 443C3D skips row, frees input then clears pending
        // Use only the actual written caller-local token and its terminator.
        // The remaining uninitialized stack bytes are not exposed to4631F4.
        std::array<std::uint8_t, 256> local_value;
        std::memcpy(local_value.data(), value.data(), value.size());
        local_value[value.size()] = 0;
        skip_retail_csv_line(text, cursor); // 443C3F..443C5D, before matching the name
        apply_score_row_443ca0(platform, rules, remaining, name, {local_value.data(), value.size() + 1u}); // 443C6C
        name = read_retail_csv_token(text, cursor, 64);                                                    // 443C7C
    }
    bytes.reset();     // 443C86 source free, before pending publication
    rules.pending = 0; // 443C8E, original482718 word, no separate host flag
}
} // namespace
void ensure_source_score_configuration_443d70(RaceSessionPlatform &platform, SourceBootFilePlatform &files)
{
    platform.require_owner_thread();
    if (platform.state().score_rules.pending == 0)
        return;                          // 443D70..443D77; zero alone skips loading
    load_scores_443bd0(platform, files); // 443D79 tail443BD0
}
} // namespace sgn::retail
