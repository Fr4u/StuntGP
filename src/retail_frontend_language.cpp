#include "sgn/retail_frontend_language.hpp"
#include "retail_frontend_catalog_parser.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace sgn::retail
{
namespace
{
struct SpanInput
{
    std::span<const std::uint8_t> bytes;
    std::uint8_t read(std::uint32_t offset, std::uint32_t) const
    {
        if (offset == bytes.size())
            return 0;
        if (offset > bytes.size())
            throw std::invalid_argument("frontend language cursor outside input");
        return bytes[offset];
    }
    [[noreturn]] void fail(std::uint32_t, const char *why) const
    {
        throw std::invalid_argument(why);
    }
};
struct LegacyWriter
{
    FrontendLanguageCatalogViews views;
    bool publish;
    [[noreturn]] void fail(std::uint32_t, const char *why) const
    {
        throw std::invalid_argument(why);
    }
    void name(unsigned index, std::span<const std::uint8_t> bytes)
    {
        if (publish)
        {
            views.language_names[index + 1] = index + 1;
            std::copy(bytes.begin(), bytes.end(),
                      reinterpret_cast<std::uint8_t *>(views.catalog.data()) + 4 + index * 36);
        }
    }
    void count(unsigned value)
    {
        if (publish)
            views.catalog.back() = value;
    }
    void flag(unsigned index, std::uint32_t value)
    {
        if (publish)
            views.catalog[9 + index * 9] = value;
    }
};
bool overlaps(const void *a, std::size_t an, const void *b, std::size_t bn) noexcept
{
    if (an == 0 || bn == 0)
        return false;
    const auto x = reinterpret_cast<std::uintptr_t>(a), y = reinterpret_cast<std::uintptr_t>(b);
    return x <= y ? y - x < an : x - y < bn;
}
std::uint32_t catalog_pass(FrontendLanguageCatalogViews views, FrontendLanguageInput input, bool publish)
{
    if (publish)
    {
        std::fill(views.text_handles.begin(), views.text_handles.end(), 0u);
        views.input_identity = input.identity;
        std::fill(views.language_names.begin(), views.language_names.end(), 0u);
        views.catalog.back() = 0;
    }
    SpanInput bytes{input.bytes};
    LegacyWriter writer{views, publish};
    return detail::parse_catalog_contents(bytes, writer, true);
}
} // namespace
std::uint32_t initialize_frontend_language_catalog(FrontendLanguageCatalogViews views, FrontendLanguageInput input)
{
    static_assert(std::endian::native == std::endian::little);
    if (input.identity == 0 || input.bytes.empty() || input.bytes.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::invalid_argument("frontend language source resource is unavailable or exceeds its cursor domain");
    const std::array<std::pair<const void *, std::size_t>, 5> ranges{
        {{views.catalog.data(), views.catalog.size_bytes()},
         {views.text_handles.data(), views.text_handles.size_bytes()},
         {views.language_names.data(), views.language_names.size_bytes()},
         {&views.input_identity, sizeof(views.input_identity)},
         {input.bytes.data(), input.bytes.size()}}};
    for (unsigned i = 0; i < ranges.size(); ++i)
        for (unsigned j = 0; j < i; ++j)
            if (overlaps(ranges[i].first, ranges[i].second, ranges[j].first, ranges[j].second))
                throw std::invalid_argument("frontend language input and source owners overlap");
    (void)catalog_pass(views, input, false);
    return catalog_pass(views, input, true);
}
FrontendLanguageTextIdentity frontend_language_text_441170(FrontendLanguageStrings strings, std::uint32_t id)
{
    if (id < strings.text_handles.size())
        return {strings.text_handles[id]};
    // 441170 indexes the source words continuously. PageLanguage ids733..740
    // reach the existing adjacent606838 name table, whose host identities are1..8.
    const auto index = id - static_cast<std::uint32_t>(strings.text_handles.size());
    if (index >= strings.language_names.size())
        throw std::invalid_argument("frontend language text ID outside source owners");
    const auto name = strings.language_names[index];
    // 440B1A fills names with6087A4; the existing native table represents that
    // empty-sentinel identity as0. It is not the NULL arena identity.
    if (name == 0)
        return {frontend_catalog_text_tag};
    if (name > 8)
        throw std::invalid_argument("frontend language name identity outside existing catalog");
    return {frontend_catalog_text_tag | (5u + (name - 1u) * 36u)};
}
std::span<const std::uint8_t> resolve_frontend_language_text(FrontendLanguageStrings strings,
                                                             FrontendLanguageTextIdentity identity)
{
    if (identity.value == 0)
        return {};
    const auto sentinel = identity.value == frontend_catalog_text_tag;
    const auto catalog = (identity.value & frontend_catalog_text_tag) != 0;
    const auto offset = sentinel ? 0u : (identity.value & ~frontend_catalog_text_tag) - 1u;
    const auto owner = sentinel  ? strings.empty_sentinel
                       : catalog ? strings.catalog
                                 : std::span<const std::uint8_t>{strings.arena};
    if (offset >= owner.size())
        throw std::invalid_argument("frontend language text identity outside its owner");
    const auto value = owner.subspan(offset);
    const auto end = std::find(value.begin(), value.end(), std::uint8_t{});
    if (end == value.end())
        throw std::invalid_argument("frontend language string lacks a bounded NUL");
    return value.first(static_cast<std::size_t>(end - value.begin()) + 1);
}
} // namespace sgn::retail
