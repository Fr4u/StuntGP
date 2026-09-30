#include "sgn/source_crt_close.hpp"
#include <bit>
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
W group(W fd)
{
    return 0x637ac0u + static_cast<W>(std::bit_cast<std::int32_t>(fd) >> 5) * 4u;
}
W offset(W fd)
{
    return (fd & 31u) * 8u;
}
W bad_descriptor(SourceCrtDescriptorIO &m)
{
    (void)m.word(0x636534); // 466520 /46B914 original AND read-modify-write.
    m.word(0x636534, 0);
    m.word(0x636530, 9);
    return 0xffffffffu;
}
} // namespace
W source_descriptor_invalidate_46b8b1(SourceCrtDescriptorIO &m, SourceCrtCloseImports &api, W fd)
{
    if (fd >= m.word(0x637bc0))
        return bad_descriptor(m); // unsigned JNC46B8BD
    const auto table_slot = group(fd), record_offset = offset(fd);
    const auto record = m.word(table_slot) + record_offset; // 46B8D3..D5 cached for both checks
    if ((m.byte(record + 4u) & 1u) == 0 || m.word(record) == 0xffffffffu)
        return bad_descriptor(m);
    if (m.word(0x4959dc) == 1u)
    { // actual mutable PE DWORD, not a constant-folded import omission
        if (fd == 0u)
            (void)api.set_std_handle(0xfffffff6u, 0);
        else if (fd == 1u)
            (void)api.set_std_handle(0xfffffff5u, 0);
        else if (fd == 2u)
            (void)api.set_std_handle(0xfffffff4u, 0);
    }
    const auto current = m.word(table_slot) + record_offset; // 46B90A fresh after optional import
    (void)m.word(current);
    m.word(current, 0xffffffffu); // 46B90C OR RMW
    return 0;
}
W source_descriptor_close_466486(SourceCrtDescriptorIO &m, SourceCrtCloseImports &api, W fd)
{
    if (fd >= m.word(0x637bc0))
        return bad_descriptor(m); // 46648E..94 unsigned
    const auto table_slot = group(fd), record_offset = offset(fd);
    if ((m.byte(m.word(table_slot) + record_offset + 4u) & 1u) == 0)
        return bad_descriptor(m);
    W error = 0;
    if (source_descriptor_handle_46b92b(m, fd) != 0xffffffffu)
    {
        bool close = true;
        if (fd == 1u || fd == 2u)
        {
            const auto stderr_handle = source_descriptor_handle_46b92b(m, 2); // 4664CF before stdout
            const auto stdout_handle = source_descriptor_handle_46b92b(m, 1);
            close = stdout_handle != stderr_handle;
        }
        if (close)
        {
            const auto handle = source_descriptor_handle_46b92b(m, fd); // 4664E4 fresh lookup
            if (!api.close_native_handle(handle))
                error = m.last_error(); // 4664F5 before invalidation
        }
    }
    (void)source_descriptor_invalidate_46b8b1(m, api, fd);      // 466502 even after failed CloseHandle
    const auto flags = m.word(table_slot) + record_offset + 4u; // 466507 fresh group
    (void)m.byte(flags);
    m.byte(flags, 0); // 46650A AND RMW; invalidate result is ignored
    if (error == 0)
        return 0; // includes successful/skipped close, even if invalidation set errno
    m.map_os_error_4699d7(error);
    return 0xffffffffu;
}
void free_source_crt_buffer_466539(SourceCrtDescriptorIO &m, SourceCrtCloseImports &api, W file)
{
    const auto flags = m.word(file + 0xcu); // 46653E single initial DWORD read
    if ((flags & 0x83u) == 0 || (flags & 8u) == 0)
        return;
    api.free_462f9e(m.word(file + 8u)); // 466549..4C before low-WORD flags RMW
    const auto low = m.byte(file + 0xcu), high = m.byte(file + 0xdu);
    m.byte(file + 0xcu, static_cast<std::uint8_t>(low & 0xf7u));
    m.byte(file + 0xdu, static_cast<std::uint8_t>(high & 0xfbu)); // 466551 preserves upper flag WORD
    m.word(file, 0);
    m.word(file + 8u, 0);
    m.word(file + 4u, 0); // 46655A,55C,55F order
}
W close_source_crt_file_462a4d(SourceCrtDescriptorIO &m, SourceCrtFileFlushServices &flush, SourceCrtCloseImports &api,
                               W file)
{
    W result = 0xffffffffu;
    const auto flags = m.word(file + 0xcu); // 462A56 cached for both AL tests
    if ((flags & 0x40u) == 0 && (flags & 0x83u) != 0)
    {
        result = flush_source_crt_buffer_46659f(flush, file); // 462A67; no466564 commit path
        free_source_crt_buffer_466539(m, api, file);
        const auto closed = source_descriptor_close_466486(m, api, m.word(file + 0x10u));
        if (std::bit_cast<std::int32_t>(closed) < 0)
            result = 0xffffffffu;
        else
        {
            const auto name = m.word(file + 0x1cu); // 462A88 only after nonnegative descriptor close
            if (name != 0)
            {
                api.free_462f9e(name);
                (void)m.word(file + 0x1cu);
                m.word(file + 0x1cu, 0); // 462A95 fresh AND RMW
            }
        }
    }
    (void)m.word(file + 0xcu);
    m.word(file + 0xcu, 0); // 462A9C also for inactive/string FILE
    return result;
}
} // namespace sgn::retail
