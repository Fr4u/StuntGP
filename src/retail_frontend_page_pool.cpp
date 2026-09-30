#include "sgn/retail_frontend_page_pool.hpp"
#include <bit>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
constexpr std::uint32_t base = 0x5FD258, stride = 0x208;
constexpr std::size_t free_count = 0x2080, used_count = 0x2084;
constexpr std::size_t free_list = 0x2088, used_bytes = 0x20C8, peak_bytes = 0x20CC;
std::uint32_t get(FrontendPagePool pool, std::size_t at)
{
    std::uint32_t word = 0;
    for (unsigned i = 0; i < 4; ++i)
        word |= std::to_integer<std::uint32_t>(pool[at + i]) << (8 * i);
    return word;
}
void put(FrontendPagePool pool, std::size_t at, std::uint32_t word)
{
    for (unsigned i = 0; i < 4; ++i)
        pool[at + i] = static_cast<std::byte>((word >> (8 * i)) & 255u);
}
std::size_t offset(std::uint32_t address)
{
    if (address < base || address >= base + 16 * stride || (address - base) % stride != 0)
        throw std::invalid_argument("frontend page identity outside source pool");
    return address - base;
}
} // namespace
void initialize_frontend_page_pool_43d430(FrontendPagePool pool)
{
    for (std::uint32_t i = 0; i < 16; ++i)
        put(pool, free_list + i * 4, base + i * stride);
    put(pool, free_count, 16);
    put(pool, used_count, 0);
    // 43D430 does not clear object bytes, used-byte total or high-water mark.
}
std::uint32_t allocate_frontend_page_object_43d470(FrontendPagePool pool, std::uint32_t bytes)
{
    const auto available = get(pool, free_count);
    if (available < 1 || available > 16 || bytes > 512)
        throw std::invalid_argument("frontend page allocation outside source capacity");
    const auto address = get(pool, free_list + (available - 1) * 4);
    const auto at = offset(address);
    // Host validation above has no state effect. The following writes retain
    // 0043D470..0043D4B9 order and unsigned32 wrap before signed peak comparison.
    put(pool, free_count, available - 1);
    put(pool, used_count, get(pool, used_count) + 1);
    put(pool, at + 4, bytes);
    const auto used = get(pool, used_bytes) + bytes;
    put(pool, at, 0xAC1DCAFE);
    put(pool, used_bytes, used);
    if (std::bit_cast<std::int32_t>(used) > std::bit_cast<std::int32_t>(get(pool, peak_bytes)))
        put(pool, peak_bytes, used);
    return address + 8;
}
void release_frontend_page_object_43d4c0(FrontendPagePool pool, std::uint32_t object)
{
    if (object < 8)
        throw std::invalid_argument("frontend page object identity underflow");
    const auto at = offset(object - 8);
    const auto available = get(pool, free_count);
    if (available >= 16 || get(pool, at + 4) > 512)
        throw std::invalid_argument("frontend page release outside source capacity");
    put(pool, used_bytes, get(pool, used_bytes) - get(pool, at + 4));
    put(pool, free_list + available * 4, object - 8);
    put(pool, free_count, available + 1);
    put(pool, used_count, get(pool, used_count) - 1);
}
std::span<std::byte> frontend_page_object_bytes(FrontendPagePool pool, std::uint32_t object, std::size_t bytes)
{
    if (object < 8 || bytes > 512)
        throw std::invalid_argument("frontend page object view outside source capacity");
    return pool.subspan(offset(object - 8) + 8, bytes);
}
} // namespace sgn::retail
