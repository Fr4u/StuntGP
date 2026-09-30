#include "sgn/host_graphics_page.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_title_page.hpp"
#include "sgn/source_title_draw.hpp"
#include "sgn/source_menu_text.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_frontend_font_metrics.hpp"
#include "sgn/frontend_page_words.hpp"
#include "sgn/host_menu_arrow.hpp"
#include "sgn/retail_text_submission.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <bit>
#include <span>
#include <stdexcept>
#include <string>

// NEW HOST (R23) GRAPHICS options page; see host_graphics_page.hpp.
// R24: rows, value stepping, strings and the session save come from the
// shared host_graphics_menu_model (also used by the in-race pause panel).
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
namespace menu = sgn::host_graphics_menu;
using menu::row_count;

// Word layout of FrontendSourceStorage::host_menu_r23 (host addresses).
constexpr W host_base = FrontendSourceStorage::host_menu_base_r23;
constexpr W name_words = 0, options_descriptor_words = 4, options_row_words = 16, options_row_capacity = 8;
constexpr W graphics_descriptor_words = 80, graphics_row_words = 96,
            graphics_row_capacity = 11; // R24: ten rows + terminator
// R27: the model has more rows than fit between the title box (y 50) and the
// help box (y 420) at the MenuText pitch 32; the page shows a window of rows
// and scrolls it (page word +24 = first shown model row). R29: 9 rows (112..368)
// so the scroll arrows fit one arrow offset above/below the window.
constexpr W visible_rows = 9;
static_assert(visible_rows + 1 <= graphics_row_capacity);
constexpr W first_shown_offset = 24, page_bytes = 28;
static_assert(graphics_row_words + graphics_row_capacity * 8 <=
              std::tuple_size_v<decltype(FrontendSourceStorage::host_menu_r23)>);
static_assert(options_row_words + options_row_capacity * 8 <= graphics_descriptor_words);
constexpr W host_address(W words)
{
    return host_base + words * 4u;
}
constexpr W descriptor_words = 11; //+00..+28, as 43C860/43C980 read them

// Host text identities (never a 441170 language ID: those are < 741).
constexpr W text_family = 0x7e231000u;
constexpr W row_label = 0x7e231000u, row_help = 0x7e231100u; //+menu position 0..19 (menu::row_at)
constexpr W options_label = 0x7e231200u, options_help = 0x7e231201u;
bool host_text(W id)
{
    return (id & 0xfffff000u) == text_family;
}

using sgn::retail::frontend_page_words::page_object_bytes;
using sgn::retail::frontend_page_words::page_pool;
using sgn::retail::frontend_page_words::page_word;
using sgn::retail::frontend_page_words::set_page_word;
using sgn::retail::frontend_page_words::source_word;
void write_row(FrontendState &f, W row, W id, W text, W help)
{
    // Same row shape as every 4881F8 Options row: colours, text, help, box1, no sprites.
    const std::array<W, 8> words{id, 0xffc8c8ffu, 0xffffffffu, text, help, 1u, 0u, 0u};
    for (W i = 0; i < 8; ++i)
        source_word(f, row + i * 4u) = words[i];
}
void clear_row(FrontendState &f, W row)
{
    for (W i = 0; i < 8; ++i)
        source_word(f, row + i * 4u) = 0;
}

// Current language: the catalog column whose flag equals 5E37B8 (the flag
// 440C40 published), identified by its LANGUAGE NAME. A catalog language the
// shared table does not know (not in the retail file) shows the English text.
const menu::Language &current_language(FrontendState &f)
{
    auto &s = f.source;
    const auto flag = s.source_word(0x5e37b8);
    const auto count = std::bit_cast<std::int32_t>(s.source_word(0x5e38dc));
    for (std::int32_t column = 0; column < count && column < 8; ++column)
    {
        const auto at = static_cast<W>(column) * 36u;
        if (s.source_word(0x5e37dcu + at) != flag)
            continue;
        const auto name = s.source_bytes(0x5e37bcu + at, 32);
        const auto end = std::find(name.begin(), name.end(), std::uint8_t{});
        return menu::language_for_catalog_name(std::u8string_view(reinterpret_cast<const char8_t *>(name.data()),
                                                                  static_cast<std::size_t>(end - name.begin())));
    }
    return menu::language_for_catalog_name({});
}
// Composed at every draw from host_graphics::current() (never cached).
std::span<std::uint8_t> compose(FrontendState &f, W id, std::array<std::uint8_t, 192> &buffer)
{
    const auto &l = current_language(f);
    menu::Latin1Text text(buffer);
    if (id == options_label)
        text.append(menu::options_row_label(l));
    else if (id == options_help)
        text.append(menu::options_row_help(l));
    else if (id - row_label < row_count)
        text.append_row(l, menu::row_at(id - row_label), host_graphics::current());
    else if (id - row_help < row_count)
        text.append(menu::help(l, menu::row_at(id - row_help)));
    else
        throw FormatError("unknown host GRAPHICS text identity");
    return text.finish();
}

