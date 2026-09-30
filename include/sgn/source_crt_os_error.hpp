#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace sgn::retail
{
struct SourceCrtErrorPair
{
    std::uint32_t native_error, crt_error;
};
// PE seed, not a separate runtime error owner. A future App/CRT data owner
// can copy this table and expose its actual values through the span below.
extern const std::array<SourceCrtErrorPair, 45> source_crt_error_table_495fe8;
void source_crt_map_os_error_4699d7(std::uint32_t native_error, std::uint32_t &actual_dos_error_636534,
                                    std::uint32_t &actual_errno_636530,
                                    std::span<const SourceCrtErrorPair, 45> actual_table);
} // namespace sgn::retail
