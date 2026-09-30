#include "sgn/retail_gameplay_configuration.hpp"
#include "sgn/retail_gameplay_tables.hpp"
#include "sgn/retail_csv.hpp"
#include "sgn/format_require.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <numeric>

namespace sgn::retail
{
namespace
{
using sgn::format_check::require;
template <class Consumer> void rows(std::string_view text, Consumer consume)
{
    require(text.size() <= 1024 * 1024 && text.find('\0') == text.npos, "invalid rules text");
    std::size_t at = 0;
    auto name = read_retail_csv_token(text, at, 64);
    while (!name.empty())
    {
        const auto value = read_retail_csv_token(text, at, 320);
        if (value.empty())
            break;
        skip_retail_csv_line(text, at);
        consume(name, value);
        name = read_retail_csv_token(text, at, 64);
    }
}
bool budget(std::uint32_t &remaining)
{
    remaining -= 4;
    return std::bit_cast<std::int32_t>(remaining) >= 0;
}
double source_number(std::string_view text)
{
    while (!text.empty() && (text.front() == ' ' || (text.front() >= '\t' && text.front() <= '\r')))
        text.remove_prefix(1);
    if (!text.empty() && text.front() == '+')
        text.remove_prefix(1);
    double value{};
    const auto converted = std::from_chars(text.data(), text.data() + text.size(), value, std::chars_format::general);
    if (converted.ec == std::errc::invalid_argument)
        return 0;
    require(converted.ec == std::errc{} && std::isfinite(value), "rules number outside finite conversion domain");
    return value;
}
std::uint32_t source_integer(std::string_view text)
{
    while (!text.empty() && (text.front() == ' ' || (text.front() >= '\t' && text.front() <= '\r')))
        text.remove_prefix(1);
    bool negative = false;
    if (!text.empty() && (text.front() == '+' || text.front() == '-'))
    {
        negative = text.front() == '-';
        text.remove_prefix(1);
    }
    std::uint32_t value = 0;
    for (char c : text)
    {
        if (c < '0' || c > '9')
            break;
        value = value * 10u + static_cast<unsigned>(c - '0');
    }
    return negative ? 0u - value : value;
}
template <std::size_t N, class Consumer>
void apply(std::string_view text, std::array<std::uint32_t, N> &order,
           const std::array<gameplay_tables::Rule, N> &definitions, Consumer consume)
{
    require(std::all_of(order.begin(), order.end(), [](auto id) { return id < N; }), "rules descriptor identity");
    std::size_t remaining = N;
    rows(text, [&](std::string_view name, std::string_view value) {
        for (std::size_t i = 0; i < remaining; ++i)
            if (name == definitions[order[i]].name)
            {
                consume(definitions[order[i]], value);
                --remaining;
                for (std::size_t j = i; j < remaining; ++j)
                    order[j] = order[j + 1];
                return;
            }
    });
}
std::string_view string(std::span<const std::uint8_t> bytes)
{
    return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}
} // namespace
GameplayRules::GameplayRules()
{
    std::iota(descriptor_order.begin(), descriptor_order.end(), 0u);
}
ScoreRules::ScoreRules()
{
    std::iota(descriptor_order.begin(), descriptor_order.end(), 0u);
}
void load_gameplay_text(std::string_view text, GameplayRules &state)
{
    state.values.fill(0);
    apply(text, state.descriptor_order, gameplay_tables::gameplay, [&](const auto &rule, std::string_view value) {
        const double parsed = budget(state.remaining_bytes) ? source_number(value) : 0.0;
        const auto scaled = static_cast<float>(parsed * double(std::bit_cast<float>(rule.scale_bits)));
        require(std::isfinite(scaled), "scaled rule outside binary32 domain");
        state.values[rule.index] = scaled;
    });
    state.pending = 0;
}
void load_score_text(std::string_view text, ScoreRules &state)
{
    state.values.fill(0);
    apply(text, state.descriptor_order, gameplay_tables::scores, [&](const auto &rule, std::string_view value) {
        state.values[rule.index] = budget(state.remaining_bytes) ? source_integer(value) : 0;
    });
    state.pending = 0;
}
void load_gameplay_configuration(const AssetStore &assets, GameplayRules &state)
{
    load_gameplay_text(string(assets.resource("Config\\Gameplay.csv")), state);
}
void load_score_configuration(const AssetStore &assets, ScoreRules &state)
{
    load_score_text(string(assets.resource("Config\\Scores.csv")), state);
}
void ensure_gameplay_configuration(const AssetStore &assets, GameplayRules &state)
{
    if (state.pending != 0)
        load_gameplay_configuration(assets, state);
}
void ensure_score_configuration(const AssetStore &assets, ScoreRules &state)
{
    if (state.pending != 0)
        load_score_configuration(assets, state);
}
} // namespace sgn::retail
