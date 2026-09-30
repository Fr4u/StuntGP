#include "sgn/source_decimal_conversion.hpp"
#include "sgn/source_application_literals.hpp"
#include "sgn/source_native_crt.hpp"
#include "sgn/race_session.hpp"
#include "sgn/source_script_search.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
struct BoundedDecimal
{
    SourceExtendedDecimal12 value;
    Word end_offset;
    Word flags;
};
[[noreturn]] void outside_domain(const char *reason)
{
    throw std::domain_error(reason);
}

// Ordinary-decimal path of46BD0D with param4/5/6/7 all zero. Unlike scanf,
// this receiver sees the original suffix, including the source D/d exponent.
// Other46BD0D states are explicitly outside this bounded receiver's domain.
BoundedDecimal decode_atof_decimal_bounded(const SourceApplicationLiteralRef &app, std::span<const std::uint8_t> input,
                                           bool camera_domain)
{
    const auto point = app.read_frontend_decimal_point();
    if (point != '.')
        outside_domain("4631A7 bounded atof: non-original decimal locale");
    // input includes its NUL, so digit classification also observes the
    // terminator in the source digit loops. A modified ctype may not eat it.
    std::size_t cursor = 0;
    const auto character = [&]() { return input[cursor]; };
    const auto advance = [&]() {
        if (cursor + 1 == input.size())
            outside_domain("4631A7 bounded atof: parser would leave source NUL");
        ++cursor;
    };
    while (character() == ' ' || character() == '\t' || character() == '\n' || character() == '\r')
        advance();
    bool negative = false;
    std::array<std::uint8_t, 12> digits{};
    const auto digit_limit = camera_domain ? 12u : 8u;
    std::size_t count = 0;
    int fractional = 0;
    const auto fractional_place = [&]() {
        if (++fractional > (camera_domain ? 6 : 5))
            outside_domain("4631A7 bounded atof: fractional places outside this caller domain");
    };
    const auto digit_run = [&](bool after_point) {
        while (app.classify_number_4631f4(character(), 4) != 0)
        {
            const auto current = character();
            if (current < '0' || current > '9')
                outside_domain("4631A7 bounded atof: non-ASCII digit");
            if (after_point)
                fractional_place();
            if (count == digit_limit)
                outside_domain("4631A7 bounded atof: significant digits outside this caller domain");
            digits[count++] = static_cast<std::uint8_t>(current - '0');
            advance();
        }
    };
    // States0/2 use literal ASCII checks, state1 skips literal zeroes;
    // only states3/4/5 use the live CRT digit table. Preserve that distinction.
    bool signed_prefix = false;
    if (character() == '+' || character() == '-')
    {
        negative = character() == '-';
        signed_prefix = true;
        advance();
    }
    bool accepted = false;
    if (character() == '0')
    {
        accepted = true; // state1, local14=1 even if no significant digit.
        do
        {
            advance();
        } while (character() == '0');
    }
    if (character() >= '1' && character() <= '9')
    {
        accepted = true; // state3 sets local14 before the digit-table loop.
        digit_run(false);
    }
    if (character() == point)
    {
        advance();
        // state5 (point with no previous digit) classifies once before
        // entering state4, which classifies again after skipping zeroes.
        if (!accepted && app.classify_number_4631f4(character(), 4) == 0)
            return {{}, 0, 4};
        accepted = true;
        if (count == 0)
            while (character() == '0')
            {
                fractional_place();
                advance();
            }
        digit_run(true);
    }
    if (!accepted)
        return {{}, signed_prefix ? 0 : static_cast<Word>(cursor), 4};
    if (character() == 'D' || character() == 'E' || character() == 'd' || character() == 'e')
        outside_domain("4631A7 bounded atof: exponent syntax requires the remaining46BD0D states");
    int power = -fractional;
    while (count != 0 && digits[count - 1] == 0)
    {
        --count;
        ++power;
    }
    const auto decode = camera_domain ? decode_source_camera_digits_bounded : decode_source_decimal_digits_bounded;
    return {decode(app, std::span<const std::uint8_t>{digits.data(), count}, power, negative),
            static_cast<Word>(cursor), 0};
}
} // namespace
static std::uint64_t publish_atof_result(NativeSourceMenuCrt &crt, const SourceApplicationLiteralRef &app,
                                         const BoundedDecimal &decoded)
{
    Word status = 0;
    std::uint64_t result = 0;
    if ((decoded.flags & 4u) != 0)
        status = 0x200; // 4681DC..4681EC.
    else
    {
        const auto packed = pack_source_binary64_46ae9d(app, decoded.value);
        result = packed.bits;
        if ((decoded.flags & 2u) != 0 || packed.status == 1)
            status = 0x80;
        if ((decoded.flags & 1u) != 0 || packed.status == 2)
            status |= 0x100;
    }
    // Exact source pointer rereads. No second result cache, no errno write;
    // +8 and+C are untouched. Both FSTP words use the same instruction site.
    const Word status_pointer = crt.read_word(0x495da8, 0x46821c);
    crt.write_word(status_pointer, status, 0x468221);
    const Word count_pointer = crt.read_word(0x495da8, 0x468229);
    crt.write_word(count_pointer + 4, decoded.end_offset, 0x468231);
    const Word result_pointer = crt.read_word(0x495da8, 0x468234);
    crt.write_word(result_pointer + 0x10, static_cast<Word>(result), 0x46823d);
    crt.write_word(result_pointer + 0x14, static_cast<Word>(result >> 32), 0x46823d);
    const Word returned_pointer = crt.read_word(0x495da8, 0x468240);
    const Word low = crt.read_word(returned_pointer + 0x10, 0x4631ec);
    const Word high = crt.read_word(returned_pointer + 0x14, 0x4631ec);
    return (std::uint64_t{high} << 32) | low;
}

