#include "sgn/contact_config.hpp"
#include "sgn/assets.hpp"
#include <charconv>
#include <cmath>

namespace sgn::retail::contact
{
std::array<float, 128> decode_contact_restitution(std::string_view text)
{
    if (text.empty() || text.size() > 1024 * 1024 || text.find('\0') != text.npos)
        throw FormatError("invalid level configuration text");
    constexpr std::string_view key = "-ColRestitutionTab[]";
    unsigned matches = 0;
    std::string_view source;
    for (std::size_t line = 0; line < text.size();)
    {
        auto end = text.find('\n', line);
        if (end == text.npos)
            end = text.size();
        auto row = text.substr(line, end - line);
        auto start = row.find_first_not_of(" \t\r");
        if (start != row.npos)
        {
            row.remove_prefix(start);
            if (row.starts_with(key))
            {
                auto tail = row.substr(key.size());
                const auto equal = tail.find_first_not_of(" \t");
                if (equal != tail.npos && tail[equal] == '=')
                {
                    ++matches;
                    source = text.substr(line + start + key.size() + equal + 1);
                }
            }
        }
        line = end + 1;
    }
    if (matches != 1)
        throw FormatError("missing or duplicate ColRestitutionTab");
    const auto skip = [&]() {
        const auto first = source.find_first_not_of(" \t\r\n,");
        if (first == source.npos)
            source = {};
        else
            source.remove_prefix(first);
    };
    std::array<float, 128> result{};
    for (auto &value : result)
    {
        skip();
        if (source.empty())
            throw FormatError("short ColRestitutionTab");
        if (source.front() == '+')
            source.remove_prefix(1);
        double parsed{};
        const auto [end, error] =
            std::from_chars(source.data(), source.data() + source.size(), parsed, std::chars_format::general);
        if (error != std::errc{} || !std::isfinite(parsed))
            throw FormatError("invalid ColRestitutionTab number");
        if (end != source.data() + source.size() && *end != ' ' && *end != '\t' && *end != '\r' && *end != '\n' &&
            *end != ',')
            throw FormatError("invalid ColRestitutionTab suffix");
        value = static_cast<float>(parsed);
        if (!std::isfinite(value))
            throw FormatError("ColRestitutionTab number outside float range");
        source.remove_prefix(static_cast<std::size_t>(end - source.data()));
    }
    skip();
    auto remaining = source;
    if (!remaining.empty() && (remaining.front() == '-' || remaining.front() == '+'))
        remaining.remove_prefix(1);
    if (!remaining.empty() && ((remaining.front() >= '0' && remaining.front() <= '9') || remaining.front() == '.'))
        throw FormatError("excess ColRestitutionTab samples");
    return result;
}
} // namespace sgn::retail::contact
