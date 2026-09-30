#include "sgn/source_numeric_control.hpp"

namespace sgn::retail
{
std::uint32_t decode_source_x87_control_46308c(std::uint32_t raw) noexcept
{
    std::uint32_t flags = 0;
    if (raw & 0x01u)
        flags |= 0x10u;
    if (raw & 0x04u)
        flags |= 0x08u;
    if (raw & 0x08u)
        flags |= 0x04u;
    if (raw & 0x10u)
        flags |= 0x02u;
    if (raw & 0x20u)
        flags |= 0x01u;
    if (raw & 0x02u)
        flags |= 0x80000u;
    flags |= (raw & 0xc00u) >> 2;
    const auto precision = raw & 0x300u;
    if (precision == 0)
        flags |= 0x20000u;
    else if (precision == 0x200u)
        flags |= 0x10000u;
    // Both raw01(reserved) and11(extended precision) map to software00.
    if (raw & 0x1000u)
        flags |= 0x40000u;
    return flags;
}
std::uint16_t encode_source_x87_control_46311e(std::uint32_t flags) noexcept
{
    std::uint32_t raw = 0;
    if (flags & 0x10u)
        raw |= 0x01u;
    if (flags & 0x08u)
        raw |= 0x04u;
    if (flags & 0x04u)
        raw |= 0x08u;
    if (flags & 0x02u)
        raw |= 0x10u;
    if (flags & 0x01u)
        raw |= 0x20u;
    if (flags & 0x80000u)
        raw |= 0x02u;
    raw |= (flags & 0x300u) << 2;
    const auto precision = flags & 0x30000u;
    if (precision == 0)
        raw |= 0x300u;
    else if (precision == 0x10000u)
        raw |= 0x200u;
    // Both software10(single) and11(reserved) leave the native precision00.
    if (flags & 0x40000u)
        raw |= 0x1000u;
    return static_cast<std::uint16_t>(raw);
}
std::uint32_t update_source_x87_control_463041(SourceX87Control &device, std::uint32_t value, std::uint32_t mask)
{
    const auto previous = decode_source_x87_control_46308c(device.fstcw());
    const auto merged = (previous & ~mask) | (value & mask);
    device.fldcw(encode_source_x87_control_46311e(merged));
    return merged;
}
void prepare_source_numeric_429510(SourceX87Control &device)
{
    (void)update_source_x87_control_463041(device, 0x10000u, 0x30000u);
    (void)update_source_x87_control_463041(device, 0u, 0x300u);
}
std::uint32_t source_compatible_mxcsr(std::uint32_t prior, std::uint16_t control) noexcept
{
    // Replace only DAZ6, masks7..12, rounding13..14 and FTZ15. Keep status0..5
    // and all higher bits as observed, never manufacture reserved set bits.
    return (prior & ~0xffc0u) | ((std::uint32_t{control} & 0x3fu) << 7) | ((std::uint32_t{control} & 0xc00u) << 3);
}
} // namespace sgn::retail
