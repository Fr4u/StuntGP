#include "sgn/source_crt_file_flush.hpp"
#include <bit>
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
}
W flush_source_crt_buffer_46659f(SourceCrtFileFlushServices &m, W f)
{
    W result = 0;
    const auto flags = m.read_word(f + 0xc, 0x4665a8);
    if ((flags & 3u) == 2u && (flags & 0x108u) != 0)
    {
        const auto base = m.read_word(f + 8u, 0x4665bb);
        const auto length = m.read_word(f, 0x4665be) - base;
        if (std::bit_cast<std::int32_t>(length) > 0)
        {
            const auto fd = m.read_word(f + 0x10u, 0x4665c8);
            if (m.write_descriptor_4695e5(fd, base, length) == length)
            {
                const auto current = m.read_word(f + 0xcu, 0x4665d7);
                if ((current & 0x80u) != 0)
                    m.write_word(f + 0xcu, current & ~2u, 0x4665e0);
            }
            else
            {
                m.write_word(f + 0xcu, m.read_word(f + 0xcu, 0x4665e5) | 0x20u, 0x4665e5);
                result = 0xffffffffu;
            }
        }
    }
    const auto base = m.read_word(f + 8u, 0x4665ec);
    (void)m.read_word(f + 4u, 0x4665ef); // Original AND read-modify-write access.
    m.write_word(f + 4u, 0, 0x4665ef);
    m.write_word(f, base, 0x4665f3);
    return result;
}
std::uint32_t flush_source_crt_file_466564(SourceCrtFileFlushServices &m, W f)
{
    if (f == 0)
        return flush_source_crt_streams_466604(m, 0);
    if (flush_source_crt_buffer_46659f(m, f) != 0)
        return 0xffffffffu;
    if ((m.read_word(f + 0xcu, 0x466586) & 0x4000u) != 0)
        return m.commit_descriptor_46b968(m.read_word(f + 0x10u, 0x46658c)) != 0 ? 0xffffffffu : 0u;
    return 0;
}
std::uint32_t flush_source_crt_streams_466604(SourceCrtFileFlushServices &m, W mode)
{
    W index = 0, count = 0, result = 0;
    if (std::bit_cast<std::int32_t>(m.read_word(0x637aa0, 0x46660d)) > 0)
    {
        do
        {
            const auto table = m.read_word(0x636a88, 0x466615);
            const auto f = m.read_word(table + index * 4u, 0x46661a);
            if (f != 0)
            {
                const auto flags = m.read_word(f + 0xcu, 0x466621);
                if ((flags & 0x83u) != 0)
                {
                    if (mode == 1)
                    {
                        if (flush_source_crt_file_466564(m, f) != 0xffffffffu)
                            ++count;
                    }
                    else if (mode == 0 && (flags & 2u) != 0 && flush_source_crt_file_466564(m, f) == 0xffffffffu)
                        result = 0xffffffffu;
                }
            }
            ++index;
        } while (std::bit_cast<std::int32_t>(index) < std::bit_cast<std::int32_t>(m.read_word(0x637aa0, 0x46665a)));
    }
    return mode == 1 ? count : result;
}
} // namespace sgn::retail
