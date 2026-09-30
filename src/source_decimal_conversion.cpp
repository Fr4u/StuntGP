#include "sgn/source_decimal_conversion.hpp"
#include "sgn/source_application_literals.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
using Extended12 = SourceExtendedDecimal12;
[[noreturn]] void outside_domain(const char *reason)
{
    throw std::domain_error(reason);
}
Word pair(std::uint16_t low, std::uint16_t high)
{
    return Word{low} | (Word{high} << 16);
}
Word read32(std::span<const std::uint8_t> bytes, std::size_t at)
{
    return Word{bytes[at]} | (Word{bytes[at + 1]} << 8) | (Word{bytes[at + 2]} << 16) | (Word{bytes[at + 3]} << 24);
}
// 46CD0B, little-endian 96-bit shift. No host floating-point arithmetic.
void shift_left_46cd0b(Extended12 &value)
{
    Word carry = 0;
    for (auto &limb : value)
    {
        const Word next = (Word{limb} << 1) | carry;
        limb = static_cast<std::uint16_t>(next);
        carry = next >> 16;
    }
}
// ___add_12: carry between all three source DWORDs, expressed as WORDs.
void add12(Extended12 &value, const Extended12 &other)
{
    Word carry = 0;
    for (std::size_t i = 0; i != value.size(); ++i)
    {
        const Word sum = Word{value[i]} + other[i] + carry;
        value[i] = static_cast<std::uint16_t>(sum);
        carry = sum >> 16;
    }
}
Extended12 digits_46cd66(std::span<const std::uint8_t> digits)
{
    Extended12 value{};
    int exponent = 0x404e;
    for (const auto digit : digits)
    {
        const auto old = value;
        shift_left_46cd0b(value);
        shift_left_46cd0b(value);
        add12(value, old);
        shift_left_46cd0b(value);
        Extended12 addend{};
        addend[0] = digit;
        add12(value, addend);
    }
    while (pair(value[4], value[5]) == 0)
    {
        for (std::size_t i = 5; i != 0; --i)
            value[i] = value[i - 1];
        value[0] = 0;
        exponent -= 16;
    }
    while ((value[4] & 0x8000u) == 0)
    {
        shift_left_46cd0b(value);
        --exponent;
    }
    value[5] = static_cast<std::uint16_t>(exponent);
    return value;
}
// 46D2DB finite normal path, including the source triangular product and
// its 16-bit guard rounding. The excluded low product terms stay excluded.
void multiply_46d2db(Extended12 &a, const Extended12 &b)
{
    const Word ea = a[5] & 0x7fffu, eb = b[5] & 0x7fffu;
    const Word sign = (a[5] ^ b[5]) & 0x8000u;
    if (ea == 0 || eb == 0 || ea >= 0x7fff || eb >= 0x7fff || ea + eb < 0x3fc0 || ea + eb >= 0xbffe)
        outside_domain("46348E %f: unsupported extended product range");
    Extended12 product{};
    for (std::size_t i = 0; i != 5; ++i)
    {
        for (std::size_t j = 0; j != 5 - i; ++j)
        {
            const Word term = Word{a[i + j]} * b[4 - j];
            const Word prior = pair(product[i], product[i + 1]);
            const Word sum = prior + term; // 46CC8C DWORD add/carry
            product[i] = static_cast<std::uint16_t>(sum);
            product[i + 1] = static_cast<std::uint16_t>(sum >> 16);
            if (sum < prior)
            {
                // The top product cannot carry in this qualified domain.
                if (i + 2 == product.size())
                    outside_domain("46348E %f: unsupported top product carry");
                ++product[i + 2];
            }
        }
    }
    int exponent = static_cast<int>(ea + eb) - 0x3ffe;
    while (exponent > 0 && (product[5] & 0x8000u) == 0)
    {
        shift_left_46cd0b(product);
        --exponent;
    }
    if (exponent <= 0)
        outside_domain("46348E %f: extended subnormal outside CSV domain");
    if (product[0] > 0x8000u || (pair(product[0], product[1]) & 0x1ffffu) == 0x18000u)
    {
        Word carry = 1;
        for (std::size_t i = 1; i != 6; ++i)
        {
            const Word sum = Word{product[i]} + carry;
            product[i] = static_cast<std::uint16_t>(sum);
            carry = sum >> 16;
        }
        if (carry != 0)
        {
            product[5] = 0x8000;
            ++exponent;
        }
    }
    if (exponent > 0x7ffe)
        outside_domain("46348E %f: extended overflow outside CSV domain");
    for (std::size_t i = 0; i != 5; ++i)
        a[i] = product[i + 1];
    a[5] = static_cast<std::uint16_t>(Word(exponent) | sign);
}
void scale_46d4fb(const SourceApplicationLiteralRef &app, Extended12 &value, int power)
{
    if (power == 0)
        return;
    if (power < -7 || power > 7)
        outside_domain("46348E %f: decimal power outside original CSV domain");
    // Only the first base-8 coefficient group is reached in this domain.
    const auto bytes = app.frontend_csv_float_literal(power < 0 ? 0x496830 : 0x4966d0);
    if (bytes.size() != 84)
        outside_domain("46348E %f: coefficient App span mismatch");
    const auto index = static_cast<std::size_t>(power < 0 ? -power : power);
    const auto offset = (index - 1) * 12;
    Extended12 coefficient{};
    for (std::size_t i = 0; i != 6; ++i)
        coefficient[i] =
            static_cast<std::uint16_t>(Word{bytes[offset + 2 * i]} | (Word{bytes[offset + 2 * i + 1]} << 8));
    // param_3 == 0 at 46AEF6: clear the low WORD before scaling.
    value[0] = 0;
    if (coefficient[0] > 0x7fff)
    {
        // Source decrements ONLY the unaligned DWORD at coefficient + 2.
        const Word adjusted = pair(coefficient[1], coefficient[2]) - 1;
        coefficient[1] = static_cast<std::uint16_t>(adjusted);
        coefficient[2] = static_cast<std::uint16_t>(adjusted >> 16);
    }
    multiply_46d2db(value, coefficient);
}
Word pack_float_46ad31(const SourceApplicationLiteralRef &app, const Extended12 &value)
{
    const auto table = app.frontend_csv_float_literal(0x4962c0);
    constexpr std::array<Word, 6> expected{128, 0xffffff81u, 24, 8, 32, 127};
    if (table.size() != 24)
        outside_domain("46348E %f: packing App span mismatch");
    for (std::size_t i = 0; i != expected.size(); ++i)
        if (read32(table, 4 * i) != expected[i])
            outside_domain("46348E %f: unsupported packing parameters");
    const Word sign = Word{value[5] & 0x8000u} << 16;
    Word high = pair(value[3], value[4]);
    const Word middle = pair(value[1], value[2]);
    const Word low = Word{value[0]} << 16;
    if ((value[5] & 0x7fffu) == 0 && (high | middle | low) == 0)
        return sign;
    int exponent = static_cast<int>(value[5] & 0x7fffu) - 0x3fff;
    // Actual 46ABD6(24) -> 46AB37(25) -> 46AB80(23): guard bit 7,
    // sticky bits 0..5 and subsequent DWORDs. Bit 6 is NOT in the mask.
    // Preserve the pinned instructions; this is not a host rounding policy.
    if ((high & 0x80u) != 0 && ((high & 0x3fu) | middle | low) != 0)
    {
        const Word prior = high;
        high += 0x100u;
        if (high < prior)
            ++exponent;
    }
    high &= 0xffffff80u;
    if (exponent <= -127 || exponent >= 128)
        outside_domain("46348E %f: float32 subnormal/overflow outside CSV domain");
    high &= 0x7fffffffu;
    return sign | (Word(exponent + 127) << 23) | (high >> 8);
}
} // namespace
static SourceExtendedDecimal12 decode_digits_domain(const SourceApplicationLiteralRef &app,
                                                    std::span<const std::uint8_t> digits, int power, bool negative,
                                                    std::size_t limit)
{
    if (digits.size() > limit || power < -7 || power > 7 ||
        std::any_of(digits.begin(), digits.end(), [](auto digit) { return digit > 9; }) ||
        (!digits.empty() && digits.front() == 0))
        outside_domain("46BD0D shared decimal tail outside its bounded digit domain");
    Extended12 value{};
    if (!digits.empty())
    {
        value = digits_46cd66(digits);
        scale_46d4fb(app, value, power);
    }
    if (negative)
        value[5] |= 0x8000u;
    return value;
}
SourceExtendedDecimal12 decode_source_decimal_digits_bounded(const SourceApplicationLiteralRef &app,
                                                             std::span<const std::uint8_t> digits, int power,
                                                             bool negative)
{
    return decode_digits_domain(app, digits, power, negative, 8);
}
SourceExtendedDecimal12 decode_source_camera_digits_bounded(const SourceApplicationLiteralRef &app,
                                                            std::span<const std::uint8_t> digits, int power,
                                                            bool negative)
{
    return decode_digits_domain(app, digits, power, negative, 12);
}
std::uint32_t pack_source_frontend_float_bounded(const SourceApplicationLiteralRef &app,
                                                 const SourceExtendedDecimal12 &value)
{
    return pack_float_46ad31(app, value);
}

