#include "sgn/source_request_search.hpp"
#include "sgn/source_462890_address.hpp"

// Pinned PE4a1d234ce1f095a427bb7f2f0174294b8c85a8fdbbc29b62a3863156b2f16a1b.
// Exact evidence:462890..46290F,464F50..464F54,464F66..46501B;
// 153 instructions/315bytes in the sealed source-search-462890 contract.
namespace sgn::retail
{
namespace
{
using source_462890::add;
using source_462890::subtract;
class Reads final
{
    const SourceRequestName &haystack_;
    const SourceApplicationLiteralRef &needle_;
    SourceSearchReadObserver *observer_;
    std::uint32_t ordinal_{};
    void observed(SourceSearchReadOwner owner, std::uint32_t site, std::uint32_t address, std::uint32_t width,
                  std::uint32_t value)
    {
        ++ordinal_;
        if (observer_)
            observer_->after_read({owner, site, address, width, value, ordinal_});
    }

  public:
    Reads(const SourceRequestName &haystack, const SourceApplicationLiteralRef &needle,
          SourceSearchReadObserver *observer)
        : haystack_(haystack), needle_(needle), observer_(observer)
    {
    }
    std::uint8_t n(std::uint32_t address, std::uint32_t site)
    {
        const auto value = needle_.read_u8_462890(address, site);
        observed(SourceSearchReadOwner::needle, site, address, 1, value);
        return value;
    }
    std::uint8_t h(std::uint32_t address, std::uint32_t site)
    {
        const auto value = haystack_.read_u8_462890(address, site);
        observed(SourceSearchReadOwner::haystack, site, address, 1, value);
        return value;
    }
    std::uint32_t word(std::uint32_t address, std::uint32_t site)
    {
        const auto value = haystack_.read_u32le_462890(address, site);
        observed(SourceSearchReadOwner::haystack, site, address, 4, value);
        return value;
    }
};
SourceRequestName single_464f66(const SourceRequestName &haystack, std::uint8_t target, Reads &reads)
{
    // 4628F7..FE passes zero-extended DL. Do not enter464F60/62 and
    // reinterpret the second pointer argument as the search character.
    auto edx = haystack.source_address_value_462890(); // 464F6C: H value
    while ((edx & 3u) != 0)
    { // 464F70/76,83/89: SOURCE address alignment
        const auto cl = reads.h(edx, 0x464f78);
        edx = add(edx, 1); // 464F7A, before both comparisons
        if (cl == target)
            return haystack.alias_value_462890(subtract(edx, 1)); // 464F50..54
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
                    return haystack.alias_value_462890(subtract(edx, 4 - byte));
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
} // namespace
SourceRequestName find_source_request_462890(SourceRequestName haystack, const SourceApplicationLiteralRef &needle,
                                             std::uint32_t needle_address, SourceSearchReadObserver *observer)
{
    Reads reads(haystack, needle, observer);
    const auto dl = reads.n(needle_address, 0x462897); // before H-value462899
    auto edi = haystack.source_address_value_462890();
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
                return haystack.alias_value_462890(subtract(edi, 1)); // 462903..09
            const auto h_even = reads.h(esi, 0x4628db);
            esi = add(esi, 2); // 4628DD executes before comparison/mismatch
            if (h_even != even)
                break; // 4628E0/E2 ->4628A8; transient ESI is no read
            const auto odd = reads.n(add(ecx, 3), 0x4628e4);
            if (odd == 0)
                return haystack.alias_value_462890(subtract(edi, 1)); // 462903..09
            const auto h_odd = reads.h(subtract(esi, 1), 0x4628eb);
            ecx = add(ecx, 2); // 4628EE before comparison
            if (h_odd != odd)
                break; // 4628F1/F3/F5 ->4628A8
        }
    }
}
} // namespace sgn::retail
