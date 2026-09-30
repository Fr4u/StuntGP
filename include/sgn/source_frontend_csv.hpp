#pragma once
#include <cstdint>
#include <span>
#include <string_view>
namespace sgn::retail
{
class FrontendState;
class SourceFrontendCsv
{
  public:
    FrontendState &frontend;
    std::span<const std::uint8_t> input;
    std::uint32_t &cursor;
    SourceFrontendCsv(FrontendState &, std::span<const std::uint8_t>, std::uint32_t &);
    std::uint32_t token_43c680(std::span<std::uint8_t> output);
    bool label_43c600(std::string_view expected);
    std::uint32_t skip_43c520();
    void next_line_43c5c0();
    std::uint32_t number_43c770(std::uint32_t &destination_word);
    std::uint32_t floating_43c7b0(std::uint32_t &destination_float_bits);
    // Borrow the original App-owned NUL-inclusive literal region.
    std::span<const std::uint8_t> literal(std::uint32_t source_address) const;
    std::uint32_t format_key(std::uint32_t format_address, std::span<const std::uint32_t> arguments,
                             std::span<std::uint8_t> output, std::uint32_t return_site);
};
} // namespace sgn::retail
