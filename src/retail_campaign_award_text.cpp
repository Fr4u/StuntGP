#include "sgn/retail_campaign_award_text.hpp"
#include <array>
#include <bit>
#include <charconv>
#include <cstring>
#include <stdexcept>

namespace sgn::retail
{
std::uint32_t format_campaign_award_46280c(std::span<std::byte, 260> destination, std::string_view format,
                                           std::uint32_t value)
{
    const auto marker = format.find('%');
    if (marker == std::string_view::npos || marker + 1 >= format.size() || format[marker + 1] != 'd' ||
        format.find('%', marker + 1) != std::string_view::npos || format.find('\0') != std::string_view::npos)
        throw std::invalid_argument("campaign award format outside recovered single-%d domain");
    std::array<char, 12> digits{};
    const auto converted =
        std::to_chars(digits.data(), digits.data() + digits.size(), std::bit_cast<std::int32_t>(value));
    if (converted.ec != std::errc{})
        throw std::logic_error("signed32 campaign award conversion failed");
    const auto count = static_cast<std::size_t>(converted.ptr - digits.data());
    if (format.size() - 2 > destination.size() - 1 - count)
        throw std::invalid_argument("campaign award text exceeds source260-byte owner");
    const auto length = format.size() - 2 + count;
    // A bounded local result also prevents host aliasing from invalidating the
    // borrowed format during assembly. The actual owner receives length+NUL only.
    std::array<char, 260> result{};
    std::memcpy(result.data(), format.data(), marker);
    std::memcpy(result.data() + marker, digits.data(), count);
    std::memcpy(result.data() + marker + count, format.data() + marker + 2, format.size() - marker - 2);
    std::memcpy(destination.data(), result.data(), length + 1);
    return static_cast<std::uint32_t>(length);
}
} // namespace sgn::retail
