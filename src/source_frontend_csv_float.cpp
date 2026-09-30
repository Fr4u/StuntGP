#include "sgn/source_frontend_csv_float.hpp"
#include "sgn/source_decimal_conversion.hpp"
#include "sgn/source_application_literals.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
[[noreturn]] void outside_domain(const char *reason)
{
    throw std::domain_error(reason);
}
} // namespace

bool scan_frontend_float_46348e(const SourceApplicationLiteralRef &app, std::span<const std::uint8_t> scratch,
                                Word &destination_bits)
{
    const auto format = app.frontend_csv_literal(0x4794dc);
    if (format.size() != 3 || format[0] != '%' || format[1] != 'f' || format[2] != 0)
        outside_domain("46348E: unsupported frontend scanf format");
    if (scratch.size() != 256)
        outside_domain("46348E %f: expected complete 5FD048 scratch owner");
    const auto end = std::find(scratch.begin(), scratch.end(), std::uint8_t{0});
    if (end == scratch.end())
        outside_domain("46348E %f: strlen would leave the 256-byte scratch owner");
    const auto length = static_cast<std::size_t>(end - scratch.begin());
    const auto point = app.read_frontend_decimal_point();
    if (point != '.')
        outside_domain("46348E %f: non-original decimal locale");
    std::size_t cursor = 0;
    while (cursor < length && app.classify_number_4631f4(scratch[cursor], 8) != 0)
        ++cursor;
    bool negative = false;
    if (cursor < length && (scratch[cursor] == '-' || scratch[cursor] == '+'))
    {
        negative = scratch[cursor] == '-';
        ++cursor;
    }
    std::array<std::uint8_t, 8> digits{};
    std::size_t count = 0;
    int fractional = 0;
    bool any_digit = false;
    const auto digit_run = [&](bool after_point) {
        while (cursor < length && app.classify_number_4631f4(scratch[cursor], 4) != 0)
        {
            const auto character = scratch[cursor++];
            if (character < '0' || character > '9')
                outside_domain("46348E %f: non-ASCII digit outside original CSV domain");
            any_digit = true;
            if (after_point && ++fractional > 5)
                outside_domain("46348E %f: more than five fractional places");
            const auto digit = static_cast<std::uint8_t>(character - '0');
            if (count == 0 && digit == 0)
                continue;
            if (count == digits.size())
                outside_domain("46348E %f: more than eight significant digits");
            digits[count++] = digit;
        }
    };
    digit_run(false);
    if (cursor < length && scratch[cursor] == point)
    {
        ++cursor;
        digit_run(true);
    }
    if (!any_digit)
        return false;
    if (cursor < length && (scratch[cursor] == 'e' || scratch[cursor] == 'E'))
        outside_domain("46348E %f: exponent notation outside original CSV domain");
    // 468436 NUL-terminates the numeric prefix before calling __fassign.
    // Other suffixes are not forwarded to the decimal parser.
    int power = -fractional;
    while (count != 0 && digits[count - 1] == 0)
    {
        --count;
        ++power;
    }
    const auto value =
        decode_source_decimal_digits_bounded(app, std::span<const std::uint8_t>{digits.data(), count}, power, negative);
    const Word converted = pack_source_frontend_float_bounded(app, value);
    destination_bits = converted; // __fassign4642AF: single final DWORD store
    return true;
}
} // namespace sgn::retail
