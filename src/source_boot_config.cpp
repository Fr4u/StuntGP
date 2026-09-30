#include "sgn/source_boot_config.hpp"
#include <algorithm>
#include <bit>
#include <limits>
#include <optional>
#include <string>

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
void check(bool ok, Word site, const char *why)
{
    if (!ok)
        throw SourceBootConfigError(site, why);
}
template <std::size_t N> void text_store(std::array<char, N> &output, std::string_view input, Word site)
{
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        check(i < N, site, "Source string write exceeds admitted destination");
        output[i] = input[i];
    }
    check(input.size() < N, site, "Source string terminator exceeds admitted destination");
    output[input.size()] = 0;
}
std::string_view c_string(std::span<const char> input, Word site)
{
    const auto end = std::find(input.begin(), input.end(), '\0');
    check(end != input.end(), site, "Unterminated source destination string");
    return {input.data(), static_cast<std::size_t>(end - input.begin())};
}
unsigned char lower(unsigned char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<unsigned char>(c + 32u) : c;
}
bool same_ascii(std::string_view a, std::string_view b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (lower(static_cast<unsigned char>(a[i])) != lower(static_cast<unsigned char>(b[i])))
            return false;
    return true;
}
class Parser
{
    SourceBootConfigState &state_;
    const SourceAllocationRef &allocation_;
    std::span<const std::uint8_t> bytes_;

