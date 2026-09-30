#include "sgn/source_crt_descriptor_io.hpp"
#include <array>
#include <bit>
#include <stdexcept>
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
W group(W fd)
{
    return 0x637ac0u + (static_cast<W>(std::bit_cast<std::int32_t>(fd) >> 5)) * 4u;
}
W offset(W fd)
{
    return (fd & 31u) * 8u;
}
W record(SourceCrtDescriptorIO &m, W fd)
{
    return m.word(group(fd)) + offset(fd);
}
bool valid(SourceCrtDescriptorIO &m, W fd)
{
    return fd < m.word(0x637bc0) && (m.byte(record(m, fd) + 4u) & 1u) != 0;
}
W bad(SourceCrtDescriptorIO &m)
{
    m.word(0x636534, 0);
    m.word(0x636530, 9);
    return 0xffffffffu;
}
} // namespace
W source_descriptor_device_46b4a2(SourceCrtDescriptorIO &m, W fd)
{
    if (fd >= m.word(0x637bc0))
        return 0; // 46B4A2..B0 unsigned; no error-store child
    // 46B4B1..C7 uses the same group/record address, without checking open flag0x01.
    return static_cast<W>(m.byte(record(m, fd) + 4u)) & 0x40u;
}
W source_descriptor_handle_46b92b(SourceCrtDescriptorIO &m, W fd)
{
    if (!valid(m, fd))
        return bad(m);
    return m.word(record(m, fd));
}
W source_descriptor_seek_46993d(SourceCrtDescriptorIO &m, W fd, W amount, W method)
{
    if (!valid(m, fd))
        return bad(m);
    const auto handle = source_descriptor_handle_46b92b(m, fd);
    if (handle == 0xffffffffu)
    {
        m.word(0x636530, 9);
        return 0xffffffffu;
    }
    const auto position = m.set_file_pointer(handle, amount, method);
    const auto error = position == 0xffffffffu ? m.last_error() : 0u;
    if (error != 0)
    {
        m.map_os_error_4699d7(error);
        return 0xffffffffu;
    }
    const auto at = record(m, fd) + 4u;
    m.byte(at, static_cast<std::uint8_t>(m.byte(at) & 0xfdu));
    return position;
}
W source_descriptor_commit_46b968(SourceCrtDescriptorIO &m, W fd)
{
    if (!valid(m, fd))
    {
        m.word(0x636530, 9);
        return 0xffffffffu;
    } // DOS error retained in this branch.
    const auto handle = source_descriptor_handle_46b92b(m, fd);
    const auto error = m.flush_file_buffers(handle) ? 0u : m.last_error();
    if (error == 0)
        return 0;
    m.word(0x636534, error);
    m.word(0x636530, 9);
    return 0xffffffffu;
}
namespace
{
// NEW HOST input transport only. Guest cursor is an actual source DWORD;
// Local cursor is a relative span offset, never an allocated guest identity.
struct GuestWriteInput
{
    W base;
    W begin() const
    {
        return base;
    }
    std::uint8_t read(SourceCrtDescriptorIO &m, W cursor) const
    {
        return m.byte(cursor);
    }
    bool write(SourceCrtDescriptorIO &m, W handle, W size, W &written) const
    {
        return m.write_file(handle, base, size, written);
    }
};
struct LocalWriteInput
{
    std::span<const std::uint8_t, 1> bytes;
    W begin() const
    {
        return 0;
    }
    std::uint8_t read(SourceCrtDescriptorIO &, W offset) const
    {
        if (offset >= bytes.size())
            throw std::out_of_range("4695E5 local input leaves its source stack-byte borrow");
        return bytes[offset];
    }
    bool write(SourceCrtDescriptorIO &m, W handle, W size, W &written) const
    {
        if (size > bytes.size())
            throw std::out_of_range("4695E5 native input leaves its source stack-byte borrow");
        return m.write_file_local(handle, bytes.first(size), written);
    }
};
// Single recovered4695E5 body. Preserve each captured or fresh source read,
// append seek, text expansion, native import, returned count and error decision.
template <class Input> W write_descriptor(SourceCrtDescriptorIO &m, W fd, const Input &input, W size)
{
    // 469779 AND is a read-modify-write, including its discarded read.
    const auto invalid = [&]() {
        (void)m.word(0x636534u);
        return bad(m);
    };
    if (fd >= m.word(0x637bc0u))
        return invalid();
    const W slot = group(fd), slot_offset = offset(fd);
    const auto initial_flags = m.byte(m.word(slot) + slot_offset + 4u); // 469614/16
    if ((initial_flags & 1u) == 0)
        return invalid();
    if (size == 0)
        return 0;
    // 469636 tests the SAME AL captured by the open-bit test, not fresh flags.
    if ((initial_flags & 0x20u) != 0)
        (void)source_descriptor_seek_46993d(m, fd, 0, 2);
    W total = 0, extra = 0, error = 0;
    const W current_record = m.word(slot) + slot_offset; // 469646/48, after append
    if ((m.byte(current_record + 4u) & 0x80u) != 0)
    {
        W cursor = input.begin();
        do
        {
            // Original stack region admits1025 bytes: final LF can appendCR+LF at offset1023.
            std::array<std::uint8_t, 1025> local;
            W used = 0;
            while (cursor - input.begin() < size)
            {
                const auto c = input.read(m, cursor++);
                if (c == 10)
                {
                    ++extra;
                    local[used++] = 13;
                }
                local[used++] = c;
                if (used >= 1024)
                    break;
            }
            W written;
            if (!m.write_file_local(m.word(record(m, fd)), std::span{local}.first(used), written))
            {
                error = m.last_error();
                break;
            }
            total += written;
            if (std::bit_cast<std::int32_t>(written) < std::bit_cast<std::int32_t>(used))
                break;
        } while (cursor - input.begin() < size);
    }
    else
    {
        W written;
        if (input.write(m, m.word(current_record), size, written))
            total = written;
        else
            error = m.last_error();
    }
    if (total != 0)
        return total - extra; // Source subtracts ALL insertedCR, even on a partial write.
    if (error != 0)
    {
        if (error == 5)
        {
            m.word(0x636530, 9);
            m.word(0x636534, 5);
        }
        else
            m.map_os_error_4699d7(error);
        return 0xffffffffu;
    }
    if ((m.byte(record(m, fd) + 4u) & 0x40u) != 0 && input.read(m, input.begin()) == 0x1a)
        return 0;
    m.word(0x636530, 28);
    m.word(0x636534, 0);
    return 0xffffffffu;
}
} // namespace
W source_descriptor_write_4695e5(SourceCrtDescriptorIO &m, W fd, W buffer, W size)
{
    return write_descriptor(m, fd, GuestWriteInput{buffer}, size);
}
W source_descriptor_write_local_4695e5(SourceCrtDescriptorIO &m, W fd, std::span<const std::uint8_t, 1> argument_byte)
{
    return write_descriptor(m, fd, LocalWriteInput{argument_byte}, 1);
}
} // namespace sgn::retail
