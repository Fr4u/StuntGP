#include "sgn/source_crt_stream_acquire.hpp"
#include "sgn/source_crt_descriptor_io.hpp"
#include <bit>
namespace sgn::retail
{
std::uint32_t source_acquire_stream_4667e1(SourceCrtStartupServices &memory)
{
    using Word = std::uint32_t;
    // 4667E1 captures the signed bound before4667F5 reads the table pointer.
    const auto count = memory.word(0x637aa0);
    if (std::bit_cast<std::int32_t>(count) <= 0)
        return 0;
    const auto table = memory.word(0x636a88);
    Word index = 0, cursor = table, file = 0;
    for (;;)
    {
        const auto candidate = memory.word(cursor); // 4667FD
        if (candidate == 0)
        {
            const auto offset = index * 4u;
            const auto allocated = memory.allocate_462fcd(32); // 46681F
            // The allocator is an external child. Both source reloads of the
            // actual table pointer occur AFTER it; retain even the zero store.
            const auto destination_table = memory.word(0x636a88); // 466825
            memory.word(destination_table + offset, allocated);   // 46682B
            const auto published_table = memory.word(0x636a88);   // 46682E
            file = memory.word(published_table + offset);         // 466833
            break;
        }
        if ((memory.byte(candidate + 0x0cu) & 0x83u) == 0)
        {
            // 466813 rereads from the original captured table, not cursor's
            // previously loaded candidate and not the current global table.
            file = memory.word(table + index * 4u);
            break;
        }
        ++index;
        cursor += 4u;
        if (std::bit_cast<std::int32_t>(index) >= std::bit_cast<std::int32_t>(count))
            return 0;
    }
    if (file == 0)
        return 0;
    // 466840 OR [FILE+10],FFFFFFFF has a constant final word. A direct store
    // avoids reading an unwritten malloc byte in the host memory admission map.
    // Only these six fields are assigned; +14/+18 retain their previous bytes.
    memory.word(file + 0x10u, 0xffffffffu);
    memory.word(file + 0x04u, 0); // 466844
    memory.word(file + 0x0cu, 0); // 466847, no busy flag until the later466671 child
    memory.word(file + 0x08u, 0); // 46684A
    memory.word(file, 0);         // 46684D
    memory.word(file + 0x1cu, 0); // 46684F
    return file;
}
} // namespace sgn::retail
