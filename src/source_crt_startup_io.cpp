#include "sgn/source_crt_descriptor_io.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_signed;
void initialize_block(SourceCrtStartupServices &m, W slot, W start)
{
    // Retain unknown+6/+7 bytes of malloc storage; only source fields are written.
    auto cursor = start;
    while (cursor < m.word(slot) + 256u)
    {
        m.byte(cursor + 4u, 0);
        m.word(cursor, 0xffffffffu);
        m.byte(cursor + 5u, 10);
        cursor += 8u;
    }
}
W record(SourceCrtStartupServices &m, W fd)
{
    return m.word(0x637ac0u + (fd >> 5) * 4u) + (fd & 31u) * 8u;
}
} // namespace
void initialize_source_crt_descriptors_469792(SourceCrtStartupServices &m)
{
    const auto block = m.allocate_462fcd(256);
    if (block == 0)
        m.fatal_46412d(27);
    m.word(0x637ac0, block);
    m.word(0x637bc0, 32);
    initialize_block(m, 0x637ac0, block);
    const auto startup = m.get_startup_info_a();
    if (startup.reserved_bytes != 0 && startup.reserved_pointer != 0)
    {
        auto count = m.word(startup.reserved_pointer);
        auto flag = startup.reserved_pointer + 4u;
        auto handles = flag + count; // Original count, BEFORE signed2048 clamp.
        if (as_signed(count) >= 2048)
            count = 2048;
        W slot = 0x637ac4;
        while (as_signed(m.word(0x637bc0)) < as_signed(count))
        {
            const auto next = m.allocate_462fcd(256);
            if (next == 0)
            {
                count = m.word(0x637bc0);
                break;
            }
            m.word(0x637bc0, m.word(0x637bc0) + 32u);
            m.word(slot, next);
            initialize_block(m, slot, next);
            slot += 4u;
        }
        for (W fd = 0; as_signed(fd) < as_signed(count); ++fd, ++flag, handles += 4u)
        {
            const auto handle = m.word(handles);
            if (handle == 0xffffffffu)
                continue;
            const auto flags = m.byte(flag);
            if ((flags & 1u) == 0)
                continue;
            if ((flags & 8u) == 0 && m.get_file_type(handle) == 0)
                continue;
            const auto entry = record(m, fd);
            m.word(entry, m.word(handles));
            m.byte(entry + 4u, m.byte(flag));
        }
    }
    for (W fd = 0; fd < 3; ++fd)
    {
        const auto entry = record(m, fd);
        if (m.word(entry) == 0xffffffffu)
        {
            m.byte(entry + 4u, 0x81);
            const auto handle = m.get_std_handle(0xfffffff6u - fd);
            auto type = W{0};
            if (handle != 0xffffffffu)
                type = m.get_file_type(handle);
            if (handle == 0xffffffffu || type == 0)
                m.byte(entry + 4u, static_cast<std::uint8_t>(m.byte(entry + 4u) | 0x40u));
            else
            {
                type &= 255u;
                m.word(entry, handle);
                if (type == 2)
                    m.byte(entry + 4u, static_cast<std::uint8_t>(m.byte(entry + 4u) | 0x40u));
                else if (type == 3)
                    m.byte(entry + 4u, static_cast<std::uint8_t>(m.byte(entry + 4u) | 8u));
            }
        }
        else
            m.byte(entry + 4u, static_cast<std::uint8_t>(m.byte(entry + 4u) | 0x80u));
    }
    m.set_handle_count(m.word(0x637bc0));
}
void initialize_source_crt_streams_46b4c8(SourceCrtStartupServices &m)
{
    auto count = m.word(0x637aa0);
    if (count == 0)
    {
        count = 512;
        m.word(0x637aa0, count);
    }
    else if (as_signed(count) < 20)
    {
        count = 20;
        m.word(0x637aa0, count);
    }
    auto table = m.calloc_46d0c0(count, 4);
    m.word(0x636a88, table);
    if (table == 0)
    {
        m.word(0x637aa0, 20);
        table = m.calloc_46d0c0(20, 4);
        m.word(0x636a88, table);
        if (table == 0)
            m.fatal_46412d(26);
    }
    for (W index = 0; index < 20; ++index)
        m.word(m.word(0x636a88) + 4u * index, 0x4962e0u + 32u * index);
    for (W fd = 0; fd < 3; ++fd)
    {
        const auto handle = m.word(record(m, fd));
        if (handle == 0xffffffffu || handle == 0)
            m.word(0x4962f0u + fd * 32u, 0xffffffffu);
    }
}
} // namespace sgn::retail

namespace sgn::retail
{
void initialize_source_crt_callbacks_462dcd(SourceCrtStartupServices &m)
{
    const auto invoke = [&](std::uint32_t target) {
        if (target == 0)
            return;
        if (target == 0x46b4c8)
        {
            initialize_source_crt_streams_46b4c8(m);
            return;
        }
        if (target == 0x46cbe7)
        {
            if (m.word(0x637be8) == 0)
            {
                m.set_multibyte_codepage_46c823(0xfffffffdu);
                m.word(0x637be8, 1);
            }
            return;
        }
        m.invoke_startup_callback(target);
    };
    invoke(m.word(0x495944));
    for (std::uint32_t cursor = 0x472008; cursor < 0x472014; cursor += 4)
        invoke(m.word(cursor));
    for (std::uint32_t cursor = 0x472000; cursor < 0x472004; cursor += 4)
        invoke(m.word(cursor));
}
} // namespace sgn::retail
