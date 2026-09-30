#include "sgn/retail_campaign_presentation.hpp"
#include "sgn/retail_championship.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/little_endian_words.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_signed;
using sgn::little_endian::read_le32;
using sgn::little_endian::write_le32;
constexpr std::array<std::uint32_t, 11> kinds{5, 4, 12, 8, 3, 6, 13, 9, 11, 7, 10};
// clang-format off: source address/offset tables, rows kept as in the pinned layout (12 offsets per row)
constexpr std::array<std::uint32_t, 11> bases{
    0x5F5390, 0x5F596C, 0x5F5F48, 0x5F6524, 0x5F6B00, 0x5F70DC,
    0x5F76B8, 0x5F7A3C, 0x5F8018, 0x5F85F4, 0x5F8BD0};
constexpr std::array<std::size_t, 72> overrides{
    0x104,0x108,0x10C,0x110,0x114,0x118,0x0C,0x10,0x14,0x70,0x18,0x1C,
    0x20,0x24,0x28,0x2C,0x30,0x40,0x94,0x98,0x34,0x38,0x3C,0x44,
    0x48,0x4C,0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C,0x11C,0x120,
    0x74,0x78,0x7C,0x80,0x84,0x88,0x8C,0x90,0x9C,0xA0,0xA4,0xA8,
    0xAC,0xB0,0xB4,0xB8,0x100,0xBC,0xC0,0xC4,0xC8,0xCC,0xD0,0xE8,
    0xEC,0xF0,0xF4,0xD4,0xD8,0xDC,0xE0,0xE4,0xF8,0xFC,0x124,0x128};
// clang-format on
bool integer_override(std::size_t at)
{
    switch (at)
    {
    case 0x2C:
    case 0x30:
    case 0x34:
    case 0x8C:
    case 0x90:
    case 0xE8:
    case 0xEC:
    case 0xF0:
    case 0xF4:
    case 0xF8:
    case 0xFC:
    case 0x124:
        return true;
    default:
        return false;
    }
}
std::uint32_t default_part(std::uint32_t kind)
{
    if (kind < 3 || kind > 13)
        throw std::invalid_argument("campaign part kind outside recovered catalog");
    return kind == 13 ? 3u : 5u; // 437A90 for the eleven 436490 part kinds
}
std::span<const std::byte, 300> part(CampaignConfigurationCatalog catalog, std::uint32_t kind, std::uint32_t index)
{
    const auto maximum = default_part(kind);
    if (index > maximum)
        throw std::invalid_argument("campaign part index outside source catalog");
    const auto found = std::find(kinds.begin(), kinds.end(), kind);
    const auto offset = bases[static_cast<std::size_t>(found - kinds.begin())] - 0x5F3AF4u + index * 300u;
    return std::span<const std::byte, 300>(catalog.items.subspan(offset, 300));
}
void validate(CampaignConfigurationCatalog catalog, std::uint32_t sponsor,
              std::span<const std::byte, 0xEC> configuration)
{
    if (sponsor > 6)
        throw std::invalid_argument("campaign sponsor outside source table");
    for (const auto kind : kinds)
        (void)part(catalog, kind, read_le32(configuration, kind * 16u + 16u));
}
std::uint32_t scaled_high(std::uint32_t wrapped_product, std::int32_t magic)
{
    const auto product = static_cast<std::int64_t>(as_signed(wrapped_product)) * magic;
    const auto high = static_cast<std::int32_t>(product >> 32);
    const auto shifted = high >> 5;
    return static_cast<std::uint32_t>(shifted) + (static_cast<std::uint32_t>(shifted) >> 31);
}
bool nonnegative_float(std::uint32_t bits)
{
    const auto magnitude = bits & 0x7FFFFFFFu;
    return magnitude <= 0x7F800000u && ((bits & 0x80000000u) == 0 || magnitude == 0);
}
std::uint32_t add_float_words(std::uint32_t a, std::uint32_t b)
{
    // FLD/FADD/FSTP at43660F/436612/43661E (and436644..43664C).
    // Preserve x87 NaN choice and indefinite, independently of SSE propagation.
    const auto ma = a & 0x7FFFFFFFu, mb = b & 0x7FFFFFFFu;
    const bool na = ma > 0x7F800000u, nb = mb > 0x7F800000u;
    if (na || nb)
    {
        std::uint32_t chosen = na ? a : b;
        if (na && nb)
        {
            const bool qa = (a & 0x00400000u) != 0, qb = (b & 0x00400000u) != 0;
            if (qa != qb)
                chosen = qa ? a : b;
            else
                chosen = ma == mb ? std::min(a, b) : ma > mb ? a : b;
        }
        return chosen | 0x00400000u;
    }
    if (ma == 0x7F800000u && mb == 0x7F800000u && a != b)
        return 0xFFC00000u;
    const double sum = static_cast<double>(std::bit_cast<float>(a)) + static_cast<double>(std::bit_cast<float>(b));
    return std::bit_cast<std::uint32_t>(static_cast<float>(sum));
}
void copy_string(std::span<std::byte> destination, std::span<const std::byte> source)
{
    const auto end = std::find(source.begin(), source.end(), std::byte{0});
    if (end == source.end())
        throw std::invalid_argument("campaign source string has no terminator");
    const auto length = static_cast<std::size_t>(end - source.begin()) + 1;
    if (length > destination.size())
        throw std::invalid_argument("campaign source string exceeds destination");
    std::copy_n(source.begin(), length, destination.begin());
}
} // namespace