  public:
    std::optional<std::size_t> cursor; // one local [EBP-4], retained across failed key lookups
    Parser(SourceBootConfigState &state, const SourceAllocationRef &allocation, std::span<const std::uint8_t> bytes)
        : state_(state), allocation_(allocation), bytes_(bytes)
    {
    }
    unsigned char byte(std::size_t at, Word site) const
    {
        check(at < bytes_.size(), site, "Source parser reads past supplied allocation");
        return bytes_[at];
    }
    std::optional<std::size_t> find(std::optional<std::size_t> from, std::string_view needle, Word site) const
    {
        check(from.has_value(), site, "Original strstr receives a null source pointer");
        for (std::size_t at = *from;; ++at)
        {
            if (needle.empty())
                return at;
            const auto first = byte(at, site);
            if (first == 0)
                return std::nullopt;
            std::size_t i = 0;
            while (i < needle.size() && byte(at + i, site) == static_cast<unsigned char>(needle[i]))
                ++i;
            if (i == needle.size())
                return at;
        }
    }
    void discarded_game_settings_probe()
    {
        // 42D2F1 passes EDX=0 and discards EAX. This probe has no stores.
        const auto at = find(0, "-GAMESETTINGS", 0x42c58bu);
        const auto &boundary = state_.boundary;
        if (!boundary.is_null)
        {
            if (!at)
                return;
            if (boundary.allocation == allocation_ && *at > boundary.offset)
                return;
            // A later CFG allocation has another host generation. Original
            // pointer ordering then chooses only between return0 and the
            // read-only strstr below followed by return1. The caller observes
            // neither result. Admit only a bounded read, without inventing
            // an x86 address order or clearing the retained589624 word.
        }
        (void)find(at, "=", 0x42c5b2u);
    }
    bool key(std::string_view name, bool publish = true)
    {
        const auto at = find(0, name, 0x42c58bu);
        const auto &boundary = state_.boundary;
        if (!boundary.is_null)
        {
            if (!at)
                return false; // 0 is below every non-null source pointer, then null test rejects
            check(boundary.allocation == allocation_, 0x42c59du,
                  "Unrecovered x86 pointer ordering between different allocation generations");
            if (*at > boundary.offset)
                return false;
        }
        const auto equal = find(at, "=", 0x42c5b2u);
        if (publish)
        {
            check(equal.has_value(), 0x42c5beu, "Original key value scan dereferences null equals pointer");
            auto value = *equal;
            while (byte(value, 0x42c5beu) != ' ')
                ++value;    // source does NOT stop at NUL/CR/LF
            cursor = value; // 42C5CC only after literal space reached
        }
        return true;
    }
    void publish_end()
    {
        const auto end = find(0, "-END", 0x42d2fcu);
        state_.boundary = end ? SourceBootSearchBoundary{allocation_, static_cast<Word>(*end), false}
                              : SourceBootSearchBoundary{}; // 42D309
    }
    std::size_t value_start(Word site) const
    {
        check(cursor.has_value(), site, "Original parser reads unwritten local value pointer");
        auto at = *cursor;
        for (;; ++at)
        {
            const auto c = byte(at, site);
            if (c != ' ' && c != '\r' && c != '\n')
                return at;
        }
    }
    template <std::size_t N> void token(std::array<char, N> &output) const
    {
        auto at = value_start(0x42c5ebu);
        std::size_t i = 0;
        for (;; ++at, ++i)
        {
            const auto c = byte(at, 0x42c600u);
            if (c == ' ' || c == '\r' || c == '\n')
                break; // NUL and TAB are copied, not delimiters
            check(i < N, 0x42c61bu, "Original token overflows admitted destination owner");
            output[i] = static_cast<char>(c);
        }
        check(i < N, 0x42c62du, "Original token terminator overflows destination owner");
        output[i] = 0;
    }
    Word number() const
    {
        auto at = value_start(0x42c6a9u);
        auto c = byte(at, 0x4631fcu);
        while (c == ' ' || (c >= '\t' && c <= '\r'))
            c = byte(++at, 0x4631fcu);
        const bool negative = c == '-';
        if (negative || c == '+')
            c = byte(++at, 0x46323bu);
        Word result = 0;
        while (c >= '0' && c <= '9')
        {
            result = result * 10u + c - static_cast<unsigned char>('0');
            c = byte(++at, 0x46326cu);
        }
        return negative ? 0u - result : result;
    }
};
Word initialized(const std::optional<Word> &value, Word site)
{
    check(value.has_value(), site, "Original Game.cfg code reads an unwritten local display value");
    return *value;
}
} // namespace
SourceBootConfigState::SourceBootConfigState()
{
    text_store(base_path, "D:\\Game\\", 0);
    text_store(graphics_path, "D:\\Game\\Graphics24\\", 0);
    text_store(maximum_graphics_path, "D:\\Game\\Graphics24Max\\", 0); // PE48499C..484A1B
    text_store(sound_path, "D:\\Game\\Sound", 0);
}
void apply_source_boot_settings_42d2e0(SourceBootConfigWorld world, SourceAllocationBank &bank,
                                       const SourceAllocationRef &allocation)
{
    auto &state = world.config;
    Parser parser(state, allocation, bank.bytes(allocation));
    parser.discarded_game_settings_probe(); // 42D2F1: EDX0, discarded result, no stores
    parser.publish_end();
    if (parser.key("GAMELOCATION"))
        parser.token(state.game_location);
    const std::string location(c_string(state.game_location, 0x42d33bu));
    text_store(state.base_path, location + "\\", 0x42d33bu);
    text_store(state.graphics_path, location + "\\Graphics\\", 0x42d34fu);
    text_store(state.sound_path, location + "\\Sound", 0x42d363u);
    if (parser.key("CONTROLMETHOD"))
    {
        std::array<char, 64> value{};
        parser.token(value);
        world.control_method = 2; // 42D396, after token stores
        const auto name = c_string(value, 0x42d3a6u);
        if (same_ascii(name, "Steering_Analog"))
            world.control_method = 0;
        else if (same_ascii(name, "Joystick_Analog"))
            world.control_method = 1;
        else if (same_ascii(name, "Keyboard"))
            world.control_method = 2;
        else if (same_ascii(name, "Pad_Digital"))
            world.control_method = 4;
        else if (same_ascii(name, "Pad_Analog"))
            world.control_method = 3;
    }
    std::optional<Word> width, height, depth; // unwritten original locals, never replaced by defaults
    if (parser.key("DISPLAYRESWIDTH"))
        width = parser.number();
    if (parser.key("DISPLAYRESHEIGHT"))
        height = parser.number();
    if (parser.key("DISPLAYRESDEPTH"))
        depth = parser.number();
    const auto w = initialized(width, 0x42d4a5u);
    if (std::bit_cast<std::int32_t>(w) >= 320 && std::bit_cast<std::int32_t>(w) <= 2048)
    {
        const auto h = initialized(height, 0x42d4b8u);
        if (std::bit_cast<std::int32_t>(h) >= 200 && std::bit_cast<std::int32_t>(h) <= 2048)
        {
            state.display_height = h; // 42D4C9 precedes the possibly unwritten depth read!
            const auto d = initialized(depth, 0x42d4ceu);
            state.display_width = w; // 42D4D1
            state.display_depth = d; // 42D4D7, depth is not range-checked
        }
    }
    (void)parser.key("SOUNDQUALITY");
    const auto rate = parser.number();
    (void)parser.key("MASTERVOLUME");
    const auto master = parser.number();
    (void)parser.key("SFXVOLUME");
    const auto effects = parser.number();
    (void)parser.key("MUSVOLUME");
    const auto music = parser.number();
    world.audio.configure_460c20(master, effects, music, rate);
    (void)parser.key("LEVELSCRIPT");
    parser.token(state.level_script);
    state.device_index = 0;
    state.driver_index = 1;
    state.uv_fix = 0; // 42D58B/591/59B before lookup
    if (parser.key("D3D_DEVICEINDEX"))
        state.device_index = parser.number();
    if (parser.key("D3D_DRIVERINDEX"))
        state.driver_index = parser.number();
    if (parser.key("D3D_UVFIX"))
        state.uv_fix = parser.number();
    world.disable_frame_correction = 0;
    if (parser.key("DISABLE_FRAMERATE_CORRECTION"))
        world.disable_frame_correction = parser.number();
    state.display_boot_graphics = 0;
    if (parser.key("DISPLAY_BOOT_GFX"))
        state.display_boot_graphics = parser.number();
}
void initialize_source_boot_config_42d660(SourceBootConfigWorld world, SourceAllocationBank &bank,
                                          SourceBootFilePlatform &files)
{
    // First44EA8C invocation uses the PE-zero bank; the second at44214D
    // follows442143 initialization. Neither CFG invocation initializes the bank.
    std::string_view path = "Game.cfg";
    if (source_boot_file_size_44b3b0(world.config, files, path) == 0)
    {
        path = "D:\\Game\\Game.cfg";
        if (source_boot_file_size_44b3b0(world.config, files, path) == 0)
        {
            path = "C:\\Game\\Game.cfg";
            (void)source_boot_file_size_44b3b0(world.config, files, path); // third result ignored
        }
    }
    const Word length = source_boot_file_size_44b3b0(world.config, files, path); // always repeated
    check(length <= std::numeric_limits<Word>::max() - 32u, 0x42d6cau,
          "Game.cfg size+32 wraps source allocation request");
    const auto allocation = bank.allocate_42e8d0(length + 32u, 0);
    check(static_cast<bool>(allocation), 0x42d6d2u, "Original CFG caller would use null allocation result");
    (void)source_boot_file_read_44b260(world.config, files, path, length, bank.bytes(allocation));
    // 42D060 at42D6E6 is the pinned literal C3 RET; ClogMeister argument has no effect.
    apply_source_boot_settings_42d2e0(world, bank, allocation);
    (void)bank.release_42e980(allocation); // 42D6F4 reached only after successful parser return
    // A host source-domain rejection preserves bank ownership and prior writes.
}
} // namespace sgn::retail
