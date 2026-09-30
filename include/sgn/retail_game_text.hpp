#pragma once
#include "sgn/assets.hpp"
#include "sgn/retail_game_text_table.hpp"
#include <optional>
namespace sgn::retail
{
struct SourceBootConfigState;
struct SourceBootFilePlatform;
struct GameTextState
{
    static constexpr unsigned arena_size = 0x3000;
    std::array<char, arena_size> arena{};                    // 60F6A0, source bytes, not UTF-8 conversion
    std::array<std::optional<std::uint32_t>, 105> offsets{}; // 6127A0, native arena offsets
    std::array<std::uint32_t, 105> descriptor_order{};       // 60F350, UINT_MAX denotes null sentinel
    std::uint32_t remaining_bytes{arena_size}, language{};   // 4835DC/612944
    GameTextState(); // explicit buffer compatibility: initializes descriptor order
    struct PeImageOnly
    {
    };
    // PE60F350 descriptors remain zero. Reached445D10 belongs to the loader;
    // creating a world must not execute that later source child.
    explicit GameTextState(PeImageOnly) noexcept {}
    GameTextState(const GameTextState &) = delete;
    GameTextState &operator=(const GameTextState &) = delete;
    std::optional<std::string_view> text(std::uint32_t index) const;
    std::string_view required(GameTextId id) const;
};
// Complete445D70 state transition and concrete CSV/interning callees. Languages
// 1..7 are the authored columns. Zero would use an unwritten original stack
// string and is explicitly outside this bounded interface.
// Each call rebuilds descriptor order and clears text handles, but preserves
// the arena and remaining allocation budget, exactly like the original.
void load_game_text(std::string_view csv, std::uint32_t language, GameTextState &);
void load_game_text(const AssetStore &, std::uint32_t language, GameTextState &);
// Invocation-only host asset borrow. The one existing loader performs its
// source resets before assets() can fail; this owns no resource or text state.
struct GameTextAssetSource
{
    virtual ~GameTextAssetSource() = default;
    virtual const AssetStore &assets() const = 0;
};
void load_game_text(GameTextAssetSource &, std::uint32_t language, GameTextState &);
// Reached445DBA ->445CA0 using the EXISTING source path/WAD cache and file
// routes, without AssetStore. Complete-read, bounded CSV domain only. Errors
// retain reached text/cache writes. Local raw allocation frees before445E46.
void load_game_text(SourceBootConfigState &, SourceBootFilePlatform &, std::uint32_t language, GameTextState &);
} // namespace sgn::retail