// ---- Descriptors (host copies in the style of 488358) ----
void copy_descriptor(FrontendState &f, W destination, W source, W rows)
{
    for (W i = 0; i < descriptor_words; ++i)
        source_word(f, destination + i * 4u) = source_word(f, source + i * 4u);
    source_word(f, destination + 0x28u) = rows;
}
// One box width for every row, like the neighbouring Options sub-pages (fixed
// 0xC8 = 200 in 488358/4884E0/488588): 200, or the widest "LABEL: VALUE" over
// every value each row can show plus 16, measured with the live frontend font
// at the descriptor's font scale (+14 = 1.0, the 439040 t[0] measure).
W uniform_row_width(FrontendState &f)
{
    const auto &l = current_language(f);
    const auto metrics = f.world().ui.fonts.metrics();
    const auto scale = std::bit_cast<float>(source_word(f, 0x488358u + 0x14u));
    double widest = 0;
    for (W row = 0; row < row_count; ++row)
    {
        for (const auto &settings : menu::value_variants())
        {
            std::array<std::uint8_t, 192> buffer;
            menu::Latin1Text text(buffer);
            text.append_row(l, menu::row_at(row), settings);
            widest = std::max(widest, measure_frontend_text_438910(metrics, text.finish(), scale));
        }
    }
    constexpr double original_width = 200, padding = 16, maximum = 560;
    return static_cast<W>(std::min(maximum, std::max(original_width, std::ceil(widest + padding))));
}
// Writes the visible window of model rows starting at `first` into the host
// row table (row ID = model row, so 43C980 event 26 keeps returning it).
void write_visible_rows(FrontendState &f, W first)
{
    const auto rows = host_address(graphics_row_words);
    if (first + visible_rows > row_count)
        throw FormatError("host GRAPHICS row window outside the model rows");
    for (W slot = 0; slot < visible_rows; ++slot)
    {
        const auto row = first + slot;
        write_row(f, rows + slot * 32u, row, row_label + row, row_help + row);
    }
    for (W slot = visible_rows; slot < graphics_row_capacity; ++slot)
        clear_row(f, rows + slot * 32u);
}
W build_graphics_descriptor(FrontendState &f)
{
    const auto descriptor = host_address(graphics_descriptor_words), rows = host_address(graphics_row_words);
    // Same layer, position, font scale, box scale, style and centring as 488358.
    copy_descriptor(f, descriptor, 0x488358, rows);
    source_word(f, descriptor + 0x24u) = uniform_row_width(f); // positive: fixed width, as 488358's 0xC8
    write_visible_rows(f, 0);
    return descriptor;
}
void write_page_name(FrontendState &f)
{
    constexpr std::array<std::uint8_t, 16> name{'P', 'a', 'g', 'e', 'G', 'r', 'a', 'p', 'h', 'i', 'c', 's', 0, 0, 0, 0};
    for (W i = 0; i < 4; ++i)
        source_word(f, host_address(name_words) + i * 4u) =
            W{name[i * 4]} | (W{name[i * 4 + 1]} << 8) | (W{name[i * 4 + 2]} << 16) | (W{name[i * 4 + 3]} << 24);
}
W create_host_graphics_page(FrontendState &f, W key, SourceNavigationPageServices &pages)
{
    f.shared_platform()->require_owner_thread();
    // Same object shapes as 43E000 case15 / 454CE0: a page (24 bytes plus the R27
    // host word +24, first shown row) and a 200-byte MenuText child.
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), page_bytes);
    const auto bytes = page_object_bytes(f, page, page_bytes);
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    write_page_name(f);
    set_page_word(f, page, 8, 0);
    set_page_word(f, page, 12, host_graphics_page_close_r23);
    // Page class word 13, as PageOptions: 43F610's idle return and 43F6B0 treat it like Options.
    set_page_word(f, page, 4, 13);
    set_page_word(f, page, 0, host_address(name_words));
    set_page_word(f, page, 16, host_graphics_page_event_r23);
    const auto child = allocate_frontend_page_object_43d470(page_pool(f), 0xc8);
    const auto child_bytes = page_object_bytes(f, child, 0xc8);
    std::fill(child_bytes.begin(), child_bytes.end(), std::byte{0});
    set_page_word(f, child, 12, source_word(f, 0x4794e8));
    set_page_word(f, child, 16, source_word(f, 0x4794ec));
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x488390);
    set_page_word(f, child, 8, 0); // 0x488390: the PE "MenuText" name
    if (initialize_frontend_menu_text_43c860(f, child, build_graphics_descriptor(f), pages) == 0)
        throw FormatError("43C860 rejected the host GRAPHICS descriptor");
    set_page_word(f, page, 20, child);
    pages.page_event(f, child, 24, 0); // as 454D67/454D72
    pages.page_event(f, child, 22, 0);
    set_page_word(f, page, 8, key); // 43ED94 publication after the initializer
    return page;
}
// 43DFA0 range-checks the factory kind before calling the factory. This
// adapter is the only factory of that push: it creates the host page and
// confirms it received the kind the route passed.
constexpr W push_kind = 15; // the Options factory kind, parent of this page
class HostGraphicsFactory final : public SourcePageFactoryServices
{
    SourceNavigationPageServices &pages_;

