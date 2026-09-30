#pragma once
#include "sgn/source_crt_buffer.hpp"
namespace sgn::retail
{
// 4692B0: source overlap/direction, byte alignment, DWORD transfers and byte
// tails over the same guest memory. Returns the original destination. No
// temporary copy, preclear, new memory bank or host memcpy/memmove substitution.
std::uint32_t copy_source_crt_bytes_4692b0(SourceCrtBufferServices &, std::uint32_t destination, std::uint32_t source,
                                           std::uint32_t count);
// 463729 with the actual468FE0/4690B9 binary provider and FILE fields. Returns
// complete elements, preserving a partial last element and reached FILE effects.
// The multiplied byte count wraps as a source DWORD. A zero product returns
// before any FILE/buffer access. Text descriptor conversion remains the explicit
// boundary of the supplied binary provider, not a successful raw-read fallback.
std::uint32_t read_source_crt_elements_binary_463729(SourceCrtBufferServices &, std::uint32_t destination,
                                                     std::uint32_t element_size, std::uint32_t element_count,
                                                     std::uint32_t file);
} // namespace sgn::retail
