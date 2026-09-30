#include "sgn/source_crt_fread.hpp"
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
bool has_buffer(SourceCrtBufferServices &m, W file)
{
    // 463750/46376C read only the low WORD of FILE flags; high bytes need not
    // be exposed merely to perform that source test. The transport is LE.
    const auto low = m.byte(file + 0xcu);
    const auto high = m.byte(file + 0xdu);
    return ((static_cast<W>(low) | (static_cast<W>(high) << 8)) & 0x10cu) != 0;
}
void copy_byte(SourceCrtBufferServices &m, W destination, W source)
{
    const auto value = m.byte(source);
    m.byte(destination, value);
}
void copy_word(SourceCrtBufferServices &m, W destination, W source)
{
    const auto value = m.word(source);
    m.word(destination, value);
}
} // namespace
W copy_source_crt_bytes_4692b0(SourceCrtBufferServices &m, W destination, W source, W count)
{
    const W original = destination;
    const W source_end = source + count; // 4692BE..CA unsigned, modulo2^32
    if (destination <= source || destination >= source_end)
    {
        // 4692D0..46937E. The three forward alignment entries copy3/2/1
        // bytes respectively. For count<4 the source jumps straight to tail.
        if ((destination & 3u) != 0 && count >= 4u)
        {
            const W prefix = 4u - (destination & 3u);
            for (W i = 0; i < prefix; ++i)
                copy_byte(m, destination + i, source + i);
            source += prefix;
            destination += prefix;
            count -= prefix;
        }
        // 4692E3 or4693AC..E0: both issue ascending DWORD load/store pairs.
        // The small-copy table enters the matching suffix of the unrolled body.
        const W words = count >> 2;
        for (W i = 0; i < words; ++i)
        {
            copy_word(m, destination, source);
            source += 4u;
            destination += 4u;
        }
        // 469408/10/1C/30:0/1/2/3 ascending byte load/store pairs.
        for (W i = 0; i < (count & 3u); ++i)
            copy_byte(m, destination + i, source + i);
    }
    else
    {
        // 469448/4C retains pointers to the final DWORD start, even for count<4.
        source += count - 4u;
        destination += count - 4u;
        if ((destination & 3u) != 0 && count >= 4u)
        {
            const W suffix = destination & 3u; // 46948B: remaining=count-suffix
            for (W i = 0; i < suffix; ++i)
                copy_byte(m, destination + 3u - i, source + 3u - i);
            source -= suffix;
            destination -= suffix;
            count -= suffix;
        }
        // 469464 STD/REP/CLD or469544..578: descending DWORD pairs.
        // Direction is represented by the addresses, not the native CPU DF.
        const W words = count >> 2;
        for (W i = 0; i < words; ++i)
        {
            copy_word(m, destination, source);
            source -= 4u;
            destination -= 4u;
        }
        // 4695A0/A8/B8/CC copies from+3 downward; unsigned pointer arithmetic
        // retains the source's short backwards path without a host underflow UB.
        for (W i = 0; i < (count & 3u); ++i)
            copy_byte(m, destination + 3u - i, source + 3u - i);
    }
    return original;
}
W read_source_crt_elements_binary_463729(SourceCrtBufferServices &m, W destination, W element_size, W element_count,
                                         W file)
{
    const W total = element_size * element_count; // 463733 IMUL keeps the low DWORD
    if (total == 0)
        return 0; // 46373C..48, no memory/provider access
    W remaining = total;
    W buffer_size = has_buffer(m, file) ? m.word(file + 0x18u) : 0x1000u;
    while (remaining != 0)
    {
        if (has_buffer(m, file))
        {
            const auto available = m.word(file + 4u);
            if (available != 0)
            {
                const W copied = remaining < available ? remaining : available; // unsigned46377B/7F
                const auto input = m.word(file);
                (void)copy_source_crt_bytes_4692b0(m, destination, input, copied);
                remaining -= copied;
                // 46378F/92 re-read each destination after copy, in case the
                // actual output overlaps FILE. Never write cached pre-copy fields.
                m.word(file + 4u, m.word(file + 4u) - copied);
                m.word(file, m.word(file) + copied);
                destination += copied;
                continue;
            }
        }
        if (remaining >= buffer_size)
        {
            const W requested = buffer_size == 0 ? remaining : remaining - remaining % buffer_size;
            const auto descriptor = m.word(file + 0x10u);
            const auto received = m.read_descriptor_binary_4690b9(descriptor, destination, requested);
            if (received == 0 || received == 0xffffffffu)
            {
                const W flag = received == 0 ? 0x10u : 0x20u;
                m.word(file + 0xcu, m.word(file + 0xcu) | flag); // 4637FB/463801
                return (total - remaining) / element_size;       // 463805..0C, partial elements retained
            }
            remaining -= received;
            destination += received; // 4637CA/CD, cached buffer_size unchanged
        }
        else
        {
            const auto value = fill_source_crt_buffer_binary_468fe0(m, file);
            if (value == 0xffffffffu)
                return (total - remaining) / element_size;
            m.byte(destination, static_cast<std::uint8_t>(value)); // 4637DD, before capacity read
            buffer_size = m.word(file + 0x18u);
            ++destination;
            --remaining;
        }
    }
    return element_count; // 4637F3: original element count, including wrapped-product cases
}
} // namespace sgn::retail
