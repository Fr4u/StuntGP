#include "sgn/source_crt_read.hpp"
#include <bit>
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
W invalid_descriptor(SourceCrtDescriptorIO &memory)
{
    memory.word(0x636534, 0);
    memory.word(0x636530, 9);
    return 0xffffffffu; // 469296..4692A7
}
} // namespace
W source_descriptor_read_binary_4690b9(SourceCrtDescriptorIO &memory, SourceCrtReadFileImport &input, W descriptor,
                                       W buffer, W size)
{
    if (descriptor >= memory.word(0x637bc0))
        return invalid_descriptor(memory); // 4690C5..4690CB unsigned
    const auto group = 0x637ac0u + static_cast<W>(std::bit_cast<std::int32_t>(descriptor) >> 5) * 4u;
    const auto offset = (descriptor & 31u) * 8u;
    const auto record = memory.word(group) + offset;
    const auto flags = memory.byte(record + 4u); // 4690DC..4690EF, captured DL
    if ((flags & 1u) == 0)
        return invalid_descriptor(memory);
    if (size == 0 || (flags & 2u) != 0)
        return 0; // 4690FF..46910A, no import or output access
    // NEW admission check before a transfer: this entry does not implement the
    // text translation branch469199..46928E. No descriptor/FILE default is invented.
    if ((flags & 0x80u) != 0)
        throw SourceCrtReadGap(0x46910c, flags, "text descriptor is outside the binary read entry");
    W total = 0, cursor = buffer, remaining = size;
    if ((flags & 0x48u) != 0)
    {
        const auto lookahead = memory.byte(record + 5u);
        if (lookahead != 10)
        {
            --remaining;
            memory.byte(buffer, lookahead); // 469118..46911B; retain before the native call
            cursor = buffer + 1u;
            total = 1;
            memory.byte(memory.word(group) + offset + 5u, 10); // 46911D..469129 fresh group read
        }
    }
    W transferred; // 46913D initializes the source local through ReadFile, not a source default.
    const auto handle = memory.word(memory.word(group) + offset); // 469134..46913A
    if (!input.read_file(handle, cursor, remaining, transferred))
    {
        const auto error = memory.last_error(); // 469147 before any CRT error-store
        if (error == 5)
        {
            memory.word(0x636530, 9);
            memory.word(0x636534, 5);
            return 0xffffffffu;
        }
        if (error == 109)
            return 0; // 469169..469170, even if a lookahead byte was already stored
        memory.map_os_error_4699d7(error);
        return 0xffffffffu;
    }
    const auto current_group = memory.word(group); // 469181 reload before consuming the count
    total += transferred; // 469186 DWORD arithmetic; earlier destination bytes are never rolled back
    const auto current_flags = memory.byte(current_group + offset + 4u);
    if ((current_flags & 0x80u) != 0)
        throw SourceCrtReadGap(0x469193, current_flags,
                               "descriptor changed to text during binary ReadFile; transfer effects retained");
    return total; // 469193 ->469291; no LF/CR/CTRL-Z translation or descriptor-EOF write
}
} // namespace sgn::retail
