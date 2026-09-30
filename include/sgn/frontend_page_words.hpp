#pragma once
// Shared frontend page-object word helpers (code organisation only, no new behaviour).
// These are the per-file helpers of the source_*_page translations, under one name each:
//     page_pool(f)                      whole 5FD258..5FF327 page pool as bytes        (was pool)
//     page_object_bytes(f, page, n)     first n bytes of page object `page`            (was object)
//     source_word(f, address)           reference to the 32-bit source word at address (was word)
//     page_word(f, page, offset)        little-endian word at offset of page object    (was get)
//     set_page_word(f, page, offset, v) store v little-endian at offset of page object (was put)
// page_word/set_page_word bound the access only through frontend_page_object_bytes(page,
// offset + 4), exactly like the local copies they replace. Variants with an extra check (e.g. the
// ArcadeScore `offset > 0x1fc` FormatError) are different helpers and stay local to their file.
// A file adopts a helper with `using sgn::retail::frontend_page_words::page_word;` (etc.) at the
// place of its former local helper.
#include "sgn/retail_frontend_page_pool.hpp"
#include "sgn/retail_frontend_state.hpp"
#include <cstddef>
#include <cstdint>
#include <span>

namespace sgn::retail::frontend_page_words
{
// Internal linkage on purpose (anonymous namespace, no `inline`): every translation unit gets its
// own copy, exactly like the per-file static helpers these replace. MSVC's inliner treats an
// internal function differently from an external inline one; with external linkage several page
// translations compiled to different (equivalent) code. [[maybe_unused]]: a file may use only some.
// page_word/set_page_word spell the pool expression out instead of calling page_pool(f): with the
// call, files whose copies spelled it out gained an unused out-of-line page_pool (object proof).
namespace
{
[[maybe_unused]] FrontendPagePool page_pool(FrontendState &f)
{
    return std::as_writable_bytes(std::span{f.source.page_pool});
}
[[maybe_unused]] std::span<std::byte> page_object_bytes(FrontendState &f, std::uint32_t page, std::size_t bytes)
{
    return frontend_page_object_bytes(page_pool(f), page, bytes);
}
[[maybe_unused]] std::uint32_t &source_word(FrontendState &f, std::uint32_t address)
{
    return f.source.source_word(address);
}
[[maybe_unused]] std::uint32_t page_word(FrontendState &f, std::uint32_t page, std::uint32_t offset)
{
    const auto b = frontend_page_object_bytes(std::as_writable_bytes(std::span{f.source.page_pool}), page, offset + 4u);
    std::uint32_t value = 0;
    for (std::uint32_t i = 0; i < 4; ++i)
        value |= std::to_integer<std::uint32_t>(b[offset + i]) << (8u * i);
    return value;
}
[[maybe_unused]] void set_page_word(FrontendState &f, std::uint32_t page, std::uint32_t offset, std::uint32_t value)
{
    const auto b = frontend_page_object_bytes(std::as_writable_bytes(std::span{f.source.page_pool}), page, offset + 4u);
    for (std::uint32_t i = 0; i < 4; ++i)
        b[offset + i] = std::byte((value >> (8u * i)) & 255u);
}
} // namespace
} // namespace sgn::retail::frontend_page_words
