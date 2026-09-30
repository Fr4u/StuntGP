#include "sgn/source_rolling_demo.hpp"
#include "sgn/source_script_search.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/source_application_literals.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_platform.hpp"
#include <array>
#include <span>

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
bool less_pointer(SourceBootSearchBoundary a, SourceBootSearchBoundary b, Word site)
{
    if (b.is_null)
        return false; // unsigned nonnegative pointer cannot be below0.
    if (a.is_null)
        return true;
    if (a.allocation != b.allocation)
        throw SourceBootConfigError(site, "Rolling-demo pointer ordering crosses allocation identities");
    return a.offset < b.offset;
}
struct Token
{
    std::array<std::uint8_t, 64> bytes;
    std::size_t initialized{};
    void write(Word at, std::uint8_t value, Word site)
    {
        if (at >= bytes.size())
            throw SourceBootConfigError(site, "Rolling-demo token write exceeds actual local64");
        bytes[at] = value;
        initialized = static_cast<std::size_t>(at) + 1;
    }
    Word length_42d032() const
    {
        Word size = 0;
        for (;; ++size)
        {
            if (size >= initialized)
                throw SourceBootConfigError(0x42d032, "strlen reaches unwritten rolling-demo local");
            if (bytes[size] == 0)
                return size;
        }
    }
};
void token_42c5e0(RaceSessionPlatform &platform, SourceBootSearchBoundary source, Token &output)
{
    const auto read = [&](Word offset, Word site) {
        auto pointer = source;
        pointer.offset = offset;
        return read_source_script_byte(platform, pointer, site);
    };
    auto ecx = source.offset;
    for (;; ++ecx)
    {
        const auto value = read(ecx, 0x42c5eb);
        if (value != ' ' && value != '\r' && value != '\n')
            break;
    }
    Word edi = 0, eax = ecx;
    auto bl = read(ecx, 0x42c600);
    if (bl != ' ')
    {
        for (;;)
        {
            if (bl == '\r')
                break; // 42C60F
            bl = read(eax, 0x42c614);
            if (bl == '\n')
                break;
            output.write(edi, bl, 0x42c61b);
            bl = read(eax + 1, 0x42c61e); // read BEFORE incrementing EDI/EAX.
            ++edi;
            ++eax;
            if (bl == ' ')
                break;
        }
    }
    output.write(edi, 0, 0x42c62d);
    // Caller42D00A supplies EDX0: no42C638 cursor publication.
    // NUL and TAB are copied as bytes; only space/CR/LF delimit the token.
}
} // namespace
void load_source_rolling_demo_42cfa0(RaceSessionPlatform &platform, SourceBootSearchBoundary input)
{
    platform.require_owner_thread();
    auto &config = platform.state().boot_config;
    SourceBootSearchBoundary cursor; // host value storage; source local starts unwritten.
    const auto selected = find_source_script_key_42c580(platform, input, {{}, 0x477944, true}, &cursor); // 42CFB6
    if (selected != 1)
        throw SourceBootConfigError(0x42cfbb, "Rolling-demo reads unwritten initial local cursor");
    const auto end = find_source_script_462890(platform, cursor, {{}, 0x477958, true}); // 42CFC4
    const auto app = platform.source_application_literals();
    {
        auto &state = app.rolling_demo_storage();
        for (auto &pointer : state.name_offsets)
            pointer.reset(); // 42CFD7: sixteen DWORD-null stores.
        // Names are intentionally not cleared, including when no list is selected.
    }
    const bool enter = less_pointer(cursor, end, 0x42cfe1);
    config.boundary = end; // 42CFE3 AFTER table clear and comparison, before branch.
    if (!enter)
        return; // 42CFE9 ->42D04E..42D054.
    Word attempts = 0, published = 0, append = 0;
    auto search_input = cursor; // 42CFD9 supplied ECX for the first DEMOSCRIPT search.
    Token token;                // one actual local64 reused across iterations, never pre-zeroed.
    for (;;)
    {
        if (attempts >= 16)
            return; // 42CFF0/3 limits ATTEMPTS, not successful entries.
        if (find_source_script_key_42c580(platform, search_input, {{}, 0x477960, true}, &cursor) == 1)
        {
            token.initialized = 0;                 // NEW observation only, no local byte clearing.
            token_42c5e0(platform, cursor, token); // 42D010, same bank, EDX0.
            format_rolling_demo_name_42d01a(
                platform, std::span<const std::uint8_t>{token.bytes.data(), token.initialized}, append);
            {
                auto &state = app.rolling_demo_storage();
                state.name_offsets[published] = append; // 42D01F, AFTER format returns.
            }
            ++published;                          // 42D021, separate from attempt counter.
            append += token.length_42d032() + 1u; // 42D03A: LOCAL format length, not formatter count.
        }
        ++attempts; // 42D041, also after a rejected key with unchanged cursor.
        search_input = cursor;
        search_input.offset += 8u; // 42D042: next search starts at cursor+8, NOT token end/root.
        if (!less_pointer(search_input, config.boundary, 0x42d04a))
            return;
    }
}
} // namespace sgn::retail
