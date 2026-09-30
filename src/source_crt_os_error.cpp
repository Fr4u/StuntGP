#include "sgn/source_crt_os_error.hpp"
#include <stdexcept>

namespace sgn::retail
{
void source_crt_map_os_error_4699d7(std::uint32_t error, std::uint32_t &dos_error, std::uint32_t &crt_error,
                                    std::span<const SourceCrtErrorPair, 45> table)
{
    if (&dos_error == &crt_error)
        throw std::logic_error("source CRT error words must be distinct owners");
    dos_error = error; // 4699DD before any table comparison
    for (const auto &pair : table)
    {
        if (error == pair.native_error)
        {
            crt_error = pair.crt_error;
            return;
        }
    }
    if (error >= 0x13u && error <= 0x24u)
    {
        crt_error = 13;
        return;
    }
    if (error >= 0xbcu)
    {
        crt_error = 8; // 469A27 also executes for errors above202
        if (error <= 0xcau)
            return;
    }
    crt_error = 22; // 469A33, including the second write for errors above202
}
} // namespace sgn::retail