  public:
    explicit HostGraphicsFactory(SourceNavigationPageServices &pages) : pages_(pages) {}
    W create_page_43e000(FrontendState &f, W key, W kind) override
    {
        if (kind != push_kind)
            throw std::logic_error("host GRAPHICS factory received another page kind");
        return create_host_graphics_page(f, key, pages_);
    }
};
// ---- R29 scroll arrows: requested while the page draws (event 3, the queue
// is built), drawn after 437ED0 consumed the lists of the same frame.
struct ArrowRequest
{
    bool active{}, up{}, down{};
    float x{}, up_y{}, down_y{};
    std::array<std::uint32_t, 4> clip{}; // 5DA360..36C (integer x0, y0, x1, y1) at request time
};
ArrowRequest &arrow_request()
{
    static ArrowRequest request; // owner thread only (require_owner_thread at both ends)
    return request;
}
constexpr float arrow_offset = 28;       // arrow centre above the first / below the last row centre
constexpr float arrow_width = 16;        // host_menu_arrow::default_width, same as the pause page
constexpr std::uint32_t arrow_list = 11; // the rows' list (descriptor +00 = 0x0B) for the 437F40 depth
void request_scroll_arrows(FrontendState &f, W page)
{
    auto &request = arrow_request();
    const auto child = page_word(f, page, 20);
    const auto descriptor = page_word(f, child, 0x18);
    const auto first = page_word(f, page, first_shown_offset);
    // 43C980 event 3 row centres: y = child+BC - (count-1) * pitch / 2, pitch = 2 * 16 * scale.
    const auto count = page_word(f, child, 0x14);
    const double pitch = 2.0 * 16.0 * double(std::bit_cast<float>(source_word(f, descriptor + 0x14u)));
    const double center_y = double(std::bit_cast<float>(page_word(f, child, 0xbc)));
    const double first_y = center_y - double(count - 1u) * pitch * 0.5;
    const double last_y = first_y + double(count - 1u) * pitch;
    request.active = true;
    request.x = std::bit_cast<float>(page_word(f, child, 0xb8));
    request.up_y = static_cast<float>(first_y - arrow_offset);
    request.down_y = static_cast<float>(last_y + arrow_offset);
    request.up = first > 0;
    request.down = first + visible_rows < row_count;
    for (W i = 0; i < 4; ++i)
        request.clip[i] = f.source.frontend_clip[i];
}
} // namespace

