#include "sgn/retail_frontend_language_selection.hpp"
#include "sgn/retail_frontend_labels.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace sgn::retail
{
namespace
{
bool horizontal(std::uint8_t c)
{
    return c == ' ' || c == '\t';
}
bool newline(std::uint8_t c)
{
    return c == '\r' || c == '\n';
}
struct Reader
{
    std::span<const std::uint8_t> input;
    std::uint32_t cursor{};
    bool unterminated_quote{};
    std::uint8_t current() const
    {
        return cursor == input.size() ? 0 : input[cursor];
    }
    std::uint32_t token(std::span<std::uint8_t> out)
    {
        unterminated_quote = false;
        out[0] = 0;
        while (horizontal(current()))
            ++cursor;
        if (current() == 0)
            return 0;
        std::uint32_t size = 0;
        auto copy = [&] {
            if (size == out.size())
                throw std::invalid_argument("frontend selection token overflow");
            out[size++] = current();
            ++cursor;
        };
        if (current() == '"')
        {
            ++cursor;
            while (current() != '"' && current() != 0)
                copy();
            if (current() != '"')
            {
                unterminated_quote = true;
                return 0;
            }
            ++cursor;
        }
        else
            while (current() != ',' && !newline(current()) && current() != 0)
                copy();
        if (size == out.size())
            throw std::invalid_argument("frontend selection token lacks NUL capacity");
        out[size] = 0;
        while (size && out[size - 1] == ' ')
            out[--size] = 0;
        if (current() == ',')
            ++cursor;
        return size;
    }
    void next_line()
    {
        while (current() != 0 && !newline(current()))
            ++cursor;
        while (horizontal(current()) || newline(current()))
            ++cursor;
    }
};
std::string_view string(std::span<const std::uint8_t> bytes)
{
    const auto end = std::find(bytes.begin(), bytes.end(), std::uint8_t{});
    if (end == bytes.end())
        throw std::invalid_argument("frontend selection unbounded string");
    return {reinterpret_cast<const char *>(bytes.data()), static_cast<std::size_t>(end - bytes.begin())};
}
bool overlap(const void *a, std::size_t an, const void *b, std::size_t bn)
{
    if (!an || !bn)
        return false;
    const auto x = reinterpret_cast<std::uintptr_t>(a), y = reinterpret_cast<std::uintptr_t>(b);
    return x <= y ? y - x < an : x - y < bn;
}
struct Wrap
{
    unsigned first, last;
    std::int32_t width;
    float scale;
    std::int32_t lines;
};
// 440DAB..441047, inclusive text IDs, in original execution order.
constexpr std::array<Wrap, 14> wraps{{{0x1a0, 0x1b4, 0x94, 1.25f, 14},
                                      {0x1d1, 0x1e9, 0x94, 1.25f, 14},
                                      {0x20c, 0x22e, 0x21c, 1.5f, 4},
                                      {0x231, 0x245, 0x15c, 1.5f, 4},
                                      {0x11f, 0x181, 0x184, 1.5f, 10},
                                      {0x1f2, 0x1f7, 0x15c, 1.5f, 10},
                                      {0x96, 0x99, 0x118, 1.25f, 10},
                                      {0x260, 0x26c, 0x17c, 1.25f, 10},
                                      {0x276, 0x27b, 0x1ae, 1.25f, 10},
                                      {0x26f, 0x273, 0x208, 1.25f, 10},
                                      {0xce, 0xcf, 0x17c, 1.5f, 6},
                                      {0x3a, 0x3b, 0x15c, 1.5f, 5},
                                      {0x3c, 0x3d, 0x230, 1.5f, 5},
                                      {2, 2, 0x258, 1.25f, 5}}};
void valid_metrics(FrontendFontMetricsViews fonts)
{
    if (fonts.count == 0 || fonts.count > 2)
        throw std::invalid_argument("frontend selection missing actual font metrics");
    for (auto glyph : fonts.glyph_map)
        if (glyph >= 160)
            throw std::invalid_argument("frontend selection glyph mapping outside record");
    for (unsigned f = 0; f < fonts.count; ++f)
    {
        const auto first = 161 * f;
        if (fonts.records[first] == 0 || fonts.records[first] > 256)
            throw std::invalid_argument("frontend selection invalid font width");
        for (unsigned i = 1; i <= 160; ++i)
            if (fonts.records[first + i] > 256)
                throw std::invalid_argument("frontend selection requires bounded glyph advances");
    }
    const auto digit = std::bit_cast<float>(fonts.digit_advance_bits);
    if (!std::isnan(digit) && (!std::isfinite(digit) || std::abs(digit) > 256))
        throw std::invalid_argument("frontend selection digit advance bound");
}
template <class Assign> std::uint32_t parse(std::span<const std::uint8_t> input, unsigned column, Assign assign)
{
    Reader reader{input};
    reader.next_line();
    reader.next_line();
    std::array<std::uint8_t, 64> label{};
    std::array<std::uint8_t, 256> value{};
    while (reader.token(label))
    {
        for (unsigned i = 0; i <= column; ++i)
        {
            const auto length = reader.token(value);
            if (reader.unterminated_quote)
                throw std::invalid_argument("frontend selection quoted value has undefined source local tail");
            if (length == 0)
                break;
        }
        reader.next_line();
        if (value[0] == '<' && value[1] == '>' && value[2] == 0)
            value[0] = 0;
        assign(string(label), string(value));
    }
    return reader.cursor;
}
void preflight(FrontendLanguageSelectionViews v, FrontendLanguageInput input, FrontendFontMetricsViews fonts,
               unsigned column)
{
    if (!input.identity || input.identity != v.catalog.input_identity || input.bytes.empty() ||
        input.bytes.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::invalid_argument("frontend selection does not borrow the retained catalog resource");
    if (v.descriptors[731 * 2] != 0)
        throw std::invalid_argument("frontend selection missing retained descriptor sentinel");
    valid_metrics(fonts);
    const std::array<std::pair<const void *, std::size_t>, 14> owners{
        {{v.catalog.catalog.data(), v.catalog.catalog.size_bytes()},
         {v.catalog.text_handles.data(), v.catalog.text_handles.size_bytes()},
         {v.catalog.language_names.data(), v.catalog.language_names.size_bytes()},
         {&v.catalog.input_identity, 4},
         {v.scratch.data(), v.scratch.size_bytes()},
         {v.arena.data(), v.arena.size_bytes()},
         {v.descriptors.data(), v.descriptors.size_bytes()},
         {&v.remaining, 4},
         {fonts.records.data(), fonts.records.size_bytes()},
         {fonts.index.data(), fonts.index.size_bytes()},
         {fonts.glyph_map.data(), fonts.glyph_map.size_bytes()},
         {&fonts.current, 4},
         {&fonts.count, 4},
         {&fonts.digit_advance_bits, 4}}};
    for (unsigned i = 0; i < owners.size(); ++i)
    {
        for (unsigned j = 0; j < i; ++j)
            if (overlap(owners[i].first, owners[i].second, owners[j].first, owners[j].second))
                throw std::invalid_argument("frontend selection owners overlap");
        if (overlap(owners[i].first, owners[i].second, input.bytes.data(), input.bytes.size()))
            throw std::invalid_argument("frontend selection input overlaps output");
    }
    std::array<bool, 731> consumed{};
    std::array<bool, frontend_text_count> assigned{};
    std::uint32_t bytes = 0;
    (void)parse(input.bytes, column, [&](std::string_view label, std::string_view value) {
        for (unsigned i = 0; i < detail::frontend_labels.size(); ++i)
            if (!consumed[i] && detail::frontend_labels[i].name == label)
            {
                consumed[i] = true;
                bytes += static_cast<std::uint32_t>(value.size() + 1);
                if (bytes > frontend_language_arena_bytes)
                    throw std::invalid_argument("frontend selection exhausts source arena");
                assigned[detail::frontend_labels[i].id] = true;
                break;
            }
    });
    for (auto w : wraps)
        for (unsigned id = w.first; id <= w.last; ++id)
            if (!assigned[id])
                throw std::invalid_argument("frontend selection would dereference missing wrap string");
}
} // namespace
std::uint32_t wrap_frontend_text_4409c0(std::span<std::uint8_t> text, std::span<std::uint8_t, 0x404> scratch,
                                        FrontendFontMetricsViews fonts, std::int32_t width, float scale,
                                        std::int32_t maximum_lines)
{
    const auto end = std::find(text.begin(), text.end(), std::uint8_t{});
    if (end == text.end() || static_cast<std::size_t>(end - text.begin()) >= scratch.size() || !std::isfinite(scale) ||
        std::abs(scale) > 256)
        throw std::invalid_argument("frontend wrap text or scale outside bounded source domain");
    if (overlap(text.data(), text.size(), scratch.data(), scratch.size()))
        throw std::invalid_argument("frontend wrap text overlaps scratch");
    valid_metrics(fonts);
    const std::array<std::pair<const void *, std::size_t>, 6> owners{
        {{fonts.records.data(), fonts.records.size_bytes()},
         {fonts.index.data(), fonts.index.size_bytes()},
         {fonts.glyph_map.data(), fonts.glyph_map.size_bytes()},
         {&fonts.current, 4},
         {&fonts.count, 4},
         {&fonts.digit_advance_bits, 4}}};
    for (unsigned i = 0; i < owners.size(); ++i)
    {
        for (unsigned j = 0; j < i; ++j)
            if (overlap(owners[i].first, owners[i].second, owners[j].first, owners[j].second))
                throw std::invalid_argument("frontend wrap font owners overlap");
        if (overlap(owners[i].first, owners[i].second, text.data(), text.size()) ||
            overlap(owners[i].first, owners[i].second, scratch.data(), scratch.size()))
            throw std::invalid_argument("frontend wrap text/scratch overlaps a font owner");
    }
    for (auto at = text.begin(); at != end; ++at)
        if (*at == 255)
            *at = 32;
    std::uint32_t start = 0, scan = 0, lines = 1;
    for (;;)
    {
        const auto delimiter = text[scan];
        if (delimiter != 32 && delimiter != '|' && delimiter != 0)
        {
            ++scan;
            continue;
        }
        std::copy(text.begin() + start, text.begin() + scan, scratch.begin());
        scratch[scan - start] = 0;
        const double measured = measure_frontend_text_438910(fonts, scratch, scale);
        const double rounded = measured + static_cast<double>(std::bit_cast<float>(0x3f7fffefu));
        // Bounded width/scale/text domains above keep this within int32.
        const auto pixels = static_cast<std::int32_t>(rounded);
        if (pixels >= width)
        {
            auto back = static_cast<std::int32_t>(scan) - 1;
            while (back > static_cast<std::int32_t>(start) && text[static_cast<unsigned>(back)] != 32)
                --back;
            if (back <= static_cast<std::int32_t>(start))
                return 0;
            text[static_cast<unsigned>(back)] = 255;
            start = static_cast<unsigned>(back) + 1;
            scan = start;
            ++lines;
        }
        else if (delimiter == '|')
        {
            ++lines;
            start = ++scan;
        }
        else if (delimiter == 0)
            return static_cast<std::int32_t>(lines) > maximum_lines ? 0 : lines;
        else
            ++scan;
    }
}
FrontendLanguageSelectionResult select_frontend_language_440c40(FrontendLanguageSelectionViews v,
                                                                FrontendLanguageInput input,
                                                                FrontendFontMetricsViews fonts, std::uint32_t flag)
{
    const auto count = std::bit_cast<std::int32_t>(v.catalog.catalog.back());
    if (count <= 0)
        return {-1, 0, 0};
    if (count > 8)
        throw std::invalid_argument("frontend language catalog exceeds eight source records");
    unsigned column = 0;
    for (; column < static_cast<unsigned>(count); ++column)
        if (v.catalog.catalog[9 + column * 9] == flag)
            break;
    if (column == static_cast<unsigned>(count))
        return {-1, 0, 0};
    preflight(v, input, fonts, column);
    for (unsigned i = 0; i < detail::frontend_labels.size(); ++i)
    {
        v.descriptors[2 * i] = i + 1;
        v.descriptors[2 * i + 1] = detail::frontend_labels[i].id;
    }
    std::fill(v.catalog.text_handles.begin(), v.catalog.text_handles.end(), 0u);
    v.remaining = frontend_language_arena_bytes;
    std::uint32_t unresolved = 731;
    const auto cursor = parse(input.bytes, column, [&](std::string_view label, std::string_view value) {
        for (unsigned i = 0; i < unresolved; ++i)
            if (detail::frontend_labels[v.descriptors[2 * i] - 1].name == label)
            {
                v.remaining -= static_cast<std::uint32_t>(value.size() + 1);
                std::copy(value.begin(), value.end(), v.arena.begin() + v.remaining);
                v.arena[v.remaining + value.size()] = 0;
                v.catalog.text_handles[v.descriptors[2 * i + 1]] = v.remaining + 1;
                --unresolved;
                for (unsigned j = i; j < unresolved; ++j)
                {
                    v.descriptors[2 * j] = v.descriptors[2 * j + 2];
                    v.descriptors[2 * j + 1] = v.descriptors[2 * j + 3];
                }
                break;
            }
    });
    for (auto w : wraps)
        for (unsigned id = w.first; id <= w.last; ++id)
        {
            const auto offset = v.catalog.text_handles[id] - 1;
            (void)wrap_frontend_text_4409c0(v.arena.subspan(offset), v.scratch, fonts, w.width, w.scale, w.lines);
        }
    v.catalog.catalog.front() = flag;
    return {static_cast<std::int32_t>(column), cursor, unresolved};
}
} // namespace sgn::retail
