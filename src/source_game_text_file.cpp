#include "sgn/retail_game_text.hpp"
#include "sgn/source_boot_config.hpp"
#include "game_text_detail.hpp"
#include <algorithm>
#include <bit>
#include <cstdlib>
#include <string>

namespace sgn::retail
{
void load_game_text(SourceBootConfigState &config, SourceBootFilePlatform &files, std::uint32_t language,
                    GameTextState &state)
{
    game_text_detail::begin_game_text_load(state); // 445D8C then445D94, before every resource operation
    const auto end = std::find(config.base_path.begin(), config.base_path.end(), '\0');
    if (end == config.base_path.end())
        throw FormatError("445CBC game text base path has no bounded terminator");
    // Literal sprintf("%s\\%s",48489C,"Config\\gametext.csv"). Do not
    // normalize the double slash produced by the existing trailing base slash.
    const std::string path = std::string(config.base_path.begin(), end) + "\\Config\\gametext.csv";
    const auto length = source_boot_file_size_44b3b0(config, files, path); // 445CCA
    if (std::bit_cast<std::int32_t>(length) <= 0)                          // 445CD1/445CD3 -> null resource
        throw FormatError("445B95 game text source buffer is null after nonpositive source size");
    // Explicit existing1MiB CSV domain, admitted before potentially huge malloc.
    // Not a source size clamp, truncated read, successful fallback or arena reset.
    if (length > 1024u * 1024u)
        throw FormatError("445CE1 game text source allocation exceeds bounded CSV domain");
    SourceBootRawAllocation bytes(
        static_cast<std::uint8_t *>(std::malloc(static_cast<std::size_t>(length) + 1u))); // 445CE1
    if (!bytes)
        throw FormatError("445CF4 game text source read would receive a null malloc result");
    const auto read_result = source_boot_file_read_44b260(config, files, path, length, {bytes.get(), length}); // 445CF4
    bytes.get()[length] = 0; // 445CF9: original terminator at requested size, not fread count
    // 44B260 returns member size on WAD success, actual count on loose reads.
    // With positive requested length, min(length,result) is the initialized prefix.
    // The existing parser cannot observe unwritten malloc bytes. Stop BEFORE its
    // entry445DCA; do not replace the unwritten tail with zero or an invented EOF.
    if (std::min(length, read_result) < length)
        throw FormatError("445DCA game text parser would receive an incompletely initialized source buffer");
    const std::string_view csv(reinterpret_cast<const char *>(bytes.get()), length);
    game_text_detail::validate_game_text_input(csv, language);
    game_text_detail::load_prepared_game_text(csv, language, state);
    bytes.reset();             // 445E3E: source free BEFORE current-language publication
    state.language = language; // 445E46
}
} // namespace sgn::retail
