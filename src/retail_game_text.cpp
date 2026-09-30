#include "sgn/retail_game_text.hpp"
#include "sgn/retail_csv.hpp"
#include "game_text_detail.hpp"
#include <algorithm>
#include <bit>
#include <limits>
#include <numeric>
namespace sgn::retail
{
namespace
{
void reset_descriptors(GameTextState &state)
{ // 445D10
    std::iota(state.descriptor_order.begin(), state.descriptor_order.end() - 1, 0u);
    state.descriptor_order.back() = std::numeric_limits<std::uint32_t>::max();
}
std::optional<std::uint32_t> intern(std::string_view value, GameTextState &state)
{ // 445F10
    state.remaining_bytes -= static_cast<std::uint32_t>(value.size() + 1);
    if (std::bit_cast<std::int32_t>(state.remaining_bytes) < 0)
        return std::nullopt;
    const auto offset = state.remaining_bytes;
    if (offset >= state.arena.size() || value.size() >= state.arena.size() - offset)
        throw FormatError("game text arena source address outside bounded storage");
    std::copy(value.begin(), value.end(), state.arena.begin() + offset);
    state.arena[offset + value.size()] = 0;
    return offset;
}
} // namespace
GameTextState::GameTextState()
{
    reset_descriptors(*this);
}
std::optional<std::string_view> GameTextState::text(std::uint32_t index) const
{
    if (index >= offsets.size())
        throw FormatError("game text ID outside source table");
    if (!offsets[index])
        return std::nullopt;
    const auto offset = *offsets[index];
    if (offset >= arena.size())
        throw FormatError("game text handle outside arena");
    const auto first = arena.begin() + offset, end = std::find(first, arena.end(), '\0');
    if (end == arena.end())
        throw FormatError("unterminated game text arena string");
    return std::string_view(arena.data() + offset, static_cast<std::size_t>(end - first));
}
std::string_view GameTextState::required(GameTextId id) const
{
    const auto found = text(static_cast<std::uint32_t>(id));
    if (!found)
        throw FormatError("required authored game text unavailable");
    return *found;
}
namespace game_text_detail
{
void begin_game_text_load(GameTextState &state)
{
    state.offsets.fill(std::nullopt); // 445D8C, before file acquisition
    reset_descriptors(state);         // 445D94 ->445D10; arena/budget/current retained
}
void validate_game_text_input(std::string_view csv, std::uint32_t language)
{
    if (language < 1 || language > 7 || csv.size() > 1024 * 1024 || csv.find('\0') != csv.npos)
        throw FormatError("game text input outside authored language domain");
}
void load_prepared_game_text(std::string_view csv, std::uint32_t language, GameTextState &state)
{
    std::size_t remaining = game_text_table::definitions.size(), at = 0;
    auto name = read_retail_csv_token(csv, at, 64);
    while (!name.empty())
    {
        std::string_view value;
        for (unsigned i = 0; i < language; ++i)
        {
            auto start = at;
            while (start < csv.size() && (csv[start] == ' ' || csv[start] == '\t'))
                ++start;
            // 445B80 leaves an unterminated temporary if a quoted value reaches
            // EOF.445D70 would still pass that temporary to strlen; reject this
            // unsafe input instead of inventing a deterministic empty string.
            if (start < csv.size() && csv[start] == '"' && csv.find('"', start + 1) == csv.npos)
                throw FormatError("unterminated game text value outside bounded domain");
            value = read_retail_csv_token(csv, at, 256);
            if (value.empty())
                break;
        }
        skip_retail_csv_line(csv, at);
        for (std::size_t i = 0; i < remaining; ++i)
        {
            const auto &definition = game_text_table::definitions[state.descriptor_order[i]];
            if (definition.name == name)
            {
                state.offsets[definition.index] = intern(value, state);
                --remaining;
                for (std::size_t j = i; j < remaining; ++j)
                    state.descriptor_order[j] = state.descriptor_order[j + 1];
                break;
            }
        }
        name = read_retail_csv_token(csv, at, 64);
    }
}
} // namespace game_text_detail
void load_game_text(std::string_view csv, std::uint32_t language, GameTextState &state)
{
    // Keep the existing explicit-buffer admission contract before source writes.
    game_text_detail::validate_game_text_input(csv, language);
    game_text_detail::begin_game_text_load(state);
    game_text_detail::load_prepared_game_text(csv, language, state);
    state.language = language; // 445E46
}
void load_game_text(GameTextAssetSource &source, std::uint32_t language, GameTextState &state)
{
    game_text_detail::begin_game_text_load(state);
    const auto bytes = source.assets().resource("Config\\GameText.csv"); // 445DBA boundary
    // 445CA0 returns null on size<=0;445B95 then reads that null source buffer.
    // Report the reached host boundary after the source table resets, not success.
    if (bytes.empty())
        throw FormatError("445B95 game text source buffer is null after empty resource");
    const std::string_view csv(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    game_text_detail::validate_game_text_input(csv, language);
    game_text_detail::load_prepared_game_text(csv, language, state);
    state.language = language; // 445E46, existing host resource borrow has no source free
}
void load_game_text(const AssetStore &assets, std::uint32_t language, GameTextState &state)
{
    class BorrowedAssets final : public GameTextAssetSource
    {
        const AssetStore &assets_;

      public:
        explicit BorrowedAssets(const AssetStore &value) : assets_(value) {}
        const AssetStore &assets() const override
        {
            return assets_;
        }
    } source(assets);
    load_game_text(source, language, state);
}
} // namespace sgn::retail
