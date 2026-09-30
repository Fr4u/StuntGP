#include "sgn/source_level_settings.hpp"
#include "sgn/source_script_search.hpp"
#include "sgn/source_script_number.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/source_level_kind_compare.hpp"
#include "sgn/source_application_literals.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_platform.hpp"
#include <array>
#include <bit>
#include <span>

#if !defined(_WIN32) || !defined(_M_X64) || defined(_M_ARM64EC)
#error The42CC20 arithmetic receiver requires native Windows AMD64.
#endif

// NEW Windows AMD64 ABI; real x87 arithmetic, live CW/PC/RC, direct owner stores.
extern "C" std::uint32_t sgn_source_level_ground_42ccec(std::uint32_t input, std::uint32_t scale_bits,
                                                        float *actual_destination);
extern "C" std::uint32_t sgn_source_level_rolling_42cecc(std::uint32_t input, std::uint32_t scale_bits,
                                                         std::uint32_t *actual_destination);

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
struct Token
{
    std::array<std::uint8_t, 128> bytes;
    std::size_t written{};
    void put(Word at, std::uint8_t value, Word pc)
    {
        if (at >= bytes.size())
            throw SourceBootConfigError(pc, "Level token exceeds caller local128");
        bytes[at] = value;
        written = static_cast<std::size_t>(at) + 1;
    }
    auto view() const
    {
        return std::span<const std::uint8_t>{bytes.data(), written};
    }
    Word length() const
    {
        for (Word at = 0;; ++at)
        {
            if (at >= written)
                throw SourceBootConfigError(0x42cc75, "strlen reaches unwritten level local");
            if (bytes[at] == 0)
                return at;
        }
    }
};
// 42C5E0, both callers pass EDX0. No NUL/TAB delimiter or pre-scan.
void token_42c5e0(RaceSessionPlatform &platform, SourceBootSearchBoundary input, Token &token)
{
    token.written = 0;
    const auto read = [&](Word offset, Word pc) {
        auto pointer = input;
        pointer.offset = offset;
        return read_source_script_byte(platform, pointer, pc);
    };
    auto ecx = input.offset;
    for (;; ++ecx)
    {
        const auto c = read(ecx, 0x42c5eb);
        if (c != ' ' && c != '\r' && c != '\n')
            break;
    }
    Word edi = 0, eax = ecx;
    auto bl = read(ecx, 0x42c600);
    if (bl != ' ')
        for (;;)
        {
            if (bl == '\r')
                break;
            bl = read(eax, 0x42c614);
            if (bl == '\n')
                break;
            token.put(edi, bl, 0x42c61b);
            bl = read(eax + 1u, 0x42c61e);
            ++edi;
            ++eax;
            if (bl == ' ')
                break;
        }
    token.put(edi, 0, 0x42c62d);
}
void admit(Word result, Word pc)
{
    if (result != 0)
        throw SourceBootConfigError(pc, "Level arithmetic requires masked x87, valid precision and empty TOP0 stack");
}
} // namespace
void configure_source_level_42cc20(RaceSessionPlatform &platform, SourceBootSearchBoundary input)
{
    platform.require_owner_thread();
    auto &state = platform.state();
    auto &level = state.level;
    SourceBootSearchBoundary cursor;
    Token token;   // same caller local128 for both token operations.
    Word number{}; // borrowed caller word, never a persistent parsed-level owner.
    const auto key = [&](Word address) {
        return find_source_script_key_42c580(platform, input, {{}, address, true}, &cursor) == 1;
    };
    const auto parse = [&](bool advance = false) {
        parse_source_number_42c6a0(platform, cursor, number, advance ? &cursor : nullptr);
    };
    if (key(0x47788c))
    {
        token_42c5e0(platform, cursor, token); // 42CC51
        format_level_track_texture_42cc62(platform, token.view());
        number = token.length();
        if (number > 2)
            level.texture_suffix = parse_source_local_integer_4631f4(platform, token.view(), number - 2u);
    }
    else
        level.track_texture[0] = 0; // 42CC98, suffix remains unchanged.
    if (key(0x477898))
    {
        parse();
        level.music_track = number;
    } // 42CCC4
    if (key(0x4778a4))
    {
        Word ground;
        parse_source_number_42c6a0(platform, cursor, ground, nullptr);
        const auto scale = platform.source_application_literals().read_level_scale_46e2f0(0x42ccef);
        admit(sgn_source_level_ground_42ccec(ground, scale, &level.ground_level), 0x42ccec);
    }
    else
        level.ground_level = std::bit_cast<float>(Word{0xc4fa0000});
    if (key(0x4778b0))
    {
        token_42c5e0(platform, cursor, token); // 42CD27
        level.game_type = 2;                   // 42CD32 BEFORE any comparison.
        if (compare_source_level_kind_46dd90(platform, token.view(), 0x4778bc) == 0)
            level.game_type = 2;
        else if (compare_source_level_kind_46dd90(platform, token.view(), 0x4778c8) == 0)
            level.game_type = 3;
        else if (compare_source_level_kind_46dd90(platform, token.view(), 0x4778dc) == 0)
            level.game_type = 4;
    }
    state.clock.remaining_ticks = 0; // 42CDA4 BEFORE key lookup.
    if (key(0x4778f0))
    {
        parse();
        state.clock.remaining_ticks = number * 60u;
    }
    if (key(0x4778f8))
    {
        parse();
        if (number > 24u)
            number = 24u;
        state.clock.lap_count = number; // unsigned clamp,42CE06.
    }
    if (key(0x477904))
    {
        parse(true);
        Word esi = number * 3600u;
        parse(true);
        esi += (number % 100u) * 60u;
        parse(true);
        const auto hundredths = ((number % 100u) * 60u) / 100u;
        state.records.lap_ticks = esi + hundredths; // 42CE91 after all three cursor scans.
    }
    level.rolling_start_offset = 60000; // 42CEA1 BEFORE key lookup.
    if (key(0x477910))
    {
        parse();
        const auto scale = platform.source_application_literals().read_level_scale_46e2f0(0x42cecf);
        admit(sgn_source_level_rolling_42cecc(number, scale, &level.rolling_start_offset), 0x42cecc);
    }
    level.overlay_demo = 0; // 42CEEC
    if (key(0x477920))
    {
        parse();
        level.overlay_demo = number;
    }
    level.camera_mode = 1; // 42CF1C
    if (key(0x47792c))
    {
        parse();
        level.camera_mode = number;
    }
    level.demo_loop_ticks = 3600; // 42CF51
    if (key(0x477938))
    {
        parse();
        if (number == 0)
        {
            level.demo_loop_ticks = 0xffffffffu;
            return;
        } // 42CF7F..88
        level.demo_loop_ticks = number * 60u; // 42CF92
    }
}
} // namespace sgn::retail
