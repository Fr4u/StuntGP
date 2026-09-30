#include "sgn/source_script_search.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/source_application_literals.hpp"
#include <cstring>
#include <bit>

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
constexpr Word add(Word a, Word b) noexcept
{
    return a + b;
}
constexpr Word subtract(Word a, Word b) noexcept
{
    return a - b;
}
struct Pointer
{
    SourceBootSearchBoundary value;
    Word offset() const noexcept
    {
        return value.is_null ? 0 : value.offset;
    }
    Pointer alias(Word offset) const
    {
        return {{value.allocation, offset, value.is_null}};
    }
};
class Reads
{
    RaceSessionPlatform &platform_;
    Pointer haystack_;
    SourceScriptNeedle needle_;

  public:
    Reads(RaceSessionPlatform &p, Pointer h, SourceScriptNeedle n) : platform_(p), haystack_(h), needle_(n) {}
    std::uint8_t n(Word relative, Word site) const
    {
        if (needle_.from_app)
            return platform_.source_application_literals().read_level_script_byte(needle_.app_address + relative, site);
        if (relative >= needle_.local.size())
            throw SourceBootConfigError(site, "Reached needle byte outside its actual owner");
        return static_cast<std::uint8_t>(needle_.local[relative]);
    }
    std::span<const std::uint8_t> borrow(Word offset, Word width, Word site) const
    {
        if (haystack_.value.is_null)
            throw SourceBootConfigError(site, "Reached null script read");
        try
        {
            const auto bytes = platform_.source_allocations().bytes(haystack_.value.allocation);
            if (offset > bytes.size() || width > bytes.size() - offset)
                throw SourceBootConfigError(site, "Reached script read outside live bank payload");
            return bytes.subspan(offset, width);
        }
        catch (const SourceAllocationError &error)
        {
            throw SourceBootConfigError(site, error.what());
        }
    }
    std::uint8_t h(Word offset, Word site) const
    {
        return borrow(offset, 1, site)[0];
    }
    Word word(Word offset, Word site) const
    {
        const auto bytes = borrow(offset, 4, site);
        Word value;
        std::memcpy(&value, bytes.data(), 4);
        if constexpr (std::endian::native == std::endian::big)
            value = std::byteswap(value);
        return value;
    }
};
// 42E8D0 aligns payloads to32 bytes. Thus source offset modulo4 supplies
// the exact alignment branch without depending on an arbitrary host address.
// One DWORD borrow/read at464F96 and a separate real reread at464FD8.
Pointer single_464f66(const Pointer &haystack, std::uint8_t target, Reads &reads)
{
    // 4628F7..FE passes zero-extended DL. Do not enter464F60/62 and
    // reinterpret the second pointer argument as the search character.
    auto edx = haystack.offset(); // 464F6C: H value
    while ((edx & 3u) != 0)
    { // 464F70/76,83/89: SOURCE address alignment
        const auto cl = reads.h(edx, 0x464f78);
        edx = add(edx, 1); // 464F7A, before both comparisons
        if (cl == target)
            return haystack.alias(subtract(edx, 1)); // 464F50..54
        if (cl == 0)
            return {}; // 464FD4..D7
    }
    std::uint32_t repeated = target; // 464F8B..94
    repeated |= repeated << 8;
    repeated |= repeated << 16;
    for (;;)
    {
        const auto w = reads.word(edx, 0x464f96);
        const auto x = w ^ repeated;
        const auto sum_w = add(w, 0x7efefeffu);
        const auto candidate = ((~x) ^ add(x, 0x7efefeffu)) & 0x81010100u;
        const auto zero_candidate = ((~w) ^ sum_w) & 0x81010100u;
        edx = add(edx, 4); // 464FB1, before mask branches and candidate reread
        if (candidate != 0)
        { // 464FB4/BA has priority over the zero mask
            const auto reread = reads.word(subtract(edx, 4), 0x464fd8);
            for (std::uint32_t byte = 0; byte < 4; ++byte)
            { // 464FDB..464FFC
                const auto value = static_cast<std::uint8_t>((reread >> (byte * 8)) & 0xffu);
                // Target comparison precedes NUL at every byte. These offsets
                // correspond to465015,46500E,465007,465000 returns in order.
                if (value == target)
                    return haystack.alias(subtract(edx, 4 - byte));
                if (value == 0)
                    return {}; // 464FD2..D7
            }
            continue; // 464FFE: candidate mask can have a false positive
        }
        if (zero_candidate == 0)
            continue; // 464FBC/C1
        if ((zero_candidate & 0x01010100u) != 0)
            return {}; // 464FC3/C8 ->FD2..D7
        if ((sum_w & 0x80000000u) != 0)
            continue; // 464FCA/D0
        return {};    // 464FD2..D7
    }
}
Pointer search(Pointer haystack, Reads &reads)
{
    constexpr Word needle_address = 0;                 // relative value; actual needle owner is retained by Reads.
    const auto dl = reads.n(needle_address, 0x462897); // before H-value462899
    auto edi = haystack.offset();
    if (dl == 0)
        return haystack; // 46290A..0F: no liveness/range/character check of H
    const auto dh = reads.n(add(needle_address, 1), 0x4628a1);
    if (dh == 0)
        return single_464f66(haystack, dl, reads); // 4628F7..FE
    // DL and DH are cached for the whole multi-character path. Only the
    // suffix reads N again; each retry restores the original N pointer value.
    for (;;)
    { // 4628A8 retry: EDI initially H, then previous candidate+1
        auto esi = edi;
        auto ecx = needle_address; // 4628AA stack pointer VALUE, no byte read
        auto al = reads.h(edi, 0x4628ae);
        esi = add(esi, 1); // 4628B0
        for (;;)
        {
            while (al != dl)
            { // 4628B1/B3 or4628BC/BE, same loaded AL
                if (al == 0)
                    return {}; // 4628B5/B7 or4628C0/C2 ->C4..C9
                al = reads.h(esi, 0x4628b9);
                esi = add(esi, 1); // 4628BB
            }
            al = reads.h(esi, 0x4628ca);
            esi = add(esi, 1); // 4628CC
            if (al == dh)
                break; // 4628CD/CF
            // Mismatch jumps to4628BC: the SAME AL becomes the next first
            // character candidate. Never reread it as another4628B9 load.
        }
        edi = subtract(esi, 1); // 4628D1: candidate+1; result is EDI-1
        for (;;)
        {
            const auto even = reads.n(add(ecx, 2), 0x4628d4);
            if (even == 0)
                return haystack.alias(subtract(edi, 1)); // 462903..09
            const auto h_even = reads.h(esi, 0x4628db);
            esi = add(esi, 2); // 4628DD executes before comparison/mismatch
            if (h_even != even)
                break; // 4628E0/E2 ->4628A8; transient ESI is no read
            const auto odd = reads.n(add(ecx, 3), 0x4628e4);
            if (odd == 0)
                return haystack.alias(subtract(edi, 1)); // 462903..09
            const auto h_odd = reads.h(subtract(esi, 1), 0x4628eb);
            ecx = add(ecx, 2); // 4628EE before comparison
            if (h_odd != odd)
                break; // 4628F1/F3/F5 ->4628A8
        }
    }
}
} // namespace
std::uint8_t read_source_script_byte(RaceSessionPlatform &platform, SourceBootSearchBoundary input, std::uint32_t site)
{
    platform.require_owner_thread();
    Reads reads(platform, Pointer{input}, {});
    return reads.h(input.offset, site);
}
SourceBootSearchBoundary find_source_script_462890(RaceSessionPlatform &platform, SourceBootSearchBoundary input,
                                                   SourceScriptNeedle needle)
{
    platform.require_owner_thread();
    Pointer source{input};
    Reads reads(platform, source, needle);
    return search(source, reads).value;
}
std::uint32_t find_source_script_key_42c580(RaceSessionPlatform &platform, SourceBootSearchBoundary input,
                                            SourceScriptNeedle needle, SourceBootSearchBoundary *cursor)
{
    auto found = find_source_script_462890(platform, input, needle); // 42C58B
    const auto &boundary = platform.state().boot_config.boundary;    // 42C590, AFTER search.
    if (!boundary.is_null)
    {
        if (found.is_null)
            return 0;
        if (boundary.allocation != found.allocation)
            throw SourceBootConfigError(0x42c59d, "Source pointer ordering crosses allocation identities");
        if (found.offset > boundary.offset)
            return 0;
    }
    found = find_source_script_462890(platform, found, {{}, 0x4777f0, true}); // 42C5B2
    if (cursor)
    {
        Reads reads(platform, Pointer{found}, {});
        auto value = found.offset;
        if (reads.h(value, 0x42c5be) != ' ')
        {
            for (;;)
            {
                const auto c = reads.h(value + 1, 0x42c5c3); // read precedes INC.
                ++value;
                if (c == ' ')
                    break;
            }
        }
        found.offset = value;
        *cursor = found; // 42C5CC, only reached after actual space.
    }
    return 1; // 42C5CE, including null second result when EDX slot is null.
}
std::uint32_t select_source_level_key_42d0ee(RaceSessionPlatform &platform, SourceBootSearchBoundary input,
                                             SourceBootSearchBoundary &cursor)
{
    const auto &key = platform.state().boot_config.level_script;
    return find_source_script_key_42c580(platform, input, {std::span<const char>{key}, 0, false}, &cursor);
}
SourceBootSearchBoundary find_source_level_end_42d0fc(RaceSessionPlatform &platform, SourceBootSearchBoundary input)
{
    return find_source_script_462890(platform, input, {{}, 0x477990, true});
}
} // namespace sgn::retail
