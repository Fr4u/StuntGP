#include "sgn/retail_level_curves.hpp"
#include "sgn/assets.hpp"
#include "sgn/format_require.hpp"
#include <bit>
#include <charconv>
#include <cmath>
#include <span>

namespace sgn::retail
{
namespace
{
using sgn::format_check::require;
void load(std::string_view text, std::string_view key, std::span<float> destination)
{
    // Source434A76/81 uses the first substring match, then the following '='.
    // Token advancement searches for literal spaces, skipping CR/LF only after
    // that space. It does not treat tabs or commas as equivalent separators.
    auto at = text.find(key);
    require(at != text.npos, "missing level curve");
    at = text.find('=', at);
    require(at != text.npos, "level curve lacks equals sign");
    at = text.find(' ', at);
    require(at != text.npos, "level curve lacks literal-space separator");
    for (auto &value : destination)
    {
        while (at < text.size() && (text[at] == ' ' || text[at] == '\r' || text[at] == '\n'))
            ++at;
        require(at < text.size(), "short level curve");
        const auto end = text.find(' ', at);
        require(end != text.npos && end > at, "unterminated level curve value");
        auto number_end = end;
        if (text[number_end - 1] == ',')
            --number_end;
        auto first = at;
        if (text[first] == '+')
            ++first;
        require(first < number_end, "empty level curve value");
        double parsed{};
        const auto result =
            std::from_chars(text.data() + first, text.data() + number_end, parsed, std::chars_format::general);
        require(result.ec == std::errc{} && result.ptr == text.data() + number_end && std::isfinite(parsed),
                "invalid level curve number");
        value = static_cast<float>(parsed);
        require(std::isfinite(value), "level curve outside binary32 range");
        at = end;
    }
}
} // namespace
void apply_level_curves(std::string_view text, LevelCurveTables &tables)
{
    require(!text.empty() && text.size() <= 1024 * 1024 && text.find('\0') == text.npos,
            "invalid level configuration text");
    // Fixed source order. An unsuccessful bounded parse is not a source state
    // transition; callers discard that failed level load.
    load(text, "-FogEnvelope[]", tables.fog);
    load(text, "-ColRestitutionTab[]", tables.restitution);
    load(text, "-FarPlaneClipDistanceTab[]", tables.far_clip);
    load(text, "-CarReplacementSplineMask[]", tables.respawn);
    load(text, "-CarLightsMask[]", tables.lights);
    std::array<float, 128> ambience;
    load(text, "-AmbienceEnvelope[]", ambience);
    for (unsigned i = 0; i < ambience.size(); ++i)
        tables.ambience[i] = std::bit_cast<std::uint32_t>(ambience[i]);
    load(text, "-TrackWidthEnvelope_Left[]", tables.width_left);
    load(text, "-TrackWidthEnvelope_Right[]", tables.width_right);
}
} // namespace sgn::retail
