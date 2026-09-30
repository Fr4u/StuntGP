#include "sgn/retail_pause_menu.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/retail_camera_vehicle.hpp"
#include "sgn/host_wide_race_anchors.hpp"
#include "sgn/host_graphics_menu_model.hpp"
#include "sgn/host_menu_arrow.hpp"
#include "sgn/menu_keyboard_acquisition.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <vector>

namespace sgn::retail
{
namespace
{
template <class T> T read(const T &value)
{
    return static_cast<const volatile T &>(value);
}
template <class T> void store(T &target, T value)
{
    static_cast<volatile T &>(target) = value;
}
using sgn::bit_reinterpret::as_signed;
using sgn::bit_reinterpret::as_word;
using sgn::bit_reinterpret::compile_time::as_float;
float stored(double value)
{
    if (!std::isfinite(value) || value > std::numeric_limits<float>::max() ||
        value < -std::numeric_limits<float>::max())
        throw FormatError("pause menu arithmetic outside finite binary32 domain");
    return static_cast<float>(value);
}
PauseMenuRow &row(PauseMenuDrawWorld &w, std::uint32_t index)
{
    if (index >= w.menu.rows.size())
        throw FormatError("pause menu row outside source table");
    return w.menu.rows[index];
}
unsigned physical_slot(std::uint32_t token)
{
    if (token < 1 || token > 4)
        throw FormatError("pause menu accessed null or unknown physical input");
    return token - 1;
}
const PauseMenuInputEdges &edges(PauseMenuDrawWorld &w, std::uint32_t token)
{
    return w.menu.physical_edges[physical_slot(token)];
}
std::span<const std::uint8_t> text_pointer(PauseMenuDrawWorld &w, std::uint32_t index)
{
    // Resolve the source pointer once at the original read boundary, without
    // reading an unaccessed suffix or copying the arena into a string bank.
    const auto &text = w.game_text;
    if (index >= text.offsets.size())
        throw FormatError("pause menu text pointer outside source arena");
    // The table load is not a dereference of its target. An unresolved/null
    // target remains unread on4291F0's vertical-clip return. Empty span is a
    // reached-byte boundary, not a fabricated NUL string.
    if (!text.offsets[index] || *text.offsets[index] >= text.arena.size())
        return {};
    const auto offset = *text.offsets[index];
    return {reinterpret_cast<const std::uint8_t *>(text.arena.data() + offset), text.arena.size() - offset};
}
void sound(PauseMenuDrawWorld &w, std::uint32_t id)
{
    w.actions.sound_461150(id);
}
void call_callback(PauseMenuDrawWorld &w, std::uint32_t identity)
{
    w.actions.invoke(identity);
}
void text(PauseMenuDrawWorld &w, std::span<const std::uint8_t> value, std::uint32_t alignment, float x, float y,
          std::uint32_t color, float scale)
{
    submit_text_4291f0(w.drawing.borrow_text(), value, alignment, {x, y, as_float(0x3f7ae148u), color, scale, 6},
                       w.sink);
}
void quad(PauseMenuDrawWorld &w)
{
    const auto t = w.drawing.borrow_text();
    const auto alpha = w.drawing.borrow_alpha_vertices();
    if (alpha.size() < 4)
        throw FormatError("pause menu quad needs four shared alpha vertices");
    submit_quad_432e00({w.descriptor, alpha.first<4>(), t.selected_material, t.materials, t.viewport}, w.sink);
}
void frame(PauseMenuDrawWorld &w, float x, float width)
{
    const auto t = w.drawing.borrow_text();
    const auto alpha = w.drawing.borrow_alpha_vertices();
    submit_pause_frame_447280({w.line_clip, alpha, t.viewport, t.specular_enabled, t.selected_material}, x, 120, width,
                              240, w.sink);
}
void meter(PauseMenuDrawWorld &w, std::uint32_t value, float x, float y)
{
    const auto t = w.drawing.borrow_text();
    const auto alpha = w.drawing.borrow_alpha_vertices();
    submit_pause_meter_446d90({t, w.descriptor, alpha, w.line_clip, w.meter_labels}, value, x, y, 64, 12, w.sink);
}

// 446C20's stores before its row loop, verbatim: row 1/6 enable, 60F020 ->
// row 6 value text, row 2 text and the 61B750 pulse phase. 446C20 runs it
// first; the ADVANCED page (which replaces 446C20's drawing) runs it too, so
// every source word evolves exactly as on an original pause frame.
void update_pause_rows_446c20(PauseMenuDrawWorld &w)
{
    store(w.menu.rows[1].enabled, read(w.render_b) != 1u ? 1u : 0u);
    if (read(w.clock.game_mode) == 3u)
        store(w.menu.rows[6].enabled, 0u);
    else
    {
        const auto split = read(w.split_setting);
        store(w.menu.rows[6].enabled, 1u); // 446C4F before split-dependent clear
        if (split != 0u)
            store(w.menu.rows[6].enabled, 0u);
    }
    const auto camera_index = read(w.selected_camera_preset) - 1u;
    if (camera_index <= 3u)
        store(w.menu.rows[6].extra_text_id, camera_index + 51u); // 446B60
    const auto type = read(w.game_type);
    if (type != 2u)
        store(w.menu.rows[2].text_id, 11u); // includes intermediate11 for type5
    if (type == 2u || type == 5u)
        store(w.menu.rows[2].text_id, 10u);
    auto phase = read(w.menu.highlight) + 4u;
    store(w.menu.highlight, phase);
    if (as_signed(phase) > 127)
    {
        phase -= 255u;
        store(w.menu.highlight, phase);
    }
}
// ======================================================================
// NEW HOST (R24..R29) pause ADVANCED row and page. Not recovered behaviour.
// R29 layout: the pause box and its art are exactly the original (one 432E00
// quad, 240 high). The original CAMERA slot (row 6: x 12, y 191) shows
// "* ADVANCED" instead of "* CAMERA  <value>", so the list keeps the original
// 7 rows, positions and pitch. The source row table 485120..4851DF, every
// callback and every source word keep their original values; 446C20 still
// performs all its stores (row 1/6 enable, 60F020 -> row 6 value text, row 2
// text, 61B750 pulse) and only its drawing of row 6 is replaced.
// ADVANCED is a host row: it is always present (also when the source disables
// the camera row: time trial 612A78 mode 3 or split screen), and while it is
// selected 61B74C keeps the source row (0..5) it was reached from.
// ADVANCED opens a page inside the same box: the original CAMERA row first
// (its GameText label/value and the unchanged 446BB0 callback, shown only
// while the source enables row 6), then the shared GRAPHICS rows, then BACK.
// Original constants used (see STATUS.txt): rows x = box + 12 (row+8),
// y = 120 + row+0C, pitch 20; value column main x + 0x430E6667 (142.4);
// right inner margin box + width - 12 (4475FB/446D90); text call 4291F0
// (alignment 0, depth 0x3F7AE148, scale 1, flags 6); "* " bullet of the
// GameText rows; 446C20 pulse colour of 61B750; sounds 48/49 (447100).
// ======================================================================
namespace menu = sgn::host_graphics_menu;
constexpr std::uint32_t camera_row = 6;                           // source CAMERA row (446BB0), ADVANCED's slot
constexpr std::uint32_t host_row_index = camera_row;              // ADVANCED in the host navigation list
constexpr float row_x = 12;                                       // initial_pause_menu_rows(): row+8 of every row
constexpr float row_pitch = 20;                                   // 131 -> 151 -> 171 -> 191
constexpr float first_page_row_y = 51 + row_pitch;                // page rows 71..191 (slot 51 holds the up arrow)
constexpr float up_arrow_slot_y = 51;                             // RESUME row+0C
constexpr float down_arrow_slot_y = 191 + row_pitch;              // one pitch below CAMERA, inside the box
constexpr float text_half_height = 8;                             // arrow centre in a 20-unit row slot
constexpr float value_column = std::bit_cast<float>(0x430e6667u); // 446C20 CLOSE offset 142.4
constexpr float right_inner_margin = 12;                          // box right - slider end (4475FB/446D90)
constexpr float label_value_gap = 4;                              // space kept before the value column
constexpr std::uint32_t page_visible_rows = 7;                    // rows 71..191 of the original box
using HostText = std::array<std::uint8_t, 160>;
bool host_bound(PauseMenuDrawWorld &w)
{
    const auto &view = w.menu.host_graphics;
    if (view.state && !view.input)
        throw FormatError("pause ADVANCED row needs its left/right input owner");
    return view.state != nullptr;
}
PauseGraphicsHostState &host(PauseMenuDrawWorld &w)
{
    return *w.menu.host_graphics.state;
}
bool host_row_selected(PauseMenuDrawWorld &w)
{
    return host_bound(w) && host(w).row_selected;
}
bool host_panel_open(PauseMenuDrawWorld &w)
{
    return host_bound(w) && host(w).panel_open;
}
const menu::Language &host_language(PauseMenuDrawWorld &w)
{
    // 4835E0 is the retail LANGUAGE FLAG (1..7), the same flag that selects the
    // race GameText column and the wide-box languages 4/6 in 4473F0.
    return menu::language_for_flag(read(w.menu.requested_language));
}
bool pressed(const std::uint8_t &current, const std::uint8_t &previous)
{
    return read(current) != 0 && read(previous) == 0;
}
std::uint32_t pulse_color(std::uint32_t pulse)
{ // 446C20 selected-row colour of the 61B750 phase
    const auto sign = as_signed(pulse) < 0 ? 0xffffffffu : 0u;
    auto magnitude = ((pulse ^ sign) - sign) << 1;
    if (as_signed(magnitude) > 255)
        magnitude = 255;
    return (magnitude | 0xffffff00u) << 16;
}
float source_box_width(PauseMenuDrawWorld &w)
{ // 4473F0: 256, or 323.2 for language flags 4/6
    const auto language = read(w.menu.requested_language);
    return as_float(language == 4u || language == 6u ? 0x43a1999au : 0x43800000u);
}
bool source_row_enabled(PauseMenuDrawWorld &w, std::uint32_t index)
{
    return read(row(w, index).enabled) != 0;
}
// ---- page list: [CAMERA while the source enables row 6] + GRAPHICS rows + BACK
enum class EntryKind
{
    camera,
    graphics,
    back
};
struct Entry
{
    EntryKind kind{};
    std::uint32_t graphics_index{}; // menu position for graphics rows
};
bool camera_available(PauseMenuDrawWorld &w)
{
    return read(row(w, camera_row).enabled) == 1u; // exactly the 446C20 draw condition of row 6
}
std::uint32_t entry_count(PauseMenuDrawWorld &w)
{
    return (camera_available(w) ? 1u : 0u) + menu::row_count + 1u;
}
Entry entry_at(PauseMenuDrawWorld &w, std::uint32_t index)
{
    const auto camera = camera_available(w) ? 1u : 0u;
    if (index >= entry_count(w))
        throw FormatError("pause ADVANCED entry outside its list");
    if (index < camera)
        return {EntryKind::camera, 0};
    if (index - camera < menu::row_count)
        return {EntryKind::graphics, index - camera};
    return {EntryKind::back, 0};
}
void keep_selection_visible(PauseMenuDrawWorld &w)
{
    auto &h = host(w);
    const auto count = entry_count(w);
    if (h.panel_row >= count)
        h.panel_row = count - 1u;
    if (h.panel_row < h.first_visible)
        h.first_visible = h.panel_row;
    else if (h.panel_row >= h.first_visible + page_visible_rows)
        h.first_visible = h.panel_row + 1u - page_visible_rows;
    if (h.first_visible + page_visible_rows > count)
        h.first_visible = count > page_visible_rows ? count - page_visible_rows : 0u;
}
// True when this frame's navigation or selection involves ADVANCED; otherwise
// the unchanged source 447100 runs. The host list is source rows 0..5 plus
// ADVANCED in row 6's place, so source navigation never lands on row 6.
bool host_navigation_involved(PauseMenuDrawWorld &w)
{
    if (!host_bound(w))
        return false;
    if (host(w).row_selected)
        return true;
    const auto input = read(w.physical_input.token);
    if (input == 0)
        return false;
    const auto &first = edges(w, input);
    const auto selected = read(w.menu.selected_row);
    if (selected >= camera_row)
        throw FormatError("pause selected row outside source rows 0..5 with ADVANCED bound");
    if (pressed(first.up, first.up_previous))
    { // source up wraps from the first enabled row to row 6
        for (auto index = selected; index-- > 0u;)
            if (source_row_enabled(w, index))
                return false;
        return true;
    }
    if (!pressed(first.down, first.down_previous))
        return false;
    for (auto index = selected + 1u; index < camera_row; ++index)
        if (source_row_enabled(w, index))
            return false;
    return true; // source down would reach row 6 (CAMERA) or wrap
}
void open_panel(PauseMenuDrawWorld &w)
{
    auto &h = host(w);
    h.panel_open = true;
    h.panel_row = 0;
    h.first_visible = 0;
}
void close_panel(PauseMenuDrawWorld &w)
{
    host(w).panel_open = false; // ADVANCED stays highlighted
    menu::save_settings();      // session overrides are never persisted
}
// 447100 over source rows 0..5 plus ADVANCED in row 6's place: identical
// sounds (48/49), debounce (61B748), back/resume 446B40 and entry-callback
// rule; ADVANCED opens the page instead of calling 446BB0.
void dispatch_pause_input_with_host_row(PauseMenuDrawWorld &w)
{
    auto &h = host(w);
    constexpr std::uint32_t list = camera_row + 1u; // 0..5 source, 6 ADVANCED
    const bool entry_host = h.row_selected;
    auto candidate = entry_host ? host_row_index : read(w.menu.selected_row);
    const auto entry_callback = entry_host ? 0u : read(row(w, candidate).callback);
    auto input = read(w.physical_input.token);
    if (input == 0)
        return;
    const auto enabled = [&](std::uint32_t index) { return index == host_row_index || source_row_enabled(w, index); };
    const auto &first = edges(w, input);
    bool navigated = false;
    if (pressed(first.up, first.up_previous))
    {
        sound(w, 48);
        bool found = false;
        for (unsigned attempts = 0; attempts < list; ++attempts)
        {
            candidate = candidate == 0u ? host_row_index : candidate - 1u;
            if (enabled(candidate))
            {
                found = true;
                break;
            }
        }
        if (!found)
            throw FormatError("pause up navigation has no enabled row");
        h.row_selected = candidate == host_row_index;
        if (!h.row_selected)
            store(w.menu.selected_row, candidate); // source publishes only the accepted row
        navigated = true;
    }
    else if (pressed(first.down, first.down_previous))
    {
        sound(w, 48);
        bool found = false;
        for (unsigned attempts = 0; attempts < list; ++attempts)
        {
            candidate = (candidate + 1u) % list;
            if (candidate != host_row_index)
                store(w.menu.selected_row, candidate); // source publishes rejected rows too
            if (enabled(candidate))
            {
                found = true;
                break;
            }
        }
        if (!found)
            throw FormatError("pause down navigation has no enabled row");
        h.row_selected = candidate == host_row_index;
        navigated = true;
    }
    if (navigated)
        input = read(w.physical_input.token);
    const auto release = read(w.menu.release_count);
    const auto &action = edges(w, input);
    if (release > 1u)
    {
        if (pressed(action.back, action.back_previous))
        {
            sound(w, 49);
            call_callback(w, 0x446b40);
            return;
        }
        if (pressed(action.activate, action.activate_previous) &&
            (candidate == host_row_index || read(row(w, candidate).callback) != 0))
        {
            sound(w, 49);
            if (entry_host)
                open_panel(w);
            else
                call_callback(w, entry_callback); // 447241 entry row, as the source
            return;
        }
    }
    if (read(action.back) != 0 || read(action.back_previous) != 0)
        store(w.menu.release_count, 0u);
    else if (release <= 1u)
        store(w.menu.release_count, release + 1u);
}
void dispatch_panel_input(PauseMenuDrawWorld &w)
{
    auto &h = host(w);
    const auto input = read(w.physical_input.token);
    if (input == 0)
        return;
    const auto count = entry_count(w);
    keep_selection_visible(w);
    const auto &e = edges(w, input);
    const auto &extra = w.menu.host_graphics.input->physical[physical_slot(input)].extra_edges;
    if (pressed(e.up, e.up_previous))
    {
        sound(w, 48);
        h.panel_row = (h.panel_row + count - 1u) % count;
        keep_selection_visible(w);
        return;
    }
    if (pressed(e.down, e.down_previous))
    {
        sound(w, 48);
        h.panel_row = (h.panel_row + 1u) % count;
        keep_selection_visible(w);
        return;
    }
    const auto entry = entry_at(w, h.panel_row);
    if (pressed(e.back, e.back_previous))
    {
        sound(w, 49);
        close_panel(w);
        return;
    }
    const bool left = pressed(extra.current_1a8, extra.previous_1af);
    const bool right = pressed(extra.current_1a9, extra.previous_1b0);
    const bool activate = pressed(e.activate, e.activate_previous);
    switch (entry.kind)
    {
    case EntryKind::camera:
        // The original CAMERA row: Enter runs the unchanged 446BB0 through the
        // same callback path and sound as 447100. The original has no backward
        // step, so Right does the same forward step and Left does nothing.
        if (activate || (right && !left))
        {
            sound(w, 49);
            call_callback(w, 0x446bb0);
        }
        return;
    case EntryKind::back:
        if (activate)
        {
            sound(w, 49);
            close_panel(w);
        }
        return;
    case EntryKind::graphics: {
        const auto row_id = menu::row_at(entry.graphics_index);
        if (activate)
        {
            sound(w, 49);
            menu::change(row_id, 1);
        }
        else if (menu::has_value(row_id) && left != right)
        {
            sound(w, 48);
            menu::change(row_id, right ? 1 : -1);
        }
        return;
    }
    }
}
void reset_host_on_pause_end(PauseMenuDrawWorld &w)
{
    if (!host_bound(w))
        return;
    auto &h = host(w);
    h.row_selected = false;
    if (h.panel_open)
        close_panel(w); // pause ended while the page was open
}
// Scale that fits `value` (NUL-terminated, `glyphs` bytes before the NUL) into
// `room` units: 1 (the original row scale) whenever it fits. 4290D0 returns
// glyph advances * scale + glyph count, so the count is not scaled.
float fit_scale(PauseMenuDrawWorld &w, std::span<const std::uint8_t> value, double room)
{
    const auto glyphs = static_cast<double>(value.size() - 1);
    const auto natural = measure_text_4290d0(w.drawing.borrow_text().fonts, value, 1);
    if (natural <= room)
        return 1;
    if (natural <= glyphs || room <= glyphs)
        throw FormatError("pause ADVANCED text cannot fit its original column");
    return stored((room - glyphs) / (natural - glyphs));
}
// One row in the original style: "* LABEL" at x+12 and, for value rows, the
// value left-aligned in the CLOSE column, each kept inside its column.
void draw_row(PauseMenuDrawWorld &w, float x, float width, float y, std::u8string_view label,
              std::span<const std::uint8_t> value, std::uint32_t color)
{
    const auto main_x = stored(double(row_x) + x);
    HostText buffer;
    menu::Latin1Text text_bytes(buffer);
    text_bytes.append(u8"* ").append(label);
    const auto label_bytes = text_bytes.finish();
    const auto label_room = value.empty() ? double(width) - double(row_x) - double(right_inner_margin)
                                          : double(value_column) - double(label_value_gap);
    text(w, label_bytes, 0, main_x, y, color, fit_scale(w, label_bytes, label_room));
    if (value.empty())
        return;
    const auto value_x = stored(double(main_x) + double(value_column));
    const auto value_room = double(x) + double(width) - double(right_inner_margin) - double(value_x);
    text(w, value, 0, value_x, y, color, fit_scale(w, value, value_room));
}
// ADVANCED in row 6's slot: the row's own x/y words, the same text call.
void draw_advanced_row(PauseMenuDrawWorld &w, float x, float width, float y)
{
    auto &entry = row(w, camera_row);
    const auto color = host(w).row_selected ? pulse_color(read(w.menu.highlight)) : 0xffffffffu;
    const auto main_x = stored(double(read(entry.x)) + x);
    const auto main_y = stored(double(read(entry.y)) + y);
    HostText buffer;
    menu::Latin1Text label(buffer);
    label.append(u8"* ").append(menu::advanced_label(host_language(w)));
    const auto bytes = label.finish();
    text(w, bytes, 0, main_x, main_y, color, fit_scale(w, bytes, double(width) - double(row_x) - right_inner_margin));
}
host_menu_arrow::Restore arrow_restore(const TextSubmission &t)
{
    // The polygon device after the preceding 4291F0 rows (432380 end state):
    // ALPHAREF 1 with specular (quality 1, no legacy restriction), else 160;
    // the texture of the material 61C5E4 names.
    const bool specular = read(t.quality) == 1u && read(t.legacy_driver_restriction) != 1u;
    const auto material = read(t.selected_material);
    return {material < t.materials.size() ? t.materials[material].primary_texture : 0u, specular ? 1u : 160u};
}
void draw_page(PauseMenuDrawWorld &w, float x, float width, float y)
{
    auto &h = host(w);
    keep_selection_visible(w);
    const auto &l = host_language(w);
    const auto settings = host_graphics::current(); // read at every draw: never a cached value
    const auto count = entry_count(w);
    for (std::uint32_t slot = 0; slot < page_visible_rows && h.first_visible + slot < count; ++slot)
    {
        const auto index = h.first_visible + slot;
        const auto color = index == h.panel_row ? pulse_color(read(w.menu.highlight)) : 0xffffffffu;
        const auto row_y = stored(double(first_page_row_y + row_pitch * static_cast<float>(slot)) + y);
        const auto entry = entry_at(w, index);
        if (entry.kind == EntryKind::camera)
        { // exactly 446C20's row 6 text and value (GameText 15 / 51..54 via row words)
            auto &camera = row(w, camera_row);
            const auto main_x = stored(double(read(camera.x)) + x);
            text(w, text_pointer(w, read(camera.text_id)), 0, main_x, row_y, color, 1);
            const auto extra = read(camera.extra_text_id);
            if (extra != 0)
                text(w, text_pointer(w, extra), 0, stored(double(main_x) + as_float(0x430e6667u)), row_y, color, 1);
            continue;
        }
        if (entry.kind == EntryKind::back)
        {
            draw_row(w, x, width, row_y, menu::back_label(l), {}, color);
            continue;
        }
        const auto row_id = menu::row_at(entry.graphics_index);
        HostText value_buffer;
        std::span<const std::uint8_t> value;
        if (menu::has_value(row_id))
        {
            menu::Latin1Text value_text(value_buffer);
            value_text.append_value(l, row_id, settings);
            value = value_text.finish();
        }
        draw_row(w, x, width, row_y, menu::label(l, row_id), value, color);
    }
    // Scroll arrows, centred in the box: above the first shown row (slot 51)
    // and below the last one (slot 211, inside the original box).
    std::vector<host_menu_arrow::Mesh> arrows;
    const float center_x = x + width * 0.5f, feather = host_menu_arrow::one_pixel_feather();
    if (h.first_visible > 0)
        arrows.push_back(host_menu_arrow::build(host_menu_arrow::Direction::up, center_x,
                                                y + up_arrow_slot_y + text_half_height, host_menu_arrow::default_width,
                                                feather));
    if (h.first_visible + page_visible_rows < count)
        arrows.push_back(host_menu_arrow::build(host_menu_arrow::Direction::down, center_x,
                                                y + down_arrow_slot_y + text_half_height,
                                                host_menu_arrow::default_width, feather));
    if (!arrows.empty())
    {
        const auto t = w.drawing.borrow_text();
        host_menu_arrow::draw(w.sink, t.viewport, arrows, as_float(0x3f7ae148u), arrow_restore(t));
    }
}
} // namespace

void validate_pause_menu_bindings(const PauseMenuWorld &w)
{
    const auto &life = w.lifecycle;
    if (&w.audio.level != &life.level_audio || &w.audio.controls != &life.hud.controls ||
        &w.streaming.audio.state != &w.audio.state || &w.streaming.audio.device != &w.audio.device ||
        &w.streaming.audio.level != &life.level_audio || &w.streaming.audio.controls != &life.hud.controls ||
        &w.streaming.audio.split_setting != &w.audio.split_setting ||
        &life.vehicle_audio.driver.state != &w.audio.state || &life.vehicle_audio.driver.device != &w.audio.device ||
        life.vehicles.data() != w.vehicles.physics.data() || &life.music.output_a != &life.level_audio.output_a ||
        &life.music.playback_active != &w.streaming.stream.playback_active ||
        &life.music.field_e100 != &w.streaming.stream.field_e100 ||
        &life.music.gain_step != &w.streaming.music.gain_step || &w.transition.controls != &life.hud.controls ||
        &w.transition.frame_pause != &life.frame_pause || &w.transition.target != &w.options.selection_phase ||
        &w.results.options != &w.options || &w.results.game_type != &w.game_type ||
        &w.results.camera_preset != &w.selected_camera_preset || w.transition.overlays.data() != life.overlays.data())
        throw FormatError("pause menu bindings must borrow the same existing game owners");
}
void resume_pause_menu_446b40(PauseMenuWorld &w)
{
    resume_pause_442350(w.lifecycle);
    store(w.lifecycle.physical_input.token, 0u); // 446B47, separate second clear
    store(w.menu.release_count, 0u);             // 446B4C
}
void restart_pause_menu_446af0(PauseMenuWorld &w)
{
    stop_audio_music(w.streaming); // 446AF0, real45F9D0
    auto invocation = w.restart.borrow_restart();
    if (&invocation.world.clock != &w.clock || &invocation.world.vehicles != &w.vehicles ||
        &invocation.world.hud.controls != &w.lifecycle.hud.controls ||
        &invocation.world.selected_camera_preset != &w.selected_camera_preset ||
        &invocation.world.audio.level != &w.lifecycle.level_audio ||
        &invocation.world.audio.audio.state != &w.audio.state ||
        &invocation.world.audio.audio.device != &w.audio.device ||
        &invocation.world.frame_projection.matrix != &w.frame_projection.matrix ||
        &invocation.world.frame_projection.inverse_depth_scale != &w.frame_projection.inverse_depth_scale ||
        &invocation.world.frame_projection.depth_offset != &w.frame_projection.depth_offset ||
        &invocation.world.frame_projection.shared_model_identity != &w.frame_projection.shared_model_identity)
        throw FormatError("pause restart invocation must borrow the current menu world");
    (void)initialize_race(invocation.world, invocation.parameters); // 446AF5, full40D500
    // Host publication of the existing value-bearing child adapter, not an
    // extra recovered source store. Never defer this behind countdown/resume.
    if (&invocation.current_vehicle_output != &invocation.world.reset.current)
        store(invocation.current_vehicle_output, invocation.world.reset.current);
    store(w.clock.countdown_pending, 4u); // 446AFA
    resume_pause_442350(w.lifecycle);     // 446B04, does not clear debounce
    store(w.menu.release_count, 0u);      // 446B09
}
void set_pause_exit_marker_445350(LevelOverrideOptions &options) noexcept
{
    store(options.transient, 1u);
}
void exit_pause_menu_446b20(PauseMenuWorld &w)
{
    set_pause_exit_marker_445350(w.options);
    request_scene_transition(w.transition, w.results); // actual441E20 request/save; not441DC0
    store(w.menu.release_count, 0u);
}
void increment_pause_audio_b_446a90(PauseMenuWorld &w) noexcept
{
    auto next = read(w.lifecycle.level_audio.input_b) + 10u;
    store(w.lifecycle.level_audio.input_b, next);
    if (next > 100u)
        store(w.lifecycle.level_audio.input_b, 0u);
    update_level_audio(w.lifecycle.level_audio, w.options);
}
void increment_pause_audio_a_446ac0(PauseMenuWorld &w) noexcept
{
    auto next = read(w.lifecycle.level_audio.input_a) + 10u;
    store(w.lifecycle.level_audio.input_a, next);
    if (next > 100u)
        store(w.lifecycle.level_audio.input_a, 0u);
    update_level_audio(w.lifecycle.level_audio, w.options);
}
void increment_pause_detail_446a70(PauseMenuWorld &w) noexcept
{
    auto next = read(w.detail) + 10u;
    store(w.detail, next);
    if (next > 100u)
    {
        next = 0;
        store(w.detail, next);
    }
    store(w.options.render_a, next);
}
void cycle_pause_camera_446bb0(PauseMenuWorld &w)
{
    auto next = read(w.selected_camera_preset) + 1u;
    store(w.selected_camera_preset, next); // 446BBA
    if (as_signed(next) > 4)
    {
        next = 1;
        store(w.selected_camera_preset, next);
    } // 446BC7
    const auto token = read(w.lifecycle.physical_input.token);
    const auto vehicle = read(w.menu.physical_bindings[physical_slot(token)].vehicle); // 446BD2 before dispatch
    if (next - 1u > 3u)
        return;
    const auto current = read(w.lifecycle.hud.controls.current);
    if (vehicle >= 6 || current >= 4)
        throw FormatError("pause camera target outside physical/controller owner");
    auto settings = w.vehicles.camera_snapshot(vehicle);
    CameraEffectTargets effects;
    for (unsigned i = 0; i < 6; ++i)
    {
        effects.vehicles[i] = &w.vehicles.physics[i];
        effects.active[i] = std::bit_cast<std::uint32_t>(w.vehicles.race[i].active);
    }
    auto &camera = w.lifecycle.hud.controls.controllers[current].camera;
    camera_apply_preset(camera, settings, next, w.selected_camera_preset, camera_vehicle_reset_adapter(effects),
                        w.frame_projection.shared_model_identity);
    w.vehicles.store_camera_settings(vehicle, settings);
    w.frame_projection.matrix = camera.projection;
    w.frame_projection.inverse_depth_scale = camera.inverse_depth_scale;
    w.frame_projection.depth_offset = camera.depth_offset;
}
void update_pause_camera_label_446b60(PauseMenuWorld &w) noexcept
{
    const auto index = read(w.selected_camera_preset) - 1u;
    if (index <= 3u)
        store(w.menu.rows[6].extra_text_id, index + 51u);
}
void dispatch_pause_menu_input_447100(PauseMenuDrawWorld &w)
{
    auto candidate = read(w.menu.selected_row);
    const auto entry_callback = read(row(w, candidate).callback); // 44710B before null test
    auto input = read(w.physical_input.token);
    if (input == 0)
        return;
    const auto &first = edges(w, input);
    bool navigated = false;
    if (read(first.up) != 0 && read(first.up_previous) == 0)
    {
        sound(w, 48);
        candidate = read(w.menu.selected_row);
        bool found = false;
        for (unsigned attempts = 0; attempts < 7; ++attempts)
        {
            --candidate;
            if (as_signed(candidate) < 0)
                candidate = 6;
            if (read(row(w, candidate).enabled) != 0)
            {
                found = true;
                break;
            }
        }
        if (!found)
            throw FormatError("pause up navigation has no enabled row; source loop would not terminate");
        store(w.menu.selected_row, candidate); // up publishes only accepted candidate
        navigated = true;
    }
    else if (read(first.down) != 0 && read(first.down_previous) == 0)
    {
        sound(w, 48);
        candidate = read(w.menu.selected_row);
        bool found = false;
        for (unsigned attempts = 0; attempts < 7; ++attempts)
        {
            candidate = std::bit_cast<std::uint32_t>(as_signed(candidate + 1u) % 7); // CDQ/IDIV
            store(w.menu.selected_row, candidate); // down publishes even rejected candidates
            if (read(row(w, candidate).enabled) != 0)
            {
                found = true;
                break;
            }
        }
        if (!found)
            throw FormatError("pause down navigation has no enabled row; source loop would not terminate");
        navigated = true;
    }
    if (navigated)
        input = read(w.physical_input.token); // 4471C2, no second null return
    const auto release = read(w.menu.release_count);
    const auto &action = edges(w, input);
    if (release > 1u)
    {
        if (read(action.back) != 0 && read(action.back_previous) == 0)
        {
            sound(w, 49);
            call_callback(w, 0x446b40);
            return;
        }
        if (read(action.activate) != 0 && read(action.activate_previous) == 0 && read(row(w, candidate).callback) != 0)
        {
            sound(w, 49);
            call_callback(w, entry_callback);
            return; // 447241 OLD ESI, not live row
        }
    }
    if (read(action.back) != 0 || read(action.back_previous) != 0)
        store(w.menu.release_count, 0u);
    else if (release <= 1u)
        store(w.menu.release_count, release + 1u);
}

void submit_pause_menu_rows_446c20(PauseMenuDrawWorld &w, float x, float y)
{
    update_pause_rows_446c20(w);
    std::uint32_t index = 0;
    while (read(row(w, index).text_id) != 0)
    {
        auto &entry = row(w, index);
        if (index == camera_row && host_bound(w))
        { // NEW HOST (R29): row 6's slot shows ADVANCED (also while the source disables CAMERA)
            draw_advanced_row(w, x, source_box_width(w), y);
            ++index;
            continue;
        }
        if (read(entry.enabled) == 1u)
        {
            std::uint32_t color = 0xffffffffu;
            if (index == read(w.menu.selected_row) && !host_row_selected(w))
            { // NEW HOST (R24)
                const auto pulse = read(w.menu.highlight);
                const auto sign = as_signed(pulse) < 0 ? 0xffffffffu : 0u;
                auto magnitude = ((pulse ^ sign) - sign) << 1;
                if (as_signed(magnitude) > 255)
                    magnitude = 255;
                color = (magnitude | 0xffffff00u) << 16;
            }
            const auto main_x = stored(double(read(entry.x)) + x);
            const auto main_text = text_pointer(w, read(entry.text_id)); // source pointer before Y read
            const auto main_y = stored(double(read(entry.y)) + y);
            text(w, main_text, 0, main_x, main_y, color, 1);
            const auto extra = read(entry.extra_text_id); // after actual main text child
            if (extra != 0)
            {
                const auto extra_x = stored(double(main_x) + as_float(0x430e6667u));
                text(w, text_pointer(w, extra), 0, extra_x, main_y, color, 1);
            }
        }
        ++index; // next text ID read remains live after both children
    }
}
void submit_pause_menu_4473f0(PauseMenuDrawWorld &w)
{
    // NEW HOST (wide_hud R20): the whole pause panel (quads 432E00, frame 447280
    // lines, meters 446D90, rows/footer text) stays one centered 4:3 composition.
    host_wide_overlay::AnchorScope anchor(host_wide_overlay::race::panel);
    if (read(w.pending) == 1u)
        return;
    if (read(w.second_pause) == 1u)
        return;
    if (read(w.frame_pause) != 1u)
    {
        store(w.menu.overlay_ready, 0u);
        store(w.menu.selected_row, 0u);
        reset_host_on_pause_end(w); // NEW HOST (R24): host state only
        return;
    }
    if (read(w.menu.overlay_ready) == 0u)
    {
        w.textures.replace_source_texture(15, 26);
        store(w.menu.overlay_ready, 1u);
    }
    // NEW HOST (R24/R29): the ADVANCED page owns input while open; the ADVANCED
    // row takes part in navigation; every other frame runs the unchanged 447100.
    if (host_panel_open(w))
        dispatch_panel_input(w);
    else if (host_navigation_involved(w))
        dispatch_pause_input_with_host_row(w);
    else
        dispatch_pause_menu_input_447100(w); // continue even after resume/restart/exit
    const bool panel = host_panel_open(w);   // after input: this frame draws what it chose
    const auto language = read(w.menu.requested_language);
    const auto width = as_float(language == 4u || language == 6u ? 0x43a1999au : 0x43800000u);
    const double horizontal = (640.0 - double(width)) * 0.5;
    const auto ready = read(w.menu.overlay_ready); // 447477, separate from later header read
    auto d = w.descriptor;
    store(d[0], 2u);
    store(d[2], 0x42f00000u);
    store(d[3], 0x3f75c28fu);
    const auto x = stored(horizontal);
    store(d[1], as_word(stored(horizontal))); // two stores, no F32 reload
    if (ready == 0u)
    {
        store(d[6], 0x446e0000u);
        store(d[7], 0x44480000u);
        store(d[8], 0x447e0000u);
        store(d[9], 0x44580000u);
        store(d[10], 11u);
        frame(w, x, width);
    }
    else
    {
        store(d[6], 0u);
        store(d[7], 0u);
        store(d[8], 0x44800000u);
        store(d[9], 0x44800000u);
        store(d[10], 15u);
    }
    store(d[5], 0x43700000u);
    store(d[4], as_word(width));
    store(d[11], 32u);
    quad(w); // R29: the original box, also with ADVANCED bound
    if (read(w.menu.overlay_ready) == 0u)
    {
        store(d[0], 2u);
        store(d[2], 0x43080000u);
        store(d[3], 0x3f7d70a4u);
        store(d[6], 0x43a00000u);
        store(d[7], 0x44210000u);
        store(d[8], 0x44110000u);
        store(d[9], 0x44370000u);
        const double header_x = (double(width) - 65.0) * 0.5 + x;
        store(d[4], 0x42820000u);
        store(d[5], 0x41b00000u);
        store(d[10], 14u);
        store(d[11], 32u);
        store(d[1], as_word(stored(header_x)));
        quad(w);
    }
    if (panel)
    { // NEW HOST (R29): the ADVANCED page replaces 446C20's drawing and the meters; footer stays
        update_pause_rows_446c20(w);
        draw_page(w, x, width, 120);
    }
    else
    {
        submit_pause_menu_rows_446c20(w, x, 120);
        const double meter_begin = double(width) - 64.0;
        const auto input_b = read(w.level_audio.input_b); // 447602
        const auto meter_x = stored((((meter_begin - 12.0) - as_float(0x414ccccdu)) + x) + as_float(0x414ccccdu));
        meter(w, input_b, meter_x, as_float(0x437f3333u));
        meter(w, read(w.level_audio.input_a), meter_x, as_float(0x4389999au));
        if (read(w.menu.rows[5].enabled) == 1u)
            meter(w, read(w.detail), meter_x, as_float(0x4393999au));
    }
    const auto input = read(w.physical_input.token);
    if (input != 0)
    {
        const auto kind = read(w.menu.physical_bindings[physical_slot(input)].kind);
        // 98+kind is a32-bit source address expression. The bounded105-entry
        // table rejects a wrapped/out-of-table native identity instead of fixing it.
        if (kind > 6u)
            throw FormatError("pause footer physical kind outside game text table");
        text(w, text_pointer(w, 98u + kind), 1, 320, 80, 0x00ffff00u, 2);
    }
}
namespace
{
class NativePauseActions final : public PauseMenuActions
{
    PauseMenuWorld &w;