static std::uint64_t convert_atof_domain(RaceSessionPlatform &platform, NativeSourceMenuCrt &crt,
                                         std::span<const std::uint8_t> written_local, bool camera_domain)
{
    platform.require_owner_thread();
    crt.require_platform(platform);
    const auto app = platform.source_application_literals();
    // NEW caller-local transport guard. No recovery/default value on failure.
    if (written_local.size() > std::numeric_limits<Word>::max())
        outside_domain("4631A7 bounded atof: input exceeds source32-bit pointer domain");
    const auto nul = std::find(written_local.begin(), written_local.end(), std::uint8_t{0});
    if (nul == written_local.end())
        outside_domain("4631A7 bounded atof: strlen would leave written local input");
    const auto length = static_cast<std::size_t>(nul - written_local.begin());
    if (std::any_of(written_local.begin(), nul, [](auto character) { return character >= 0x80; }))
        outside_domain("4631A7 bounded atof: non-ASCII input requires unbounded CRT parser");
    std::size_t start = 0;
    for (;;)
    { // 4631AC..4631D8: actual same App width/table, including the NUL.
        const bool multibyte = app.number_uses_multibyte_4631f4(8);
        if (app.classify_number_4631f4(written_local[start], 8, multibyte) == 0)
            break;
        if (start == length)
            outside_domain("4631A7 bounded atof: ctype classifies source NUL as whitespace");
        ++start;
    }
    // 4631DF strlen is represented by the bounded NUL above.4681B6 receives
    // the trimmed pointer; its consumed count excludes outer whitespace.
    const auto decoded =
        decode_atof_decimal_bounded(app, written_local.subspan(start, length - start + 1), camera_domain);
    return publish_atof_result(crt, app, decoded);
}

std::uint64_t convert_source_local_atof_bounded_4631a7(RaceSessionPlatform &platform, NativeSourceMenuCrt &crt,
                                                       std::span<const std::uint8_t> input)
{
    return convert_atof_domain(platform, crt, input, false);
}
std::uint64_t convert_source_camera_atof_bounded_4631a7(RaceSessionPlatform &platform, NativeSourceMenuCrt &crt,
                                                        SourceBootSearchBoundary input)
{
    platform.require_owner_thread();
    crt.require_platform(platform);
    const auto app = platform.source_application_literals();
    const auto read = [&](Word at, Word site) {
        auto p = input;
        p.offset = at;
        return read_source_script_byte(platform, p, site);
    };
    const auto word = [&](Word at, Word site) {
        if (input.is_null)
            throw SourceBootConfigError(site, "camera strlen reaches an unowned source address");
        try
        {
            const auto bytes = platform.source_allocations().bytes(input.allocation);
            if (at > bytes.size() || bytes.size() - at < 4)
                throw SourceBootConfigError(site, "camera strlen DWORD outside source allocation");
            return Word(bytes[at]) | (Word(bytes[at + 1]) << 8) | (Word(bytes[at + 2]) << 16) |
                   (Word(bytes[at + 3]) << 24);
        }
        catch (const SourceAllocationError &error)
        {
            throw SourceBootConfigError(site, error.what());
        }
    };
    Word start = input.offset;
    for (;;)
    { // 4631AC width read precedes each source character, even on NULL+1
        const bool wide = app.number_uses_multibyte_4631f4(8);
        const auto byte = read(start, wide ? 0x4631b5u : 0x4631c4u);
        if (app.classify_number_4631f4(byte, 8, wide) == 0)
            break;
        ++start;
    }
    // Full468250 BYTE prologue and DWORD/re-read strlen, bank alignment32.
    Word cursor = start, length = 0;
    bool found = false;
    while ((cursor & 3u) != 0)
    {
        const auto byte = read(cursor, 0x46825c);
        ++cursor;
        if (byte == 0)
        {
            length = cursor - 1u - start;
            found = true;
            break;
        }
    }
    while (!found)
    {
        const auto value = word(cursor, 0x468270);
        const auto candidate = (~value) ^ (value + 0x7efefeffu);
        cursor += 4u;
        if ((candidate & 0x81010100u) == 0)
            continue;
        const auto reread = word(cursor - 4u, 0x468288);
        for (Word lane = 0; lane < 4; ++lane)
            if (((reread >> (8u * lane)) & 255u) == 0)
            {
                length = cursor - (4u - lane) - start;
                found = true;
                break;
            }
    }
    const auto bytes = platform.source_allocations().bytes(input.allocation);
    if (start > bytes.size() || std::uint64_t(length) + 1u > bytes.size() - start)
        throw SourceBootConfigError(0x4681d2, "camera atof string crosses its source allocation");
    const auto suffix = std::span<const std::uint8_t>(bytes).subspan(start, std::size_t(length) + 1);
    // Bounded lexical domain, checked after the source strlen. The complete
    // retail CAM suffixes are ASCII; no fake replacement/decimal fallback.
    if (std::any_of(suffix.begin(), suffix.end(), [](auto c) { return c >= 128; }))
        outside_domain("camera4631A7: non-ASCII requires remaining CRT lexical states");
    const auto decoded = decode_atof_decimal_bounded(app, suffix, true);
    return publish_atof_result(crt, app, decoded);
}
} // namespace sgn::retail