void draw_host_graphics_page_arrows_r29(FrontendState &f, PolygonSubmissionSink &sink, const TextSubmission &text)
{
    f.shared_platform()->require_owner_thread();
    auto &request = arrow_request();
    if (!request.active)
        return;
    const auto current = request;
    request = {};
    const auto inside = [&](float x, float y) {
        return x >= float(std::bit_cast<std::int32_t>(current.clip[0])) &&
               y >= float(std::bit_cast<std::int32_t>(current.clip[1])) &&
               x <= float(std::bit_cast<std::int32_t>(current.clip[2])) &&
               y <= float(std::bit_cast<std::int32_t>(current.clip[3]));
    };
    // NEW HOST (R30): the two meshes are kept while their build arguments stay the same (the page is
    // static between scrolls), instead of being rebuilt and reallocated every frame. build() is a pure
    // function of its arguments, so the drawn vertices are identical. Owner thread only, like the request.
    struct ArrowMeshes
    {
        std::array<host_menu_arrow::Mesh, 2> mesh; // up, down (draw order)
        std::array<std::array<float, 3>, 2> key{}; // x, y, feather of each built mesh
        std::array<bool, 2> built{};
    };
    static ArrowMeshes cache;
    const auto feather = host_menu_arrow::one_pixel_feather();
    const auto prepare = [&](unsigned index, host_menu_arrow::Direction direction, float y) {
        const std::array<float, 3> key{current.x, y, feather};
        if (!cache.built[index] || cache.key[index] != key)
        {
            cache.built[index] = false;
            cache.mesh[index] = host_menu_arrow::build(direction, current.x, y, arrow_width, feather);
            cache.key[index] = key;
            cache.built[index] = true;
        }
    };
    const bool up = current.up && inside(current.x, current.up_y);
    const bool down = current.down && inside(current.x, current.down_y);
    if (!up && !down)
        return;
    if (up)
        prepare(0, host_menu_arrow::Direction::up, current.up_y);
    if (down)
        prepare(1, host_menu_arrow::Direction::down, current.down_y);
    const std::span<const host_menu_arrow::Mesh> arrows(cache.mesh.data() + (up ? 0 : 1),
                                                        std::size_t(up ? 1 : 0) + std::size_t(down ? 1 : 0));
    // After the lists the device holds the last command's state; the arrows put
    // back the texture of the material 61C5E4 names and the 432380 ALPHAREF.
    const bool specular = text.quality == 1u && text.legacy_driver_restriction != 1u;
    const auto material = text.selected_material;
    const host_menu_arrow::Restore restore{
        material < text.materials.size() ? text.materials[material].primary_texture : 0u, specular ? 1u : 160u};
    const auto depth = static_cast<float>(1.0 - double(arrow_list) * double(std::bit_cast<float>(0x3c23d70au)));
    host_menu_arrow::draw(sink, text.viewport, arrows, depth, restore);
}