  public:
    explicit NativePauseActions(PauseMenuWorld &world) : w(world) {}
    void sound_461150(std::uint32_t id) override
    {
        audio_start(w.audio, id, 0, 0, 255, 0, 1, nullptr);
    }
    void invoke(std::uint32_t id) override
    {
        switch (id)
        {
        case 0x446b40:
            resume_pause_menu_446b40(w);
            return;
        case 0x446af0:
            restart_pause_menu_446af0(w);
            return;
        case 0x446b20:
            exit_pause_menu_446b20(w);
            return;
        case 0x446a90:
            increment_pause_audio_b_446a90(w);
            return;
        case 0x446ac0:
            increment_pause_audio_a_446ac0(w);
            return;
        case 0x446a70:
            increment_pause_detail_446a70(w);
            return;
        case 0x446bb0:
            cycle_pause_camera_446bb0(w);
            return;
        default:
            throw FormatError("pause menu latched callback outside recovered function domain");
        }
    }
};
PauseMenuDrawWorld draw_world(PauseMenuWorld &w, PauseMenuActions &actions)
{
    return {w.menu,
            w.transition.pending,
            w.lifecycle.second_pause,
            w.lifecycle.frame_pause,
            w.lifecycle.physical_input,
            w.clock,
            w.audio.split_setting,
            w.render_b,
            w.game_type,
            w.detail,
            w.selected_camera_preset,
            w.lifecycle.level_audio,
            w.game_text,
            w.textures,
            w.descriptor,
            w.line_clip,
            w.meter_labels,
            w.drawing,
            w.sink,
            actions};
}
} // namespace
void dispatch_pause_menu_input_447100(PauseMenuWorld &w)
{
    NativePauseActions actions(w);
    auto draw = draw_world(w, actions);
    dispatch_pause_menu_input_447100(draw);
}
void submit_pause_menu_rows_446c20(PauseMenuWorld &w, float x, float y)
{
    NativePauseActions actions(w);
    auto draw = draw_world(w, actions);
    submit_pause_menu_rows_446c20(draw, x, y);
}
void submit_pause_menu_4473f0(PauseMenuWorld &w)
{
    NativePauseActions actions(w);
    auto draw = draw_world(w, actions);
    submit_pause_menu_4473f0(draw);
}

} // namespace sgn::retail
