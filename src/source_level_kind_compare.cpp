#include "sgn/source_level_kind_compare.hpp"
namespace sgn::retail
{
namespace
{
std::uint8_t fold_46ddba(std::uint8_t byte) noexcept
{
    // SUB/CMP/SBB/AND/ADD on8-bit operands: fold exactly ASCII A..Z.
    const auto displaced = static_cast<std::uint8_t>(byte - 0x41u);
    const auto add = displaced < 0x1au ? 0x20u : 0u;
    return static_cast<std::uint8_t>(displaced + add + 0x41u);
}
} // namespace
std::int32_t compare_source_level_kind_46dd90(const SourceApplicationLiteralRef &app,
                                              std::span<const std::uint8_t> local, std::uint32_t literal)
{
    if (local.size() > 128)
        throw std::invalid_argument("level-kind input belongs to the128-byte caller local");
    const bool non_c = app.read_compare_locale_46dda2() != 0; // Before either input BYTE read.
    std::uint32_t local_cursor = 0;
    std::uint8_t previous = 0xff; // 46DDA8 or46DDE3.
    while (previous != 0)
    {
        auto right = app.read_level_kind_byte_46dd90(literal, non_c ? 0x46ddf0u : 0x46ddb0u);
        ++literal; // 46DDB2/46DDF2, before first-input read.
        const auto local_site = non_c ? 0x46ddf3u : 0x46ddb3u;
        if (local_cursor >= local.size())
            throw SourceLiteralReadError(local_site, local_cursor,
                                         "unwritten caller-stack comparison byte; address is relative offset");
        auto left = local[local_cursor];
        ++local_cursor; // 46DDB5/46DDF5.
        previous = right;
        if (left == right)
            continue; // Direct equality includes terminating NUL.
        if (non_c)
            throw SourceLiteralReadError(0x46ddfc, left,
                                         "non-C locale mismatch requires unreconstructed46AA6C/NLS lowercase child");
        right = fold_46ddba(right); // 46DDBA..46DDC5, second argument first.
        left = fold_46ddba(left);   // 46DDC7 exchange,46DDC9..46DDD4.
        previous = left;
        if (left == right)
            continue;
        // 46DDDA/46DDDC produce AL FF or01,46DDDE sign extends.
        return left < right ? -1 : 1;
    }
    return 0;
}
} // namespace sgn::retail
