#pragma once
#include <cstdint>
#include <span>

namespace sgn::retail
{
class SourceApplicationLiteralRef;
// 43C7D2 -> sscanf46348E, fixed App-owned "%f". Read the complete 256-byte
// scratch through its first NUL, including retained bytes after lexer failure.
// True: one assignment. False: matching/input failure, destination unchanged.
// Unsupported numeric/owner domain throws; destination remains unchanged.
bool scan_frontend_float_46348e(const SourceApplicationLiteralRef &app, std::span<const std::uint8_t> scratch,
                                std::uint32_t &destination_bits);
} // namespace sgn::retail
