#pragma once
#include <cstddef>
#include <string_view>
namespace sgn::retail
{
// Source445B80 token rules and the caller's line skip through445AE0. Cursor is
// a bounded host index; returned text borrows the original input. Empty means
// the source reader returned0, including an unclosed quoted token.
std::string_view read_retail_csv_token(std::string_view, std::size_t &cursor, std::size_t capacity);
void skip_retail_csv_line(std::string_view, std::size_t &cursor);
} // namespace sgn::retail
