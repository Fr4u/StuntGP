#pragma once
#include <cstdint>
#include <span>

namespace sgn::retail
{
class FrontendState;
// Full43BBB0 and bit-reader/delta/normalization children. Borrowed owners only.
// Copies172 bytes before reading the first control byte, even for a terminal
// first frame. Retains all header bits. Result128+44*frames includes terminal.
// Partial output remains on a reached host memory boundary; no preflight scan,
// invented sentinel, normalization feedback or rewritten recording cursor.
std::uint32_t decompress_source_replay_43bbb0(std::span<std::uint8_t> destination,
                                              std::span<const std::uint8_t> compressed_source);
// Actual44D9E9 arguments: captured5F26A8 source slot address and5E3C48 input.
// Destination is a suffix of the SAME world ReplayBank; no second allocation.
std::uint32_t decompress_frontend_saved_replay_44d9e9(FrontendState &, std::uint32_t captured_destination_address);
} // namespace sgn::retail
