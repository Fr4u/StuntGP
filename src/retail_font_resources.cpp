#include "sgn/retail_font_resources.hpp"
#include "sgn/format_require.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <utility>

namespace sgn::retail
{
namespace
{
using sgn::format_check::require;
void observe_bank(FontBankObserver *observer, const SourceAllocationBank &bank, FontBankEventKind kind,
                  std::uint32_t site, const SourceAllocationRef &allocation, std::uint32_t bytes)
{
    if (observer)
        observer->observe({kind, site, allocation, bytes, 0}, bank);
}
SourceAllocationRef allocate_font(SourceFontContext c, std::uint32_t site, std::uint32_t size)
{
    const auto allocation = c.bank.allocate_42e8d0(size, 0);
    observe_bank(c.observer, c.bank, FontBankEventKind::allocation, site, allocation, size);
    return allocation;
}
void release_font(SourceFontContext c, std::uint32_t site, const SourceAllocationRef &allocation)
{
    const auto size = static_cast<std::uint32_t>(c.bank.bytes(allocation).size());
    (void)c.bank.release_42e980(allocation);
    observe_bank(c.observer, c.bank, FontBankEventKind::release, site, allocation, size);
}
void require_allocation(const SourceAllocationRef &allocation, std::uint32_t site)
{
    if (!allocation)
        throw SourceAllocationError(site, "font source null payload is outside the admitted read/write domain");
}
std::uint32_t bits(double value)
{
    return std::bit_cast<std::uint32_t>(static_cast<float>(value));
}
std::uint32_t half(std::span<const std::uint8_t> b, std::size_t at)
{
    return b[at] | (std::uint32_t(b[at + 1]) << 8);
}
Bytes read_font(MaterialFileSource &files, const MaterialResourcePath &path)
{
    const auto size = files.source_file_size(path);
    require(size >= 18 && size <= 16 * 1024 * 1024, "font source file size outside qualified domain");
    Bytes bytes(size);
    const auto read = files.read_source_file(path, bytes);
    require(read.source_size != 0 && read.copied_bytes == size, "incomplete font source read");
    return bytes;
}
Bytes decode_font(std::span<const std::uint8_t> raw, FontImageGlobals image)
{
    require(raw.size() >= 18 && raw[2] == 2 && raw[16] == 24, "font requires source type2 RGB24 TGA");
    const auto width = half(raw, 12), height = half(raw, 14), start = 18u + raw[0];
    require(width > 0 && width <= 256 && height >= 160 && height <= 65535 && start <= raw.size() &&
                std::uint64_t(width) * height * 3 <= raw.size() - start,
            "font TGA payload outside source bounds");
    image.width = image.tga_width = width;
    image.height = image.tga_height = height;
    Bytes rgb(std::size_t(width) * height * 3);
    // 462310 tests BL (the accepted image TYPE2), so descriptor orientation
    // does not affect this source path. Original rows are always reversed.
    for (std::uint32_t y = 0; y < height; ++y)
        for (std::uint32_t x = 0; x < width; ++x)
        {
            const auto from = start + (std::size_t(height - 1 - y) * width + x) * 3,
                       to = (std::size_t(y) * width + x) * 3;
            rgb[to] = raw[from + 2];
            rgb[to + 1] = raw[from + 1];
            rgb[to + 2] = raw[from];
        }
    return rgb;
}
} // namespace
std::uint32_t &RetailFontResources::source_word(std::size_t font, std::size_t offset)
{
    require(font < 2 && offset < retail_font_record_bytes && offset % 4 == 0, "font word outside source record");
    const auto word = offset / 4;
    if (word == 2)
        return metric_words_[161 * font];
    if (word >= 10 && (word - 10) % 11 == 0)
        return metric_words_[161 * font + 1 + (word - 10) / 11];
    const auto removed_width = word > 2 ? 1u : 0u;
    const auto removed_advances = word / 11;
    return other_words_[font][word - removed_width - removed_advances];
}
const std::uint32_t &RetailFontResources::source_word(std::size_t font, std::size_t offset) const
{
    return const_cast<RetailFontResources *>(this)->source_word(font, offset);
}
FrontendFontMetricsViews RetailFontResources::metrics() &
{
    return {metric_words_, index, glyph_map, current, count, digit_advance_bits};
}
std::uint32_t RetailFontResources::allocate_scratch(std::size_t font)
{
    require(source_bank_ == nullptr, "preview scratch cannot enter a source-bank font owner");
    require(font < 2 && scratch_.size() < std::numeric_limits<std::uint32_t>::max(),
            "font scratch allocation identity exhausted");
    auto allocation = std::make_unique<std::array<std::uint8_t, 1024>>();
    scratch_.push_back(std::move(allocation));
    const auto id = static_cast<std::uint32_t>(scratch_.size());
    source_word(font, 4) = id;
    return id;
}
void RetailFontResources::bind_source_bank(const SourceAllocationBank &bank)
{
    require(bank.initialized(), "font bank requires explicit prior source42EAB0");
    require(scratch_.empty(), "font owner already contains unmigrated preview scratch");
    require(source_bank_ == nullptr || source_bank_ == &bank, "font owner belongs to another source bank");
    source_bank_ = &bank;
}
void RetailFontResources::require_preview_mode() const
{
    require(source_bank_ == nullptr, "preview font entry cannot mutate a source-bank font owner");
}
std::uint32_t RetailFontResources::allocate_source_scratch(std::size_t font, SourceAllocationBank &bank,
                                                           FontBankObserver *observer)
{
    bind_source_bank(bank);
    require(font < 2 && source_scratch_.size() < std::numeric_limits<std::uint32_t>::max(),
            "source font scratch identity exhausted");
    source_scratch_.reserve(source_scratch_.size() + 1); // host identity storage, never a source payload
    const auto ref = bank.allocate_42e8d0(1024, 0);
    std::uint32_t id = 0;
    if (ref)
    {
        source_scratch_.push_back(ref);
        id = static_cast<std::uint32_t>(source_scratch_.size());
    }
    source_word(font, 4) = id; // 428C63 also publishes null;428C00 never reads this scratch
    observe_bank(observer, bank, FontBankEventKind::allocation, 0x428c58, ref, 1024);
    return id;
}
SourceAllocationRef RetailFontResources::source_scratch_allocation(std::size_t font,
                                                                   const SourceAllocationBank &bank) const
{
    require(source_bank_ == &bank, "font scratch belongs to another bank");
    (void)bank.initialized(); // owner-thread check; no payload lifetime extension
    const auto id = source_word(font, 4);
    require(id <= source_scratch_.size(), "font scratch ID is outside its source history");
    return id == 0 ? SourceAllocationRef{} : source_scratch_[id - 1];
}
namespace
{
void prepare_font_atlas_into(RetailFontResources &fonts, std::size_t font, std::span<const std::uint8_t> rgb,
                             std::span<std::uint8_t> atlas, std::uint32_t width, std::uint32_t height,
                             std::int32_t window_width, SourceAllocationBank *bank, FontBankObserver *observer)
{
    require(font < 2 && width > 0 && width <= 256 && height >= 160 && height <= 65535 &&
                std::uint64_t(width) * height * 3 <= rgb.size(),
            "font RGB dimensions outside qualified owner");
    const auto space = fonts.glyph_map[32];
    require(space < 160, "font retained space map outside source glyph owner");
    const auto cell = height / 160;
    require(cell <= 256, "font cell exceeds source atlas");
    require(atlas.size() == 256 * 256 * 3, "font atlas must use the exact source0x30000 allocation");
    for (std::size_t i = 0; i < atlas.size(); i += 3)
    {
        atlas[i] = 255;
        atlas[i + 1] = 0;
        atlas[i + 2] = 255;
    }
    const auto width_factor = static_cast<double>(std::bit_cast<float>(0x3acccccdu));
    fonts.source_word(font, 0x1c) = bits(static_cast<double>(window_width) * 3.0 * width_factor);
    fonts.source_word(font, 0x14) = bits(static_cast<double>(window_width) * 8.0 * width_factor);
    fonts.source_word(font, 0x28 + 44 * space) = 8;
    fonts.source_word(font, 0) = 0;
    if (bank)
        fonts.allocate_source_scratch(font, *bank, observer);
    else
        fonts.allocate_scratch(font);
    fonts.source_word(font, 8) = width;
    fonts.source_word(font, 12) = cell;
    std::uint32_t pack_x = 0, pack_y = 0;
    for (std::uint32_t glyph = 0; glyph < 160; ++glyph)
    {
        std::uint32_t left = width, right = 0, glyph_width = 0;
        for (std::uint32_t y = 0; y < cell; ++y)
        {
            for (std::uint32_t x = 0; x < width; ++x)
            {
                const auto at = (std::size_t(glyph) * cell * width + std::size_t(y) * width + x) * 3;
                const auto r = rgb[at], g = rgb[at + 1], b = rgb[at + 2];
                if ((r == 255 && g == 0 && b == 255) || (r < 96 && g < 96 && b < 96))
                    continue;
                left = std::min(left, x);
                right = std::max(right, x);
            }
            glyph_width = std::max(glyph_width, (right >= left ? right - left : 0u) + 1u);
        }
        if (pack_x + glyph_width > 256)
        {
            pack_x = 0;
            pack_y += cell;
            if (pack_y >= 256)
                break;
        }
        require(pack_y + cell <= 256, "font glyph exceeds source atlas tail");
        for (std::uint32_t y = 0; y < cell; ++y)
        {
            const auto from = (std::size_t(glyph) * cell * width + std::size_t(y) * width + left) * 3;
            const auto to = (std::size_t(pack_y + y) * 256 + pack_x) * 3;
            require(from + std::size_t(glyph_width) * 3 <= rgb.size(), "font glyph copy exceeds decoded RGB owner");
            std::copy_n(rgb.begin() + static_cast<std::ptrdiff_t>(from), std::size_t(glyph_width) * 3,
                        atlas.begin() + static_cast<std::ptrdiff_t>(to));
        }
        const auto record = 0x20 + 44 * glyph;
        fonts.source_word(font, record + 28) = bits(static_cast<double>(pack_x) / 256.0); // 428DAF
        fonts.source_word(font, record + 32) = bits(
            (static_cast<double>(pack_y) + static_cast<double>(std::bit_cast<float>(0x3d4ccccdu))) / 256.0); // 428DC1
        fonts.source_word(font, record + 36) = bits(static_cast<double>(pack_x + glyph_width) / 256.0);      // 428DCF
        fonts.source_word(font, record + 40) = bits((static_cast<double>(pack_y) + cell + 0.25) / 256.0);    // 428DE6
        fonts.source_word(font, record + 8) = glyph_width;                                                   // 428DE9
        fonts.source_word(font, record + 12) = cell;                                                         // 428DEC
        fonts.source_word(font, record + 24) = glyph_width + 1;                                              // 428DF2
        fonts.source_word(font, record + 16) = 0;                                                            // 428DF8
        fonts.source_word(font, record + 20) = 0;                                                            // 428DFB
        pack_x += glyph_width + 1;
    }
    // The source aliases the whole11-word previous space glyph, including UVs
    // and any retained fields, into glyph153. Do not copy its advance alone.
    std::array<std::uint32_t, 11> space_words{};
    for (std::size_t i = 0; i < 11; ++i)
        space_words[i] = fonts.source_word(font, 0x20 + 44 * space + i * 4);
    for (std::size_t i = 0; i < 11; ++i)
        fonts.source_word(font, 0x1a6c + i * 4) = space_words[i];
}
} // namespace
Bytes prepare_retail_font_atlas(RetailFontResources &fonts, std::size_t font, std::span<const std::uint8_t> rgb,
                                std::uint32_t width, std::uint32_t height, std::int32_t window_width)
{
    fonts.require_preview_mode();
    Bytes atlas(256 * 256 * 3);
    prepare_font_atlas_into(fonts, font, rgb, atlas, width, height, window_width, nullptr, nullptr);
    return atlas;
}
void prepare_source_font_atlas_428c00(RetailFontResources &fonts, std::size_t font, SourceFontContext c,
                                      const SourceAllocationRef &rgb, const SourceAllocationRef &atlas,
                                      std::uint32_t width, std::uint32_t height, std::int32_t window_width)
{
    fonts.bind_source_bank(c.bank);
    require(!(rgb == atlas), "font RGB and atlas must be distinct source allocations");
    prepare_font_atlas_into(fonts, font, c.bank.bytes(rgb), c.bank.bytes(atlas), width, height, window_width, &c.bank,
                            c.observer);
}
void append_font_reload_42be90(FontReloadViews reload, std::string_view path)
{
    require(path.find('\0') == std::string_view::npos, "font source path contains embedded NUL");
    require(reload.font_reload_cursor < 7, "font reload table needs terminal name slot");
    require(reload.shared_name_cursor < reload.shared_name_arena.size() &&
                path.size() < reload.shared_name_arena.size() - reload.shared_name_cursor,
            "font shared reload name arena exhausted");
    const auto at = reload.font_reload_cursor * 2;
    // Offset-plus-one keeps the source null pointer distinct from arena[0].
    reload.font_reload_records[at] = reload.shared_name_cursor + 1;
    reload.font_reload_records[at + 1] = reload.ordinary_material_count;
    reload.font_reload_records[at + 2] = 0; // 42BEAE
    ++reload.font_reload_cursor;            // 42BEB4
    std::copy(path.begin(), path.end(), reload.shared_name_arena.begin() + reload.shared_name_cursor);
    reload.shared_name_arena[reload.shared_name_cursor + path.size()] = 0;
    reload.shared_name_cursor += static_cast<std::uint32_t>(path.size() + 1);
}
void upload_retail_font_atlases(RetailFontResources &fonts, std::size_t font, std::span<const std::uint8_t> rgb,
                                std::uint32_t enabled, FontAtlasUploadTarget &target)
{
    fonts.require_preview_mode();
    require(font < 2, "font upload record outside source bank");
    if (enabled != 1)
        return; // 4327EE; retain old atlas indices and transparency.
    require(rgb.size() == 256 * 256 * 3, "font atlas must have original256x256 RGB pixels");
    auto &transparency = target.current_transparency();
    std::vector<std::uint16_t> words(256 * 256);
    for (std::size_t i = 0; i < words.size(); ++i)
    {
        auto r = rgb[3 * i], g = rgb[3 * i + 1], b = rgb[3 * i + 2];
        std::uint16_t alpha = 0x8000;
        if (r < 8 && g < 8 && b < 8)
            r = g = b = 8;
        else if (r == 255 && g == 0 && b == 255)
        {
            r = g = b = 0;
            alpha = 0;
            transparency = 1;
        }
        words[i] = static_cast<std::uint16_t>(alpha | ((r & 0xf8) << 7) | ((g & 0xf8) << 2) | (b >> 3));
    }
    fonts.source_word(font, 0x20) = target.upload_font_atlas({256, 256, words});
    for (auto &word : words)
        word ^= 0x7fff;
    fonts.source_word(font, 0x24) = target.upload_font_atlas({256, 256, words});
}
void initialize_retail_font_resources(RetailFontResources &fonts, MaterialFileSource &files, FontImageGlobals image,
                                      FontReloadViews reload, std::string_view root, std::int32_t width,
                                      std::uint32_t enabled, FontAtlasUploadTarget &target, FontAtlasObserver *observer)
{
    fonts.require_preview_mode();
    require(fonts.glyph_map[32] < 160, "font retained space glyph outside source bank");
    require(root.find('\0') == std::string_view::npos && root.size() + 23 < 128,
            "font source path exceeds original local buffer");
    fonts.digit_advance_bits = 0;
    fonts.current = 1;
    fonts.count = 0;
    const auto index_read =
        files.read_source_file({MaterialResourceRoot::graphics24, "\\Fonts\\INDEX.TXT"}, fonts.index);
    require(index_read.source_size != 0 && index_read.copied_bytes == 160, "incomplete font index source read");
    const unsigned count = width >= 1024 ? 2 : 1;
    for (unsigned font = 0; font < count; ++font)
    {
        fonts.current = font + 1;
        const std::string name = font == 1 ? "I32_b025" : width >= 1152 ? "I32" : "I32_b05";
        const std::string relative = "\\Fonts\\" + name + ".TGA";
        const auto raw = read_font(files, {MaterialResourceRoot::graphics24, relative});
        const auto rgb = decode_font(raw, image);
        const auto atlas = prepare_retail_font_atlas(fonts, font, rgb, image.width, image.height, width);
        if (observer)
            observer->packed_atlas(font, atlas);
        upload_retail_font_atlases(fonts, font, atlas, enabled, target);
        append_font_reload_42be90(reload, std::string(root) + relative);
        ++fonts.count;
    }
    fonts.glyph_map.fill(0);
    for (unsigned i = 0; i < 160; ++i)
        fonts.glyph_map[fonts.index[i]] = static_cast<std::uint8_t>(i);
    fonts.glyph_map[32] = 153;
    fonts.current = 1;
}
namespace
{
void decode_source_font_tga(SourceFontContext c, const SourceAllocationRef &output, FontImageGlobals image)
{
    const auto raw = c.bank.bytes(c.decode.file);
    auto rgb = c.bank.bytes(output);
    if (raw.size() < 18)
        throw SourceAllocationError(0x4622cd, "font TGA header exceeds actual file allocation");
    if (raw[2] != 2 || raw[16] != 24)
        throw SourceAllocationError(0x4622e0, "font TGA source fatal: expected type2 RGB24");
    const auto width = half(raw, 12), height = half(raw, 14), start = 18u + raw[0];
    c.decode.pixel_depth = 24; // 4622F0
    image.tga_width = width;   // 462300
    image.width = width;       // 46230A
    image.tga_height = height; // 462313
    image.height = height;     // 462318
    c.decode.pixel_cursor_allocation = c.decode.file;
    c.decode.pixel_cursor_offset = start; // 46231D
    const auto extent = std::uint64_t(width) * height * 3;
    if (width == 0 || height == 0 || start > raw.size() || extent > raw.size() - start || extent > rgb.size())
        throw SourceAllocationError(0x462340, "font decoder source extent exceeds actual input/output allocation");
    // 462310 tests the accepted TYPE byte2, not descriptor bit0x20. Source
    // rows advance forward while the RGB output rows run backwards.
    for (std::uint32_t row = 0; row < height; ++row)
    {
        for (std::uint32_t x = 0; x < width; ++x)
        {
            const auto from = std::size_t(c.decode.pixel_cursor_offset) + std::size_t(x) * 3;
            const auto to = (std::size_t(height - 1 - row) * width + x) * 3;
            rgb[to] = raw[from + 2];
            rgb[to + 1] = raw[from + 1];
            rgb[to + 2] = raw[from];
        }
        c.decode.pixel_cursor_offset += width * 3; // 46239C; retained after the file is freed
    }
    observe_bank(c.observer, c.bank, FontBankEventKind::decoded, 0x46241a, output,
                 static_cast<std::uint32_t>(rgb.size()));
}
void read_decode_source_font(SourceFontContext c, MaterialFileSource &files, const MaterialResourcePath &path,
                             const SourceAllocationRef &output, FontImageGlobals image)
{
    const auto size = files.source_file_size(path); // actual SECOND size query at4623D0
    if (size == 0)
        throw SourceAllocationError(0x4623f0, "font source file not found;4623C0 fatal boundary");
    if (size > 16 * 1024 * 1024)
        throw SourceAllocationError(0x4623f9, "font file outside bounded source input domain");
    c.decode.file = allocate_font(c, 0x4623f9, size); // 62F5CC is not cleared by a later free
    require_allocation(c.decode.file, 0x462408);
    const auto read = files.read_source_file(path, c.bank.bytes(c.decode.file));
    observe_bank(c.observer, c.bank, FontBankEventKind::read, 0x462408, c.decode.file, size);
    if (read.source_size == 0 || read.copied_bytes != size)
        throw SourceAllocationError(0x462408, "font short source read outside admitted complete-file domain");
    decode_source_font_tga(c, output, image);
    release_font(c, 0x462425, c.decode.file);
}
} // namespace
void upload_source_font_atlases_4327e0(RetailFontResources &fonts, std::size_t font, SourceFontContext c,
                                       const SourceAllocationRef &atlas, std::uint32_t enabled,
                                       FontAtlasUploadTarget &target)
{
    fonts.bind_source_bank(c.bank);
    require(font < 2, "source font upload outside two records");
    if (enabled != 1)
        return; // 4327EE retains both old indices and all atlas RGB bytes
    auto bytes = c.bank.bytes(atlas);
    require(bytes.size() == 256 * 256 * 3, "source font upload requires the actual0x30000 atlas");
    static_assert(std::endian::native == std::endian::little && sizeof(std::uint16_t) == 2 &&
                  alignof(std::uint16_t) <= 32);
    // The exact bank core uses raw malloc storage aligned to32. Its implicit
    // uint16_t array objects are byte-addressable during RGB packing (C++20
    // implicit object creation), then consumed here as native little-endian
    // words. This is not a cast of a std::vector<uint8_t> or an x86 pointer.
    std::span<std::uint16_t> words{reinterpret_cast<std::uint16_t *>(bytes.data()), 256 * 256};
    auto &transparency = target.current_transparency();
    for (std::size_t i = 0; i < words.size(); ++i)
    {
        auto r = bytes[3 * i], g = bytes[3 * i + 1], b = bytes[3 * i + 2];
        std::uint16_t alpha = 0x8000;
        if (r < 8 && g < 8 && b < 8)
            r = g = b = 8;
        else if (r == 255 && g == 0 && b == 255)
        {
            r = g = b = 0;
            alpha = 0;
            transparency = 1;
        }
        words[i] = static_cast<std::uint16_t>(alpha | ((r & 0xf8) << 7) | ((g & 0xf8) << 2) | (b >> 3));
    }
    observe_bank(c.observer, c.bank, FontBankEventKind::upload, 0x43280d, atlas,
                 static_cast<std::uint32_t>(bytes.size()));
    fonts.source_word(font, 0x20) = target.upload_font_atlas({256, 256, words});
    for (auto &word : words)
        word ^= 0x7fff; // 4327C0 changes the SAME allocation in place
    observe_bank(c.observer, c.bank, FontBankEventKind::upload, 0x432832, atlas,
                 static_cast<std::uint32_t>(bytes.size()));
    fonts.source_word(font, 0x24) = target.upload_font_atlas({256, 256, words});
}
void load_source_font_resource_428e40(RetailFontResources &fonts, std::size_t font, SourceFontContext c,
                                      MaterialFileSource &files, FontImageGlobals image, FontReloadViews reload,
                                      std::string_view root, std::string_view name, std::int32_t width,
                                      std::uint32_t enabled, FontAtlasUploadTarget &target, FontAtlasObserver *observer)
{
    fonts.bind_source_bank(c.bank);
    require(font < 2 && fonts.current == font + 1, "428E40 requires its actual selected font record");
    require(!name.empty() && name.find_first_of("\\/:\0", 0, 4) == name.npos && root.find('\0') == root.npos &&
                root.size() + name.size() + 12 < 128,
            "font source path outside bounded original local buffer");
    const MaterialResourcePath path{MaterialResourceRoot::graphics24, "\\Fonts\\" + std::string(name) + ".TGA"};
    const auto size = files.source_file_size(path); // 428E6C, even when size is0
    if (size > 16 * 1024 * 1024)
        throw SourceAllocationError(0x428e75, "font RGB allocation outside bounded input domain");
    const auto rgb = allocate_font(c, 0x428e75, size);
    require_allocation(rgb, 0x428e85);
    read_decode_source_font(c, files, path, rgb, image);
    const auto atlas = allocate_font(c, 0x428e91, 0x30000);
    require_allocation(atlas, 0x428e9d);
    prepare_source_font_atlas_428c00(fonts, font, c, rgb, atlas, image.width, image.height, width);
    observe_bank(c.observer, c.bank, FontBankEventKind::packed, 0x4327e0, atlas, 0x30000);
    if (observer)
        observer->packed_atlas(font, c.bank.bytes(atlas));
    upload_source_font_atlases_4327e0(fonts, font, c, atlas, enabled, target);
    append_font_reload_42be90(reload, std::string(root) + path.relative_name);
    release_font(c, 0x428eb7, atlas);
    release_font(c, 0x428ebe, rgb);
    ++fonts.count; // 428EC3..428ECB, AFTER both source frees
}
void initialize_retail_font_resources(RetailFontResources &fonts, SourceFontContext c, MaterialFileSource &files,
                                      FontImageGlobals image, FontReloadViews reload, std::string_view root,
                                      std::int32_t width, std::uint32_t enabled, FontAtlasUploadTarget &target,
                                      FontAtlasObserver *observer)
{
    fonts.bind_source_bank(c.bank);
    require(fonts.glyph_map[32] < 160, "font retained space map outside source glyph owner");
    require(root.find('\0') == root.npos && root.size() + 23 < 128, "font source root exceeds original local buffer");
    fonts.digit_advance_bits = 0;
    fonts.current = 1;
    fonts.count = 0;
    const auto index_read =
        files.read_source_file({MaterialResourceRoot::graphics24, "\\Fonts\\INDEX.TXT"}, fonts.index);
    require(index_read.source_size != 0 && index_read.copied_bytes == 160, "incomplete font index source read");
    const unsigned count = width >= 1024 ? 2 : 1;
    for (unsigned font = 0; font < count; ++font)
    {
        fonts.current = font + 1;
        // Pinned428FA0 strings: both1280/1152 branches useI32/I32_b025;
        // 1024 selectsI32_b05/I32_b025; lower widths selectI32_b05 alone.
        const std::string_view name = font == 1 ? "I32_b025" : width >= 1152 ? "I32" : "I32_b05";
        load_source_font_resource_428e40(fonts, font, c, files, image, reload, root, name, width, enabled, target,
                                         observer);
    }
    fonts.glyph_map.fill(0);
    for (unsigned i = 0; i < 160; ++i)
        fonts.glyph_map[fonts.index[i]] = static_cast<std::uint8_t>(i);
    fonts.glyph_map[32] = 153;
    fonts.current = 1;
}
} // namespace sgn::retail
