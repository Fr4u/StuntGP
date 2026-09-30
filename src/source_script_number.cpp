#include "sgn/source_script_number.hpp"
#include "sgn/source_script_search.hpp"
#include "sgn/source_application_literals.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
// Only the BYTE transport differs between the real bank and caller-local
// entry. The instruction order and canonical CRT owner are shared.
template <class Read> Word integer_4631f4(const SourceApplicationLiteralRef &app, Word cursor, Read &&read)
{
    for (;;)
    {
        const bool multibyte = app.number_uses_multibyte_4631f4(8); // 4631FC, before input BYTE.
        const auto character = read(cursor, multibyte ? 0x463205u : 0x463214u);
        if (app.classify_number_4631f4(character, 8, multibyte) == 0)
            break;
        ++cursor; // 463227, only on nonzero classification.
    }
    auto character = read(cursor, 0x46322a); // Actual reread after whitespace loop.
    ++cursor;                                // 46322D.
    const auto sign = character;             // 463231.
    if (character == 0x2du || character == 0x2bu)
    {
        character = read(cursor, 0x46323a);
        ++cursor; // 46323D, before digit classification.
    }
    Word value = 0; // 46323E.
    for (;;)
    {
        const bool multibyte = app.number_uses_multibyte_4631f4(4); // 463240 every iteration.
        if (app.classify_number_4631f4(character, 4, multibyte) == 0)
            break;
        value = value * 10u + static_cast<Word>(character) - 0x30u; // 463264/463267, modulo2^32.
        character = read(cursor, 0x46326b);
        ++cursor; // 46326E, also after the last digit read.
    }
    return sign == 0x2du ? 0u - value : value; // 463271..463278. No signed-overflow UB.
}
} // namespace
void parse_source_number_42c6a0(RaceSessionPlatform &platform, SourceBootSearchBoundary input, Word &actual_destination,
                                SourceBootSearchBoundary *optional_cursor)
{
    platform.require_owner_thread();
    const auto app = platform.source_application_literals();
    // Each read resolves the actual same bank reference. No retained payload
    // snapshot; output can change memory observed by the later cursor scan.
    const auto read = [&](Word offset, Word site) {
        auto at = input;
        at.offset = offset;
        return read_source_script_byte(platform, at, site);
    };
    auto start = input.offset; // ESI receives the caller source pointer.
    for (;;)
    {
        const auto character = read(start, 0x42c6a9);
        if (character != 0x20u && character != 0x0du && character != 0x0au)
            break;
        ++start; // 42C6B7. TAB is NOT in this outer trim.
    }
    const auto value = integer_4631f4(app, start, read); // 42C6BB.
    actual_destination = value;                          // 42C6C8, even with no digits or negative/wrapped value.
    if (optional_cursor == nullptr)
        return; // 42C6CA, no following source access.
    auto character = read(start, 0x42c6cc);
    Word length = 0; // 42C6CE.
    while (character != 0x20u)
    {
        character = read(start + length + 1u, 0x42c6d5);
        ++length; // 42C6D9; only literal space ends this scan, not NUL/CR/LF/TAB.
    }
    auto next = input;
    next.offset = start + length + 1u; // 42C6DF.
    *optional_cursor = next;           // 42C6E3, AFTER the destination write and entire scan.
}
Word parse_source_local_integer_4631f4(RaceSessionPlatform &platform, std::span<const std::uint8_t> written_local,
                                       Word offset)
{
    platform.require_owner_thread();
    const auto app = platform.source_application_literals();
    return integer_4631f4(app, offset, [&](Word at, Word site) {
        if (at >= written_local.size())
            throw SourceBootConfigError(site, "Integer read reaches unwritten caller-local byte (relative offset)");
        return written_local[at];
    });
}
Word parse_source_bank_integer_4631f4(RaceSessionPlatform &platform, SourceBootSearchBoundary input)
{
    platform.require_owner_thread();
    return integer_4631f4(platform.source_application_literals(), input.offset, [&](Word offset, Word site) {
        auto at = input;
        at.offset = offset;
        return read_source_script_byte(platform, at, site);
    });
}

} // namespace sgn::retail
