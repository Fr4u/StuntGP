#include "sgn/contact_ai_stunts.hpp"
#include <charconv>
#include <cmath>
#include <string_view>

namespace sgn::retail
{
AiStuntKnots decode_ai_stunt_knots(std::span<const std::uint8_t> bytes)
{
    const std::string_view text(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    if (text.find('\0') != std::string_view::npos)
        throw FormatError("NUL in AI stunt configuration");
    auto at = text.find("-CarAI_StuntKnotList[] =");
    if (at == std::string_view::npos)
        throw FormatError("missing authored AI stunt knot list");
    at += 16;
    AiStuntKnots result{};
    unsigned count = 0;
    const auto number = [&](std::string_view block, std::string_view key) {
        auto begin = block.find(key);
        if (begin == std::string_view::npos)
            throw FormatError("missing AI stunt knot field");
        begin = block.find('=', begin);
        if (begin == std::string_view::npos)
            throw FormatError("missing AI stunt knot equals");
        ++begin;
        while (begin < block.size() &&
               (block[begin] == ' ' || block[begin] == '\t' || block[begin] == '\r' || block[begin] == '\n'))
            ++begin;
        if (begin < block.size() && block[begin] == '+')
            ++begin;
        double value{};
        const auto conversion =
            std::from_chars(block.data() + begin, block.data() + block.size(), value, std::chars_format::general);
        if (conversion.ec != std::errc{} || !std::isfinite(value))
            throw FormatError("invalid AI stunt knot number");
        if (conversion.ptr < block.data() + block.size() && *conversion.ptr != ' ' && *conversion.ptr != '\t' &&
            *conversion.ptr != '\r' && *conversion.ptr != '\n')
            throw FormatError("invalid AI stunt knot number suffix");
        return value;
    };
    while ((at = text.find("-PathPosition", at)) != std::string_view::npos)
    {
        if (count >= 32)
            throw FormatError("AI stunt knot list lacks space for zero terminator");
        const auto next = text.find("-PathPosition", at + 16),
                   end = next == std::string_view::npos ? text.size() : next;
        const auto block = text.substr(at, end - at);
        auto &knot = result[count++];
        knot.progress = vehicle_f32(number(block, "-PathPosition"));
        knot.width = vehicle_f32(number(block, "-Width"));
        const auto grade = number(block, "-Grade");
        if (grade < 0 || grade > 2 || std::floor(grade) != grade)
            throw FormatError("AI stunt grade outside authored 0..2 domain");
        knot.grade = static_cast<std::uint32_t>(grade);
        if (!std::isfinite(knot.progress) || knot.progress <= 0 || !std::isfinite(knot.width) || knot.width < 0)
            throw FormatError("AI stunt knot coordinates outside authored domain");
        at += 16;
    }
    return result;
}
} // namespace sgn::retail
