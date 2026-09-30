#include "sgn/retail_level.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
void check(bool good, const char *message)
{
    if (!good)
        throw std::domain_error(message);
}
void valid(LevelBlock block)
{
    check(block.end <= block.text.size(), "level end boundary outside input");
    check(block.text.find('\0') == std::string_view::npos, "embedded NUL in level script");
}
std::optional<std::size_t> key(LevelBlock block, std::string_view name, std::size_t begin = 0)
{
    const auto at = block.text.find(name, begin);
    if (at == std::string_view::npos || at > block.end)
        return std::nullopt;
    const auto equal = block.text.find('=', at);
    check(equal != std::string_view::npos, "level key has no following equals sign");
    const auto space = block.text.find(' ', equal);
    check(space != std::string_view::npos, "level key has no following literal space");
    return space;
}
bool whitespace(char c)
{
    return c == ' ' || c == '\r' || c == '\n';
}
void skip(std::string_view text, std::size_t &at)
{
    while (at < text.size() && whitespace(text[at]))
        ++at;
    check(at < text.size(), "unterminated level value");
}
std::string_view token(std::string_view text, std::size_t &at)
{
    skip(text, at);
    const auto start = at;
    while (at < text.size() && !whitespace(text[at]))
        ++at;
    check(at < text.size(), "unterminated level string");
    const auto value = text.substr(start, at - start);
    ++at;
    return value;
}
std::uint32_t atoi32(std::string_view text)
{
    std::size_t i = 0;
    while (i < text.size() && (text[i] == ' ' || (text[i] >= '\t' && text[i] <= '\r')))
        ++i;
    bool negative = false;
    if (i < text.size() && (text[i] == '+' || text[i] == '-'))
        negative = text[i++] == '-';
    std::uint32_t result = 0;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9')
        result = result * 10u + std::uint32_t(text[i++] - '0');
    return negative ? 0u - result : result;
}
std::uint32_t number(std::string_view text, std::size_t &at, bool next = false)
{
    skip(text, at);
    const auto result = atoi32(text.substr(at));
    if (next)
    {
        const auto space = text.find(' ', at);
        check(space != std::string_view::npos, "level number lacks literal-space continuation");
        at = space + 1;
    }
    return result;
}
bool equals_ignore_case(std::string_view a, std::string_view b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        const auto upper = [](char c) { return c >= 'a' && c <= 'z' ? char(c - ('a' - 'A')) : c; };
        if (upper(a[i]) != upper(b[i]))
            return false;
    }
    return true;
}
} // namespace
std::string_view LevelSettings::texture_name() const
{
    const auto end = std::find(track_texture.begin(), track_texture.end(), '\0');
    check(end != track_texture.end(), "level texture name lacks terminator");
    return {track_texture.data(), static_cast<std::size_t>(end - track_texture.begin())};
}
std::optional<LevelBlock> find_level_block(std::string_view script, std::string_view label)
{
    valid({script, script.size()});
    check(!label.empty() && label.find('\0') == std::string_view::npos, "invalid level label");
    const auto at = key({script, script.size()}, label);
    if (!at)
        return std::nullopt;
    const auto end = script.find("-END", *at);
    check(end != std::string_view::npos, "level block has no -END boundary");
    return LevelBlock{script.substr(*at), end - *at};
}
std::vector<std::string> discover_level_blocks(std::string_view script)
{
    valid({script, script.size()});
    std::vector<std::string> result;
    std::size_t begin = 0;
    bool comment = false;
    while (begin < script.size())
    {
        const auto newline = script.find('\n', begin),
                   end = newline == std::string_view::npos ? script.size() : newline;
        const auto line = script.substr(begin, end - begin);
        std::string clean;
        for (std::size_t i = 0; i < line.size(); ++i)
        {
            if (!comment && i + 1 < line.size() && line.substr(i, 2) == "/*")
            {
                comment = true;
                ++i;
                continue;
            }
            if (comment && i + 1 < line.size() && line.substr(i, 2) == "*/")
            {
                comment = false;
                ++i;
                continue;
            }
            if (!comment)
                clean.push_back(line[i]);
        }
        const auto first = clean.find_first_not_of(" \r\t"), equal = clean.find('=');
        if (first != std::string::npos && equal != std::string::npos && first < equal)
        {
            const auto last = clean.find_last_not_of(" \r\t", equal - 1);
            auto name = clean.substr(first, last - first + 1);
            if (!name.empty() && name[0] == '-')
                name.erase(0, 1);
            const auto after = clean.find_first_not_of(" \r\t", equal + 1);
            if (after == std::string::npos && name != "END" && name.find_first_of(" \t") == std::string::npos)
                result.push_back(std::move(name));
        }
        if (newline == std::string_view::npos)
            break;
        begin = newline + 1;
    }
    return result;
}
void apply_level_settings(LevelBlock block, LevelSettings &state)
{
    valid(block);
    if (auto at = key(block, "TRACKTEXSET"))
    {
        const auto value = token(block.text, *at);
        check(value.size() < state.track_texture.size(), "level texture string exceeds original buffer");
        check(value.find('%') == std::string_view::npos, "format directive in original texture format-string argument");
        std::copy(value.begin(), value.end(), state.track_texture.begin());
        state.track_texture[value.size()] = 0;
        if (value.size() > 2)
            state.texture_suffix = atoi32(value.substr(value.size() - 2));
    }
    else
        state.track_texture[0] = 0;
    if (auto at = key(block, "MUS_TRACK"))
        state.music_track = number(block.text, *at);
    state.ground_level = -2000.0f;
    if (auto at = key(block, "GROUNDLEVEL"))
        state.ground_level = static_cast<float>(double(std::bit_cast<std::int32_t>(number(block.text, *at))) * 100.0);
    if (auto at = key(block, "GAMETYPE"))
    {
        const auto value = token(block.text, *at);
        check(value.size() < 128, "game type exceeds original temporary buffer");
        state.game_type = 2;
        if (equals_ignore_case(value, "CHALLENGE_TYPE_1"))
            state.game_type = 3;
        else if (equals_ignore_case(value, "TIMETRIAL_TYPE_1"))
            state.game_type = 4;
    }
    state.time_ticks = 0;
    if (auto at = key(block, "TIME"))
        state.time_ticks = number(block.text, *at) * 60u;
    if (auto at = key(block, "LAPCOUNT"))
        state.lap_count = std::min(number(block.text, *at), 24u);
    if (auto at = key(block, "LAPRECORD"))
    {
        const auto minutes = number(block.text, *at, true), seconds = number(block.text, *at, true),
                   hundredths = number(block.text, *at, true);
        state.lap_record_ticks = minutes * 3600u + (seconds % 100u) * 60u + ((hundredths % 100u) * 60u) / 100u;
    }
    state.rolling_start_offset = 60000;
    if (auto at = key(block, "ROLSTARTOFFSET"))
    {
        // 0042cecc FILD zero-extended qword, FMUL 100, then __ftol. No FST to
        // binary32 occurs; integer product fits binary53 and EAX is its low word.
        state.rolling_start_offset = number(block.text, *at) * 100u;
    }
    state.overlay_demo = 0;
    if (auto at = key(block, "OVERLAYDEMO"))
        state.overlay_demo = number(block.text, *at);
    state.camera_mode = 1;
    if (auto at = key(block, "CAMERAMODE"))
        state.camera_mode = number(block.text, *at);
    state.demo_loop_ticks = 3600;
    if (auto at = key(block, "R_DEMOLOOP"))
    {
        const auto value = number(block.text, *at);
        state.demo_loop_ticks = value ? value * 60u : 0xffffffffu;
    }
}
std::array<LevelCarSettings, 6> parse_level_car_settings(LevelBlock block)
{
    valid(block);
    std::array<LevelCarSettings, 6> result{};
    for (std::size_t i = 0; i < 6; ++i)
    {
        auto &car = result[i];
        car = {1, 0, 0, 0, 100, 100};
        const auto prefix = "L_CAR" + std::to_string(i + 1);
        const std::array<std::pair<std::string_view, std::uint32_t *>, 6> fields = {{{"_CONFIG", &car.config},
                                                                                     {"_TYRE", &car.tyre},
                                                                                     {"_LIVERY", &car.livery},
                                                                                     {"_REPLAY", &car.replay},
                                                                                     {"_AISKILL", &car.ai_skill},
                                                                                     {"_AIMODE", &car.ai_mode}}};
        for (const auto &[suffix, field] : fields)
            if (auto at = key(block, prefix + std::string(suffix)))
                *field = number(block.text, *at);
    }
    return result;
}
LevelAssetBindings parse_level_asset_bindings(LevelBlock block, bool include_script_cars)
{
    valid(block);
    LevelAssetBindings result;
    std::size_t cursor = 0;
    auto bind = [&](std::string_view name, std::uint32_t type) {
        const auto at = key(block, name, cursor);
        if (!at)
            return false;
        cursor = *at;
        const auto value = token(block.text, cursor);
        check(value.size() < 128, "level asset name exceeds bounded new interface");
        result.requests.push_back({std::string(value), type});
        return true;
    };
    bind("L_SKY", 2);
    if (bind("L_TRACK", 3))
        result.track_name = result.requests.back().name;
    if (include_script_cars)
    {
        cursor = 0;
        for (std::uint32_t slot = 0; slot < 6; ++slot)
        {
            const auto name = "L_CAR" + std::to_string(slot + 1);
            if (!bind(name, 4u + slot * 2u))
                break;
            check(bind(name + "SHADOW", 5u + slot * 2u), "script car lacks required shadow binding");
        }
    }
    return result;
}
} // namespace sgn::retail