W build_host_options_descriptor_r23(FrontendState &f, W source_descriptor)
{
    const auto descriptor = host_address(options_descriptor_words), rows = host_address(options_row_words);
    copy_descriptor(f, descriptor, source_descriptor, rows);
    // Rows of the table 454CE0 selected (4881F8 or 4882B8), counted like 43C860:
    // until a row whose text word (+0C) is zero.
    auto source_row = source_word(f, source_descriptor + 0x28u);
    W count = 0;
    while (source_word(f, source_row + 12u) != 0)
    {
        if (count + 2u > options_row_capacity)
            throw FormatError("Options row table exceeds the host GRAPHICS row copy");
        for (W i = 0; i < 8; ++i)
            source_word(f, rows + count * 32u + i * 4u) = source_word(f, source_row + i * 4u);
        ++count;
        source_row += 32u;
    }
    write_row(f, rows + count * 32u, host_graphics_options_row_id_r23, options_label, options_help);
    for (W row = count + 1u; row < options_row_capacity; ++row)
        clear_row(f, rows + row * 32u);
    return descriptor;
}
bool route_host_graphics_option_r23(FrontendState &f, W selection, SourceNavigationPageServices &pages,
                                    CampaignScoreMusic &music)
{
    if (selection != host_graphics_options_row_id_r23)
        return false;
    HostGraphicsFactory factory(pages);
    (void)push_frontend_page_43dfa0(f, 0, push_kind, factory, pages, music); // music key0, as454E44..454E9A
    return true;
}
void destroy_host_graphics_page_r23(FrontendState &f, W page)
{
    f.shared_platform()->require_owner_thread();
    const auto child = page_word(f, page, 20);
    if (page_word(f, child, 12) != 0x43c970u)
        throw FormatError("host GRAPHICS page reached an unreconstructed MenuText destructor identity");
    release_frontend_page_object_43d4c0(page_pool(f), child); // as 454D8E; 43C970 is RET
    set_page_word(f, page, 20, 0);
    menu::save_settings(); // leaving the page persists the user's choices
}
W dispatch_host_graphics_page_r23(FrontendState &f, W page, W event, SourceNavigationPageServices &pages,
                                  CampaignScoreMusic &music, SourceMainPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 19: // as 454DA0 event19: same tiled layout, background and scroll words
        f.source.source_word(0x5ff46c) = 1;
        set_frontend_packed_word_43dd10(f.source, 0x114311);
        f.source.source_word(0x5ff448) = 99;
        f.source.source_word(0x5ff44c) = 0x3e99999a;
        f.source.source_word(0x5ff450) = 0xbf000000;
        (void)calls.menu_text_event(f, page_word(f, page, 20), event, 0);
        return 0;
    case 16: { // Enter: MenuText flash and sound, then next value (or reset)
        const auto child = page_word(f, page, 20);
        (void)calls.menu_text_event(f, child, event, 0);
        const auto row = calls.menu_text_event(f, child, 26, 0);
        if (row < row_count)
            menu::change(menu::row_at(row), 1);
        return 0;
    }
    case 13:
    case 14: { // left/right: previous/next value, with the selection pages' value sounds
        const auto row = calls.menu_text_event(f, page_word(f, page, 20), 26, 0);
        if (row < row_count && menu::has_value(menu::row_at(row)))
        {
            menu::change(menu::row_at(row), event == 14 ? 1 : -1);
            calls.feedback_440890(f, event);
            pages.feedback_440880(f, 20);
        }
        return 0;
    }
    case 17: // as 455430/4556B0 event17
        if (pop_frontend_page_43eef0(f, pages, music) != 0)
            calls.feedback_440890(f, event);
        return 0;
    case 3: { // title box with 4392B0's exact 439040 arguments, then the rows
        request_scroll_arrows(f, page);
        std::array<std::uint8_t, 192> buffer;
        menu::Latin1Text title(buffer);
        title.append(menu::title(current_language(f)));
        draw_frontend_box_borrowed_439040(
            f,
            {2, 4, 0x43a00000u, 0x42480000u, {}, {0x3fa00000u, 0xffffffffu, 3, 0, 0x3f800000u, 0x41, 0xffffff10u, 0}},
            title.finish());
        [[fallthrough]];
    }
    case 2:
    case 20:
        (void)calls.menu_text_event(f, page_word(f, page, 20), event, 0);
        break;
    case 11:
    case 12: { // up/down: 43C980 inside the window; at its edge scroll the window
        const auto child = page_word(f, page, 20);
        const auto selected = calls.menu_text_event(f, child, 27, 0);
        const auto first = page_word(f, page, first_shown_offset);
        if (event == 11 && selected == 0 && first > 0)
        {
            set_page_word(f, page, first_shown_offset, first - 1u);
            write_visible_rows(f, first - 1u);
            calls.feedback_440890(f, event); // the 43C980 move sound
            return 0;
        }
        if (event == 12 && selected + 1u == visible_rows && first + visible_rows < row_count)
        {
            set_page_word(f, page, first_shown_offset, first + 1u);
            write_visible_rows(f, first + 1u);
            calls.feedback_440890(f, event);
            return 0;
        }
        (void)calls.menu_text_event(f, child, event, 0);
        break;
    }
    default:
        break;
    }
    return 0;
}
bool draw_host_menu_text_438ff0_r23(FrontendState &f, W ecx, W edx, const std::array<W, 11> &stack)
{
    if (!host_text(stack[2]))
        return false;
    std::array<std::uint8_t, 192> buffer;
    SourceFrontendTextCall<8> call{ecx, edx, stack[0], stack[1], {}, {}};
    for (unsigned i = 0; i < 8; ++i)
        call.tail[i] = stack[i + 3];
    draw_frontend_box_borrowed_439040(f, call, compose(f, stack[2], buffer));
    return true;
}
} // namespace sgn::retail
