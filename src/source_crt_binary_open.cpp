#include "sgn/source_crt_binary_open.hpp"
#include "sgn/source_crt_stream_acquire.hpp"
#include "sgn/source_crt_descriptor_io.hpp"
#include "sgn/source_crt_close.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>
namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
Word arithmetic_shift(Word value, int count)
{
    return static_cast<Word>(std::bit_cast<std::int32_t>(value) >> count);
}
Word descriptor_group(Word descriptor)
{
    return 0x637ac0u + arithmetic_shift(descriptor, 5) * 4u;
}
Word descriptor_offset(Word descriptor)
{
    return (descriptor & 31u) * 8u;
}
Word source_open_binary_46b9bf(SourceCrtStartupServices &memory, SourceCrtCloseImports &close,
                               SourceCrtBinaryOpenImport &native, std::span<const std::uint8_t> path, bool writing)
{
    // Admitted rb/wb: oflag8000/8301, share40, permission1A4. Explicit
    // binary avoids636858. Only wb reaches the mutable creation-umask read.
    Word attributes = 0x80u;
    if (writing && ((~memory.word(0x636538u) & 0x1a4u & 0x80u) == 0))
        attributes = 1u;                                              // 46BAF9..46BB0B, before descriptor acquisition
    const auto descriptor = source_acquire_descriptor_46b7a5(memory); // 46BB37
    if (descriptor == 0xffffffffu)
    {
        (void)memory.word(0x636534); // 46BB45 AND RMW of the existing initialized App word
        memory.word(0x636534, 0);
        memory.word(0x636530, 24); // 46BB45..4C
        return 0xffffffffu;
    }
    // Original SECURITY_ATTRIBUTES has descriptorNULL and inherit1. Host
    // transport adjusts only its cb for the actual Windows ABI; templateNULL.
    const auto handle = native.create_file_a(path, writing ? 0x40000000u : 0x80000000u, 3, true, writing ? 2u : 3u,
                                             attributes); // 46BB6B
    Word type = 0;
    if (handle != 0xffffffffu)
    {
        type = memory.get_file_type(handle); // 46BB78, whole DWORD, not low byte
        if (type == 0)
            (void)close.close_native_handle(handle); // 46BB83
    }
    if (handle == 0xffffffffu || type == 0)
    {
        // When GetFileType failed, CloseHandle precedes this GetLastError.
        const auto error = memory.last_error();
        memory.map_os_error_4699d7(error);
        return 0xffffffffu;
    }
    const Word type_flags = type == 2u ? 0x40u : type == 3u ? 8u : 0u;
    // 46BBB3 return is discarded by source. Publication may SetStdHandle.
    (void)source_publish_descriptor_handle_46b83a(memory, close, descriptor, handle);
    const auto entry = memory.word(descriptor_group(descriptor)) + descriptor_offset(descriptor);
    memory.byte(entry + 4u, static_cast<std::uint8_t>(type_flags | 1u)); // 46BBDC
    // Both admitted binary modes skip text/update and append children.
    return descriptor; // 46BC71
}
Word source_open_binary_466671(SourceCrtStartupServices &memory, SourceCrtCloseImports &close,
                               SourceCrtBinaryOpenImport &native, std::span<const std::uint8_t> path, Word file,
                               bool writing)
{
    // 466678 precedes either admitted parse. Original default FILEflags may
    // contain commit mode; retain them before ORing the r/w direction bit.
    const auto flags = memory.word(0x63681c) | (writing ? 2u : 1u);
    const auto descriptor = source_open_binary_46b9bf(memory, close, native, path, writing); // 4667B0
    if (std::bit_cast<std::int32_t>(descriptor) < 0)
        return 0;
    memory.word(0x636818, memory.word(0x636818) + 1u); // 4667C5, existing peer-owned counter
    memory.word(file + 0x0cu, flags);                  // 4667CB
    memory.word(file + 4u, 0);                         // 4667CE
    memory.word(file, 0);                              // 4667D1
    memory.word(file + 8u, 0);                         // 4667D3
    memory.word(file + 0x1cu, 0);                      // 4667D6
    memory.word(file + 0x10u, descriptor);             // 4667D9
    return file;
}
} // namespace
Word source_acquire_descriptor_46b7a5(SourceCrtStartupServices &memory)
{
    Word group = 0, first_descriptor = 0, table_slot = 0x637ac0u;
    while (table_slot < 0x637bc0u)
    {
        auto cursor = memory.word(table_slot); // 46B7B4
        if (cursor == 0)
        {
            const auto allocated = memory.allocate_462fcd(256); // 46B7F7
            if (allocated == 0)
                return 0xffffffffu;
            memory.word(0x637bc0, memory.word(0x637bc0) + 32u); // 46B801
            table_slot = 0x637ac0u + group * 4u;
            auto end = allocated + 256u;
            memory.word(table_slot, allocated); // 46B815
            cursor = allocated;
            while (cursor < end)
            {
                // Constant AND/OR outputs do not read uninitialized host bytes.
                memory.byte(cursor + 4u, 0);
                memory.word(cursor, 0xffffffffu);
                memory.byte(cursor + 5u, 10);
                end = memory.word(table_slot) + 256u; // 46B826..2B reload every iteration
                cursor += 8u;
            }
            return group * 32u; // 46B82F..32
        }
        const auto end = cursor + 256u; // 46B7BA captured for this existing block
        while (cursor < end)
        {
            if ((memory.byte(cursor + 4u) & 1u) == 0)
            {
                (void)memory.word(cursor);        // 46B7CF RMW of the existing initialized record
                memory.word(cursor, 0xffffffffu); // leave flags/extra bytes alone
                const auto descriptor = arithmetic_shift(cursor - memory.word(table_slot), 3) + first_descriptor;
                if (descriptor != 0xffffffffu)
                    return descriptor;
                break; // 46B7DE falls through the next-group path
            }
            cursor += 8u;
        }
        table_slot += 4u;
        ++group;
        first_descriptor += 32u;
    }
    return 0xffffffffu;
}
Word source_publish_descriptor_handle_46b83a(SourceCrtStartupServices &memory, SourceCrtCloseImports &native,
                                             Word descriptor, Word handle)
{
    if (descriptor < memory.word(0x637bc0))
    {
        const auto slot = descriptor_group(descriptor), offset = descriptor_offset(descriptor);
        if (memory.word(memory.word(slot) + offset) == 0xffffffffu)
        {
            if (memory.word(0x4959dc) == 1u && descriptor <= 2u)
                (void)native.set_std_handle(0xfffffff6u - descriptor, handle); // 46B88A
            // Original reload follows optional native SetStdHandle.
            memory.word(memory.word(slot) + offset, handle); // 46B890..92
            return 0;
        }
    }
    (void)memory.word(0x636534); // 46B89A AND RMW
    memory.word(0x636534, 0);
    memory.word(0x636530, 9); // 46B89A..A1
    return 0xffffffffu;
}
Word source_fopen_binary_read_462ac3(SourceCrtStartupServices &memory, SourceCrtCloseImports &close,
                                     SourceCrtBinaryOpenImport &native, std::span<const std::uint8_t> path,
                                     std::span<const std::uint8_t> mode)
{
    // NEW host admission; no generic fopen claim for unimplemented modes.
    if (mode.size() < 3 || mode[0] != 'r' || mode[1] != 'b' || mode[2] != 0)
        throw std::logic_error("source binary-read fopen admits actual rb NUL bytes only");
    if (std::find(path.begin(), path.end(), std::uint8_t{0}) == path.end())
        throw std::logic_error("source binary-read fopen requires a bounded NUL pathname");
    // 462AC3 supplies share40;462AA3 acquires before invoking466671.
    const auto file = source_acquire_stream_4667e1(memory); // 462AA3
    if (file == 0)
        return 0;
    return source_open_binary_466671(memory, close, native, path, file, false); // 462ABA
}
Word source_fopen_binary_write_462ac3(SourceCrtStartupServices &memory, SourceCrtCloseImports &close,
                                      SourceCrtBinaryOpenImport &native, std::span<const std::uint8_t> path,
                                      std::span<const std::uint8_t> mode)
{
    // NEW host admission: this API is the actual bounded wb caller, not a
    // generic fopen substitute. Admission precedes FILE/descriptor effects.
    if (mode.size() < 3 || mode[0] != 'w' || mode[1] != 'b' || mode[2] != 0)
        throw std::logic_error("source binary-write fopen admits actual wb NUL bytes only");
    if (std::find(path.begin(), path.end(), std::uint8_t{0}) == path.end())
        throw std::logic_error("source binary-write fopen requires a bounded NUL pathname");
    const auto file = source_acquire_stream_4667e1(memory); // 462AA3, before466678
    if (file == 0)
        return 0;
    return source_open_binary_466671(memory, close, native, path, file, true); // 462ABA
}
} // namespace sgn::retail
