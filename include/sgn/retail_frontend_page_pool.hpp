#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace sgn::retail
{
// Borrow the future sole frontend owner of 5FD258..5FF327. Original storage
// has16 blocks of0x208 (8-byte header +512-byte body), followed by0x50 metadata.
// Returned32-bit values are checked source object identities, not host pointers.
using FrontendPagePool = std::span<std::byte, 0x20D0>;
void initialize_frontend_page_pool_43d430(FrontendPagePool);
std::uint32_t allocate_frontend_page_object_43d470(FrontendPagePool, std::uint32_t bytes);
void release_frontend_page_object_43d4c0(FrontendPagePool, std::uint32_t object);
std::span<std::byte> frontend_page_object_bytes(FrontendPagePool, std::uint32_t object, std::size_t bytes);
} // namespace sgn::retail
