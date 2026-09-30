#include "sgn/retail_campaign_launch.hpp"
#include "sgn/retail_campaign_presentation.hpp"
#include "sgn/retail_frontend_launch.hpp"
#include "sgn/little_endian_words.hpp"
#include <algorithm>
#include <array>

namespace sgn::retail
{
namespace
{
using sgn::little_endian::read_le32;
using sgn::little_endian::write_le32;
template <std::size_t N> auto bank(FrontendSourceStorage &owner, std::uint32_t address)
{
    return std::as_writable_bytes(std::span<std::uint8_t, N>{owner.source_bytes(address, N)});
}
void validate_configuration(std::span<const std::byte, 0xEC> configuration)
{
    for (const auto kind : {5u, 4u, 12u, 8u, 3u, 6u, 13u, 9u, 11u, 7u, 10u})
        if (read_le32(configuration, kind * 16u + 16u) > (kind == 13 ? 3u : 5u))
            throw FormatError("campaign launch part index outside source catalog");
}
} // namespace

void launch_campaign_race_44bf10(FrontendLaunchWorld &world)
{
    auto &owner = world.frontend;
    const auto live = bank<0x160>(owner, 0x5F92DC);
    const auto rules = bank<0x660>(owner, 0x5F943C);
    const auto items = bank<0x57E4>(owner, 0x5F3AF4);
    const auto launch = bank<0x1EC>(owner, 0x61BBE0);
    const auto result = bank<0x7C>(owner, 0x61BED0);
    const auto race = read_le32(live, 8), model = read_le32(live, 0x10), sponsor = read_le32(live, 0x108);
    // New host bounds, before any mutation. These reject pointers outside the
    // represented source banks; they are not additional retail decisions.
    if (race > 21 || model > 21 || sponsor > 6 || owner.initials_scratch[3] != 0)
        throw FormatError("campaign launch input outside represented source owners");
    const auto round = rules.subspan(0x88 + race * 0x44, 0x44);
    const auto track = read_le32(round, 0), controller = owner.source_word(0x5E37A4);
    if (track > 24 || controller < 1 || controller > 4)
        throw FormatError("campaign launch track or controller outside source domain");
    validate_configuration(live.subspan<0x14, 0xEC>());
    for (std::size_t row = 0; row < 6; ++row)
    {
        const auto grid = read_le32(live, 0x10C + row * 16);
        if (grid < 1 || grid > 6 || read_le32(live, 0x108 + row * 16) > 6)
            throw FormatError("campaign launch grid or rival sponsor outside source domain");
        if (row != 0)
        {
            const auto rival_model = read_le32(round, 8 + (row - 1) * 12);
            if (rival_model > 21)
                throw FormatError("campaign rival model outside source catalog");
            validate_configuration(bank<0xEC>(owner, 0x5F26AC + rival_model * 0xEC));
        }
    }
    const auto records = bank<32>(owner, 0x5E0F7C + track * 32);

    // 44BF1C..44BF34: clear the actual output banks, not temporary substitutes.
    std::fill(launch.begin(), launch.end(), std::byte{0});
    std::fill(result.begin(), result.end(), std::byte{0});
    std::array<std::byte, 0x218> assembled{}; // original caller-local stack data
    compose_campaign_configuration_436490({items, rules}, sponsor, live.subspan<0x14, 0xEC>(),
                                          std::span<const std::byte, 300>{items.subspan(model * 300u, 300)}, assembled);
    // 44BF64..44BFD2 copies the model suffix, not the 0xEC config prefix.
    std::copy_n(assembled.begin() + 0xEC, 300, launch.begin());
    std::copy_n(live.begin() + 4, 2, launch.begin() + 0x130);
    write_le32(launch, 0x138, read_le32(live, 0x10C));
    launch[0x132] = live[6];
    write_le32(launch, 0x134, model);
    write_le32(launch, 0x12C, 1);
    write_le32(launch, 0x13C, read_le32(rules, 0x38 + sponsor * 8));
    // Pinned 4863F4 pointer table; source copies exactly three literal bytes.
    static constexpr std::array<std::array<char, 3>, 7> names{{{'.', '.', '.'},
                                                               {'T', 'G', 'R'},
                                                               {'R', 'L', 'N'},
                                                               {'I', 'T', 'X'},
                                                               {'C', 'A', 'L'},
                                                               {'T', 'T', 'N'},
                                                               {'M', 'Y', 'S'}}};
    for (std::size_t row = 1; row < 6; ++row)
    {
        const auto at = 0x12C + row * 0x1C;
        const auto rival = read_le32(live, 0x108 + row * 16);
        const auto name = std::as_bytes(std::span(names[rival]));
        std::copy(name.begin(), name.end(), launch.begin() + at + 4);
        write_le32(launch, at, 0);
        write_le32(launch, at + 16, read_le32(rules, 0x38 + rival * 8));
        write_le32(launch, at + 12, read_le32(live, 0x10C + row * 16));
        const auto authored = 8 + (row - 1) * 12;
        write_le32(launch, at + 8, read_le32(round, authored));
        write_le32(launch, at + 20, read_le32(round, authored + 4));
        write_le32(launch, at + 24, read_le32(round, authored + 8));
    }
    // 44C03A..44C08B uses record values at stride eight, preserving flags.
    write_le32(launch, 0x1D4, track);
    write_le32(launch, 0x1D8, read_le32(round, 4));
    write_le32(launch, 0x1DC, read_le32(records, 0));
    write_le32(launch, 0x1E0, read_le32(records, 8));
    write_le32(launch, 0x1E4, read_le32(records, 16));
    write_le32(launch, 0x1E8, read_le32(records, 24));
    select_campaign_race(world); // actual full 43AF20, called at 0044C090
}
} // namespace sgn::retail
