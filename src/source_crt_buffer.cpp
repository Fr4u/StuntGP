#include "sgn/source_crt_buffer.hpp"
#include <bit>
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
}
void allocate_source_crt_buffer_46b45e(SourceCrtBufferServices &m, W file)
{
    m.word(0x636818, m.word(0x636818) + 1u); // 46B45E, before allocation, including failure
    const auto allocated = m.allocate_462fcd(0x1000);
    m.word(file + 8u, allocated); // 46B475 publishes even a null allocation result
    if (allocated != 0)
    {
        m.word(file + 0xcu, m.word(file + 0xcu) | 8u);
        m.word(file + 0x18u, 0x1000);
    }
    else
    {
        m.word(file + 0xcu, m.word(file + 0xcu) | 4u);
        m.word(file + 8u, file + 0x14u); // actual inline two-byte buffer, no second allocation
        m.word(file + 0x18u, 2);
    }
    const auto base = m.word(file + 8u); // 46B498 fresh read after stores
    (void)m.word(file + 4u);             // 46B49B original AND read-modify-write
    m.word(file + 4u, 0);
    m.word(file, base);
}
W fill_source_crt_buffer_binary_468fe0(SourceCrtBufferServices &m, W file)
{
    const auto initial = m.word(file + 0xcu);
    if ((initial & 0x83u) == 0 || (initial & 0x40u) != 0)
        return 0xffffffffu;
    if ((initial & 2u) != 0)
    {
        m.word(file + 0xcu, initial | 0x20u);
        return 0xffffffffu;
    }
    const auto reading = initial | 1u;
    m.word(file + 0xcu, reading); // 469008; retained if allocation/import later fails
    if ((reading & 0x10cu) == 0)
        allocate_source_crt_buffer_46b45e(m, file);
    else
    {
        const auto base = m.word(file + 8u);
        m.word(file, base);
    }
    const auto size = m.word(file + 0x18u); // 46901F..469025 source push/read order
    const auto base = m.word(file + 8u);
    const auto descriptor = m.word(file + 0x10u);
    auto count = m.read_descriptor_binary_4690b9(descriptor, base, size);
    m.word(file + 4u, count); // 469030, including00000000 orFFFFFFFF
    if (count == 0 || count == 0xffffffffu)
    {
        const auto flag = count == 0 ? 0x10u : 0x20u; // 4690A3..4690AA
        m.word(file + 0xcu, m.word(file + 0xcu) | flag);
        (void)m.word(file + 4u);
        m.word(file + 4u, 0); // 4690B0 AND; preserve preceding access
        return 0xffffffffu;
    }
    const auto flags = m.word(file + 0xcu);
    if ((flags & 0x82u) == 0)
    {
        const auto fd = m.word(file + 0x10u);
        W record = 0x495fe0; // actual PE sentinel for fdFFFFFFFF, not a copied record
        if (fd != 0xffffffffu)
        {
            const auto group = 0x637ac0u + static_cast<W>(std::bit_cast<std::int32_t>(fd) >> 5) * 4u;
            record = m.word(group) + (fd & 31u) * 8u;
        }
        if ((m.byte(record + 4u) & 0x82u) == 0x82u)
            m.word(file + 0xcu, flags | 0x2000u);
    }
    if (m.word(file + 0x18u) == 0x200u)
    {
        const auto flags_now = m.word(file + 0xcu);
        if ((flags_now & 8u) != 0 && (flags_now & 0x400u) == 0)
            m.word(file + 0x18u, 0x1000);
    }
    const auto pointer = m.word(file);
    --count;
    m.word(file + 4u, count);            // 469095..469098 count update before byte access
    const auto result = m.byte(pointer); // zero-extended actual byte
    m.word(file, pointer + 1u);
    return result; // 46909E..4690A2
}
} // namespace sgn::retail
