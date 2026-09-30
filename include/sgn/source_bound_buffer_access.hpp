#pragma once
#include "sgn/source_world_span.hpp"
#include "sgn/source_transport_word.hpp"
#include <limits>
#include <span>
namespace sgn::retail
{
// NEW host representation (R19 perf_entities), not recovered behaviour.
// Same reached checks, messages, order and bank calls as SourceTransportBuffer
// (source_screen_transport.cpp: extent(), then the one bank operation), minus
// its per-access weak platform lock. Use ONLY where the caller already holds a
// locked shared_ptr to the platform and has admitted this binding's platform
// as owner-equivalent to it (difference()/same() checks): the skipped lock can
// then never fail and never releases anything within the synchronous call.
// The binding is borrowed for one access; nothing is retained.
namespace bound_buffer
{
inline std::uint32_t extent(const SourceWorldBufferBinding &b, std::uint32_t at, std::uint32_t n, std::uint32_t site)
{
    const auto count = b.byte_count == std::numeric_limits<std::uint32_t>::max() ? b.bytes : b.byte_count;
    if (!b.bank || at > count || n > count - at || b.byte_offset > b.bytes || count > b.bytes - b.byte_offset)
        throw SourceAllocationError(site, "reached source buffer access leaves captured allocation extent");
    if (b.bank->allocation_size(b.allocation) != b.bytes)
        throw SourceAllocationError(site, "source buffer generation/extent changed");
    return b.byte_offset + at;
}
inline void known(const SourceWorldBufferBinding &b, std::uint32_t at, std::span<const std::uint8_t> bytes,
                  std::uint32_t site)
{
    const auto offset = extent(b, at, static_cast<std::uint32_t>(bytes.size()), site);
    b.bank->write_known(b.allocation, offset, bytes, site);
}
inline void store(const SourceWorldBufferBinding &b, std::uint32_t at, const SourceTransportWord &value,
                  std::uint32_t site)
{
    const auto offset = extent(b, at, 4, site);
    b.bank->store_transport_word(b.allocation, offset, value, site);
}
inline std::span<std::uint8_t> writable_known(const SourceWorldBufferBinding &b, std::uint32_t at, std::uint32_t n,
                                              std::uint32_t site)
{
    const auto offset = extent(b, at, n, site);
    return b.bank->readable_bytes(b.allocation, offset, n, site);
}
inline std::span<const std::uint8_t> read(const SourceWorldBufferBinding &b, std::uint32_t at, std::uint32_t n,
                                          std::uint32_t site)
{
    const auto offset = extent(b, at, n, site);
    return b.bank->readable_bytes(b.allocation, offset, n, site);
}
} // namespace bound_buffer
} // namespace sgn::retail