std::uint32_t campaign_item_price_436420(CampaignConfigurationCatalog catalog, std::uint32_t sponsor,
                                         std::span<const std::byte, 16> selection, std::span<const std::byte, 300> item)
{
    if (sponsor > 6)
        throw std::invalid_argument("campaign sponsor outside source table");
    auto price = read_le32(item, 0);
    constexpr auto negative_magic = std::bit_cast<std::int32_t>(0xAE147AE1u);
    if (read_le32(selection, 0) != 0)
        price += scaled_high(read_le32(catalog.rules, 0x30) * price, negative_magic);
    if (sponsor != 0 && read_le32(item, 0x124) == read_le32(catalog.rules, 0x38 + sponsor * 8u))
        price += scaled_high(read_le32(catalog.rules, 0x2C) * price, negative_magic);
    return price;
}
void apply_campaign_upgrade_4365d0(CampaignConfigurationCatalog catalog, std::uint32_t sponsor, std::uint32_t kind,
                                   std::span<const std::byte, 0xEC> configuration, std::span<std::byte, 300> out)
{
    if (sponsor > 6)
        throw std::invalid_argument("campaign sponsor outside source table");
    const auto fallback = default_part(kind);
    const auto selected = read_le32(configuration, kind * 16u + 16u);
    const auto item = part(catalog, kind, selected == 0 ? fallback : selected);
    const auto price =
        selected == 0
            ? read_le32(item, 0)
            : campaign_item_price_436420(
                  catalog, sponsor, std::span<const std::byte, 16>(configuration.subspan(kind * 16u + 12u, 16)), item);
    write_le32(out, 0, read_le32(out, 0) + price);
    write_le32(out, 8, add_float_words(read_le32(item, 8), read_le32(out, 8)));
    if (selected == 0)
        return; // default part only contributes price and mass/word08
    for (const auto at : overrides)
    {
        const auto value = read_le32(item, at);
        if (integer_override(at) ? as_signed(value) >= 0 : nonnegative_float(value))
            write_le32(out, at, value);
    }
}
void compose_campaign_configuration_436490(CampaignConfigurationCatalog catalog, std::uint32_t sponsor,
                                           std::span<const std::byte, 0xEC> configuration,
                                           std::span<const std::byte, 300> model, std::span<std::byte, 0x218> out)
{
    validate(catalog, sponsor, configuration);
    std::fill(out.begin(), out.end(), std::byte{0});
    std::copy(configuration.begin(), configuration.end(), out.begin());
    std::copy(model.begin(), model.end(), out.begin() + 0xEC);
    auto assembled = out.subspan<0xEC, 300>();
    if (read_le32(out, 0) != 0)
    {
        const auto price = read_le32(assembled, 0);
        write_le32(assembled, 0, price - scaled_high(price * read_le32(catalog.rules, 0x30), 0x51EB851F));
    }
    for (const auto kind : kinds)
        apply_campaign_upgrade_4365d0(catalog, sponsor, kind, configuration, assembled);
}
std::uint32_t campaign_configuration_value_437de0(CampaignConfigurationCatalog catalog, std::uint32_t sponsor,
                                                  std::uint32_t car, std::span<const std::byte, 0xEC> configuration)
{
    if (car > 20)
        throw std::invalid_argument("campaign car outside source catalog");
    std::array<std::byte, 0x218> assembled{};
    compose_campaign_configuration_436490(catalog, sponsor, configuration,
                                          std::span<const std::byte, 300>(catalog.items.subspan(car * 300u, 300)),
                                          assembled);
    return read_le32(assembled, 0xEC);
}
std::span<const std::byte> format_campaign_value_43d5f0(std::uint32_t value, std::span<std::byte, 0x80> scratch)
{
    static constexpr std::array zero{std::byte{'0'}, std::byte{0}};
    if (as_signed(value) < 0)
        return zero;
    std::size_t written = 0;
    unsigned group = 0;
    do
    {
        if (group == 3)
        {
            scratch[written++] = std::byte{','};
            group = 0;
        }
        ++group;
        scratch[written++] = static_cast<std::byte>('0' + value % 10u);
        value /= 10u;
    } while (value != 0);
    for (std::size_t i = 0; i < written; ++i)
        scratch[64 + i] = scratch[written - i - 1];
    scratch[64 + written] = std::byte{0};
    return scratch.subspan(64, written + 1);
}
void validate_campaign_presentation(CampaignPresentationViews view)
{
    const auto sponsor = read_le32(view.live, 0x108), car = read_le32(view.live, 0x10);
    if (car > 20 || view.initials_scratch[3] != std::byte{0})
        throw std::invalid_argument("campaign presentation source owner outside recovered bounds");
    validate(view.catalog, sponsor, view.live.subspan<0x14, 0xEC>());
    for (std::size_t row = 1; row < 6; ++row)
        if (read_le32(view.live, 0x108 + row * 16) > 6)
            throw std::invalid_argument("campaign rival name outside source table");
}
void build_campaign_presentation_44bd00(CampaignPresentationViews view)
{
    validate_campaign_presentation(view);
    const auto sponsor = read_le32(view.live, 0x108), car = read_le32(view.live, 0x10);
    for (std::size_t i = 0; i < 3; ++i)
        view.initials_scratch[i] = view.live[4 + i]; // full43D690
    copy_string(view.display.subspan(0x14, 8), view.initials_scratch);
    write_le32(view.display, 4, read_le32(view.live, 0x100));
    write_le32(view.display, 8, read_le32(view.live, 0x104));
    write_le32(view.display, 0, 1);
    write_le32(view.display, 28, sponsor);
    write_le32(view.display, 16, read_le32(view.live, 0x10C));
    write_le32(view.display, 12, 0xFFFFFFFFu);
    static constexpr std::array<std::array<char, 4>, 7> names{{{'.', '.', '.', 0},
                                                               {'T', 'G', 'R', 0},
                                                               {'R', 'L', 'N', 0},
                                                               {'I', 'T', 'X', 0},
                                                               {'C', 'A', 'L', 0},
                                                               {'T', 'T', 'N', 0},
                                                               {'M', 'Y', 'S', 0}}};
    for (std::size_t row = 1; row < 6; ++row)
    {
        const auto id = read_le32(view.live, 0x108 + row * 16);
        const auto at = row * 32;
        copy_string(view.display.subspan(at + 20, 8), std::as_bytes(std::span(names[id])));
        write_le32(view.display, at, 0);
        write_le32(view.display, at + 4, read_le32(view.live, 0x100 + row * 16));
        write_le32(view.display, at + 8, read_le32(view.live, 0x104 + row * 16));
        write_le32(view.display, at + 28, id);
        write_le32(view.display, at + 16, read_le32(view.live, 0x10C + row * 16));
        write_le32(view.display, at + 12, 0xFF808080u);
    }
    sort_campaign_standings_44be80(view.display.subspan<0, 0xC0>());
    const auto value = campaign_configuration_value_437de0(view.catalog, sponsor, car, view.live.subspan<0x14, 0xEC>());
    copy_string(view.display.subspan(0xC4, 64),
                format_campaign_value_43d5f0(read_le32(view.live, 12), view.numeric_scratch));
    copy_string(view.display.subspan(0x108, 64), format_campaign_value_43d5f0(value, view.numeric_scratch));
    write_le32(view.display, 0x104, value);
    write_le32(view.display, 0xC0, read_le32(view.live, 12));
}
} // namespace sgn::retail