namespace
{
// 46AD31 stack limbs are ordered most significant first, unlike Extended12.
using Mantissa96 = std::array<Word, 3>;
bool low_zero_46ab37(const Mantissa96 &value, unsigned position)
{
    const auto index = static_cast<std::size_t>(position / 32);
    const unsigned shift = 31 - (position % 32);
    if ((value[index] & ~(Word{0xffffffffu} << shift)) != 0)
        return false;
    for (auto i = index + 1; i < value.size(); ++i)
        if (value[i] != 0)
            return false;
    return true;
}
bool add_bit_46ab80(Mantissa96 &value, unsigned position)
{
    auto index = static_cast<std::size_t>(position / 32);
    Word increment = Word{1} << (31 - (position % 32));
    for (;;)
    {
        const Word previous = value[index];
        value[index] += increment; // 46CC8C: DWORD addition followed by carry.
        if (value[index] >= previous)
            return false;
        if (index == 0)
            return true;
        --index;
        increment = 1;
    }
}
bool round_46abd6(Mantissa96 &value, unsigned precision)
{
    const auto index = static_cast<std::size_t>(precision / 32);
    const unsigned shift = 31 - (precision % 32);
    bool carry = false;
    if ((value[index] & (Word{1} << shift)) != 0 && !low_zero_46ab37(value, precision + 1))
        carry = add_bit_46ab80(value, precision - 1);
    value[index] &= Word{0xffffffffu} << shift;
    for (auto i = index + 1; i < value.size(); ++i)
        value[i] = 0;
    return carry;
}
void shift_right_46aca4(Mantissa96 &value, unsigned amount)
{
    // 46ACA4 first shifts each DWORD with carry, then shifts whole DWORDs.
    // The source masks x86 shift counts; avoid a C++ shift by32 when bits==0.
    const unsigned bits = amount % 32;
    Word carry = 0;
    if (bits != 0)
        for (auto &limb : value)
        {
            const Word previous = limb;
            limb = (previous >> bits) | carry;
            carry = previous << (32 - bits);
        }
    const auto words = static_cast<std::size_t>(amount / 32);
    for (auto i = value.size(); i != 0;)
    {
        --i;
        value[i] = i < words ? 0 : value[i - words];
    }
}
} // namespace
SourcePackedBinary64 pack_source_binary64_46ae9d(const SourceApplicationLiteralRef &app,
                                                 const SourceExtendedDecimal12 &value)
{
    const auto table = app.frontend_csv_float_literal(0x4962a8);
    constexpr std::array<Word, 6> expected{1024, 0xfffffc01u, 53, 11, 64, 1023};
    if (table.size() != 24)
        outside_domain("46AE9D: packing App span mismatch");
    for (std::size_t i = 0; i < expected.size(); ++i)
        if (read32(table, 4 * i) != expected[i])
            outside_domain("46AE9D: unsupported packing parameters");
    const int maximum = static_cast<int>(read32(table, 0));
    const int minimum = std::bit_cast<std::int32_t>(read32(table, 4));
    const unsigned precision = read32(table, 8);
    const unsigned exponent_bits = read32(table, 12);
    const int bias = static_cast<int>(read32(table, 20));
    const Word source_exponent = value[5] & 0x7fffu;
    int exponent = static_cast<int>(source_exponent) - 0x3fff;
    Mantissa96 mantissa{pair(value[3], value[4]), pair(value[1], value[2]), Word{value[0]} << 16};
    Word status = 0;
    if (source_exponent == 0)
    { // 46AD78..46AD99: zero versus extended denormal.
        exponent = 0;
        if ((mantissa[0] | mantissa[1] | mantissa[2]) != 0)
        {
            mantissa = {};
            status = 2;
        }
    }
    else
    {
        const auto original = mantissa; // 46ADA6 saves before rounding.
        if (round_46abd6(mantissa, precision))
            ++exponent;
        if (exponent < minimum - static_cast<int>(precision))
        {
            mantissa = {};
            exponent = 0;
            status = 2; // 46ADCB..46ADD5,46AE13.
        }
        else if (exponent <= minimum)
        {
            mantissa = original; // 46ADD7..46AE13: round the original denormal.
            shift_right_46aca4(mantissa, static_cast<unsigned>(minimum - exponent));
            (void)round_46abd6(mantissa, precision);
            shift_right_46aca4(mantissa, exponent_bits + 1);
            exponent = 0;
            status = 2;
        }
        else if (exponent >= maximum)
        {
            mantissa = {0x80000000u, 0, 0}; // 46AE1E..46AE41.
            shift_right_46aca4(mantissa, exponent_bits);
            exponent = bias + maximum;
            status = 1;
        }
        else
        {
            mantissa[0] &= 0x7fffffffu; // 46AE46..46AE5D.
            exponent += bias;
            shift_right_46aca4(mantissa, exponent_bits);
        }
    }
    const Word sign = Word{value[5] & 0x8000u} << 16;
    mantissa[0] |= (static_cast<Word>(exponent) << (31 - exponent_bits)) | sign;
    return {(std::uint64_t{mantissa[0]} << 32) | mantissa[1], status}; // 46AE5F..46AE9C.
}
} // namespace sgn::retail
