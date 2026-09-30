#include "sgn/source_crt_fwrite.hpp"
#include "sgn/source_crt_descriptor_io.hpp"
#include "sgn/source_crt_buffer.hpp"
#include "sgn/source_crt_file_flush.hpp"
#include "sgn/source_crt_fread.hpp"
#include <bit>
#include <span>
#include <type_traits>
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
W low_file_flags(SourceCrtDescriptorIO &m, W file)
{
    const auto low = m.byte(file + 0xcu), high = m.byte(file + 0xdu);
    return static_cast<W>(low) | (static_cast<W>(high) << 8);
}
W failed_byte(SourceCrtDescriptorIO &m, W file, W flags)
{
    m.word(file + 0xcu, flags | 0x20u);
    return 0xffffffffu;
}
} // namespace
W flush_source_crt_byte_46462a(SourceCrtDescriptorIO &m, SourceCrtBufferServices &buffer, W value, W file)
{
    const auto initial = m.word(file + 0xcu);     // 464632 captured before the descriptor
    const auto descriptor = m.word(file + 0x10u); // 464635 also on invalid flags
    if ((initial & 0x82u) == 0 || (initial & 0x40u) != 0)
        return failed_byte(m, file, initial);
    if ((initial & 1u) != 0)
    {
        (void)m.word(file + 4u);
        m.word(file + 4u, 0); // 46464C AND RMW before EOF test
        if ((initial & 0x10u) == 0)
            return failed_byte(m, file, initial);
        const auto base = m.word(file + 8u);
        m.word(file, base);
        m.word(file + 0xcu, initial & ~1u); // 46465D,65F order
    }
    const auto current = m.word(file + 0xcu); // 464662 fresh flags after transition
    (void)m.word(file + 4u);
    m.word(file + 4u, 0); // 464665 AND RMW
    W received = 0;       // 464669 clears the actual local return-count slot
    const W writing = (current & ~0x10u) | 2u;
    m.word(file + 0xcu, writing); // 464675 before optional descriptor query/allocation
    if ((writing & 0x10cu) == 0 &&
        ((file != 0x496300u && file != 0x496320u) || source_descriptor_device_46b4a2(m, descriptor) == 0))
        allocate_source_crt_buffer_46b45e(buffer, file);
    W expected;
    if ((low_file_flags(m, file) & 0x108u) != 0)
    { // 46469C reads only the source low WORD
        const auto base = m.word(file + 8u), pointer = m.word(file);
        expected = pointer - base; // 4646AA DWORD difference, signed test below
        m.word(file, base + 1u);   // 4646AF before capacity read/count store
        const auto capacity = m.word(file + 0x18u);
        m.word(file + 4u, capacity - 1u);
        if (std::bit_cast<std::int32_t>(expected) > 0)
            received = source_descriptor_write_4695e5(m, descriptor, base, expected);
        else
        {
            W record = 0x495fe0u;
            if (descriptor != 0xffffffffu)
            {
                const auto slot = 0x637ac0u + static_cast<W>(std::bit_cast<std::int32_t>(descriptor) >> 5) * 4u;
                record = m.word(slot) + (descriptor & 31u) * 8u;
            }
            if ((m.byte(record + 4u) & 0x20u) != 0)
                (void)source_descriptor_seek_46993d(m, descriptor, 0, 2);
        }
        // 4646FF reloads the base even after a failed/short descriptor write.
        // The incoming byte is stored before comparing received with expected.
        const auto current_base = m.word(file + 8u);
        m.byte(current_base, static_cast<std::uint8_t>(value));
    }
    else
    {
        expected = 1;
        // NEW HOST borrow of the existing argument's low byte, no guest VA or
        // heap storage. The full4695E5 body still owns validation/error effects.
        static_assert(std::endian::native == std::endian::little);
        static_assert(std::is_same_v<std::uint8_t, unsigned char>);
        const std::span<const std::uint8_t, 1> argument_byte{reinterpret_cast<const std::uint8_t *>(&value), 1};
        received = source_descriptor_write_local_4695e5(m, descriptor, argument_byte); // 464712
    }
    if (received != expected)
        return failed_byte(m, file, m.word(file + 0xcu)); // 464723 fresh RMW
    return value & 0xffu;                                 // 464729..2F, not the sign-extended input
}
W write_source_crt_elements_463811(SourceCrtDescriptorIO &m, SourceCrtBufferServices &buffer,
                                   SourceCrtFileFlushServices &flush, W source, W element_size, W element_count, W file)
{
    const W total = element_size * element_count; // 46381B retains the low DWORD
    if (total == 0)
        return 0; // 463825..34, no FILE/input/provider access
    W remaining = total;
    W capacity = (low_file_flags(m, file) & 0x10cu) != 0 ? m.word(file + 0x18u) : 0x1000u;
    while (remaining != 0)
    {
        const auto buffered = m.word(file + 0xcu) & 0x108u; // 46384F captures the full DWORD mask
        if (buffered != 0)
        {
            const auto available = m.word(file + 4u);
            if (available != 0)
            {
                const W copied = remaining < available ? remaining : available;
                const auto destination = m.word(file);
                (void)copy_source_crt_bytes_4692b0(buffer, destination, source, copied);
                // 463874/77 re-read each destination after source-copy effects.
                m.word(file + 4u, m.word(file + 4u) - copied);
                m.word(file, m.word(file) + copied);
                remaining -= copied;
                source += copied;
                continue;
            }
        }
        if (remaining >= capacity)
        {
            if (buffered != 0 && flush_source_crt_buffer_46659f(flush, file) != 0)
                return (total - remaining) / element_size;
            const W requested = capacity == 0 ? remaining : remaining - remaining % capacity;
            const auto descriptor = m.word(file + 0x10u);
            const auto written = source_descriptor_write_4695e5(m, descriptor, source, requested);
            if (written != 0xffffffffu)
            {
                source += written;
                remaining -= written;
            } // 4638C0/C3 precede short-write test
            if (written == 0xffffffffu || written < requested)
            {
                m.word(file + 0xcu, m.word(file + 0xcu) | 0x20u); // 463907 source RMW
                return (total - remaining) / element_size;
            }
        }
        else
        {
            const auto value = static_cast<W>(static_cast<std::int32_t>(std::bit_cast<std::int8_t>(m.byte(source))));
            if (flush_source_crt_byte_46462a(m, buffer, value, file) == 0xffffffffu)
                return (total - remaining) / element_size;
            ++source;
            capacity = m.word(file + 0x18u);
            --remaining; // 4638E2,E5,E8 order
            if (std::bit_cast<std::int32_t>(capacity) <= 0)
                capacity = 1; // 4638EC..F6 signed clamp
        }
    }
    return element_count; // 4638FF, original element count even for a wrapped product
}
} // namespace sgn::retail
