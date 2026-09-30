#pragma once
#include "sgn/host_graphics_settings.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>

// NEW HOST (R24) shared GRAPHICS menu model. Not recovered game behaviour.
// One row list, one value stepping rule, one localized string table and one
// session save, used by BOTH the Options GRAPHICS page (host_graphics_page.cpp)
// and the in-race pause GRAPHICS panel (retail_pause_menu.cpp), so the two menus
// cannot diverge. Values are always read from host_graphics::current() at the
// moment a caller composes a row (never cached): Alt+Enter and the other menu
// change settings through host_graphics::set() as well.
namespace sgn::host_graphics_menu
{

// ---- Session (registered by main.cpp before the menu runs) ----
// Fields whose session value came from a command-line/environment override.
// They are applied for this session only: an overridden field the user has not
// changed in a GRAPHICS menu keeps its persisted file value when saving.
// display_mode has no command-line override.
struct SessionOverrides
{
    bool interpolation{}, vsync{}, original_4x3{};
};
void begin_session(const std::filesystem::path &game_root, const host_graphics::Settings &persisted,
                   SessionOverrides overrides);
// Appends one line to <root>/reports/source-menu-session.log (host diagnostics).
void report(const std::filesystem::path &game_root, std::string_view line);
// Leaving a GRAPHICS menu: writes default_path(root) with the user's values
// (untouched session overrides keep the file's values). Requires begin_session.
void save_settings();

// ---- Rows ----
// Storage order (indexes the string tables). New rows are appended before
// reset; the order the menus show is display_order below.
enum class Row : std::uint8_t
{
    display_mode,
    interpolation,
    vsync,
    aspect,
    anti_aliasing,
    texture_filter,
    mipmaps,
    alpha_to_coverage,
    fxaa,
    smooth_gradients,        // R27
    texture_edge_fix,        // R27
    frame_rate_limit,        // R27
    low_latency,             // R27
    high_precision_textures, // R28
    sharp_hud,               // R28
    texture_sharpness,       // R28
    smooth_magnification,    // R28
    bloom,                   // R28
    smooth_effects,          // R28
    reset,
};
inline constexpr std::uint32_t row_count = 20; // display_mode..reset
// R28 menu order, most used first: display and timing, then anti-aliasing and
// texture filtering, then image effects, then the texture-load options (which
// take effect on the next track); RESET last (the pause panel adds BACK).
inline constexpr std::array<Row, row_count> display_order{
    Row::display_mode,
    Row::interpolation,
    Row::vsync,
    Row::frame_rate_limit,
    Row::low_latency,
    Row::aspect,
    Row::anti_aliasing,
    Row::alpha_to_coverage,
    Row::fxaa,
    Row::texture_filter,
    Row::texture_sharpness,
    Row::smooth_magnification,
    Row::smooth_gradients,
    Row::bloom,
    Row::smooth_effects,
    Row::mipmaps,
    Row::high_precision_textures,
    Row::texture_edge_fix,
    Row::sharp_hud,
    Row::reset,
};
// Menu position (0..row_count-1) -> row.
constexpr Row row_at(std::uint32_t index)
{
    return display_order.at(index);
}
// True for rows with a value (every row except reset).
constexpr bool has_value(Row row)
{
    return row != Row::reset;
}
// Enter / right (+1) or left (-1). Value rows step with wrap-around, reset
// restores Settings{}; then host_graphics::set() (applied live by consumers).
void change(Row row, int direction);
// Settings that together show every value of every row (one field varied
// at a time); menus measure them so their layout never depends on the
// current value.
std::span<const host_graphics::Settings> value_variants();

// ---- Localized strings (the 7 languages of the retail languagePC.csv) ----
struct Language;
// Retail catalog language: by its LANGUAGE NAME column (frontend 5E37BC+36n),
// or by its LANGUAGE FLAG (5E37B8 / race 4835E0; retail flags 1..7 in file
// column order). Unknown names/flags select English.
const Language &language_for_catalog_name(std::u8string_view name);
const Language &language_for_flag(std::uint32_t flag);

std::u8string_view options_row_label(const Language &); //"GRAPHICS" row in Options/pause
std::u8string_view options_row_help(const Language &);
std::u8string_view title(const Language &);
std::u8string_view back_label(const Language &); // pause panel "BACK"
// R29: the pause row shown in CAMERA's slot ("ADVANCED"), opening the ADVANCED page.
std::u8string_view advanced_label(const Language &);
std::u8string_view label(const Language &, Row);
std::u8string_view help(const Language &, Row); // mipmaps help carries the next-track note
std::u8string_view separator(const Language &); //": " (French " : ")
// Short uppercase note for the pause panel (MIPMAPS: applies from the next
// track); empty for every other row.
std::u8string_view pause_note(const Language &, Row);

// Frontend and race fonts are one byte per glyph in the retail file's
// Latin-1 code page. Appends UTF-8 text (only U+0001..U+00FF) to a NUL-free
// byte buffer; throws when the text leaves Latin-1 or the buffer.
class Latin1Text
{
    std::span<std::uint8_t> out_;
    std::size_t size_{};

  public:
    explicit Latin1Text(std::span<std::uint8_t> out);
    Latin1Text &append(std::u8string_view text);
    Latin1Text &append_value(const Language &, Row, const host_graphics::Settings &);
    // "LABEL: VALUE" (Options page row) or "LABEL" for reset.
    Latin1Text &append_row(const Language &, Row, const host_graphics::Settings &);
    std::span<std::uint8_t> finish(); // NUL-terminated, span includes the NUL
};
} // namespace sgn::host_graphics_menu
