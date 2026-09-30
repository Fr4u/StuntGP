#include "sgn/retail_loading_tables.hpp"

namespace sgn::retail
{
WorldMaterialPrelude source_loading_prelude(LoadingPrelude prelude)
{
    static const std::array<std::string, 9> boot_names{"Boot\\Fade",   "Boot\\Boot1a", "Boot\\Boot1b",
                                                       "Boot\\Boot1c", "Boot\\Boot1d", "Boot\\Boot2a",
                                                       "Boot\\Boot2b", "Boot\\Boot2c", "Boot\\Boot2d"};
    static const std::array<std::string, 5> loading_names{"Loading\\Fade", "Loading\\Load1a", "Loading\\Load1b",
                                                          "Loading\\Load1c", "Loading\\Load1d"};
    static const auto boot = [] {
        std::array<WorldMaterialPreludeEntry, 9> entries{};
        for (std::size_t i = 0; i < entries.size(); ++i)
            entries[i] = {&boot_names[i], 0, 0};
        return entries;
    }();
    static const auto loading = [] {
        std::array<WorldMaterialPreludeEntry, 5> entries{};
        for (std::size_t i = 0; i < entries.size(); ++i)
            entries[i] = {&loading_names[i], 0, 0};
        return entries;
    }();
    switch (prelude)
    {
    case LoadingPrelude::boot:
        return boot;
    case LoadingPrelude::loading:
        return loading;
    }
    throw FormatError("loading presentation prelude selector outside source tables");
}
std::span<const LoadingTile, 4> source_loading_tiles()
{
    static constexpr std::array<LoadingTile, 4> tiles{{{0, 0, 0x43800000, 0x43800000},
                                                       {0x43800000, 0, 0x43800000, 0x43800000},
                                                       {0, 0x43800000, 0x43800000, 0x43600000},
                                                       {0x43800000, 0x43800000, 0x43800000, 0x43600000}}};
    return tiles;
}
} // namespace sgn::retail
