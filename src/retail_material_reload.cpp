#include "sgn/retail_material_reload.hpp"
#include "sgn/source_boot_config.hpp"
#include "sgn/retail_font_resources.hpp"
#include "sgn/format_require.hpp"
#include <algorithm>
#include <memory>

namespace sgn::retail
{
namespace
{
using sgn::format_check::require;
bool overlaps_owner(const MaterialReloadStorage &storage, const void *data, std::size_t size)
{
    if (size == 0)
        return false;
    const auto first = reinterpret_cast<std::uintptr_t>(std::addressof(storage));
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    // Subtraction avoids wrapping either interval's end address.
    return address >= first ? address - first < sizeof(storage) : first - address < size;
}
void require_count(const MaterialReloadStorage &storage, const std::uint32_t &count)
{
    require(!overlaps_owner(storage, std::addressof(count), sizeof(count)),
            "ordinary material count must have its own source owner");
}
} // namespace
void reset_material_reload_42bdd0(MaterialReloadStorage &storage)
{
    // 42BDDD,42BDE9,42BDF5; only then42BDF7..42BE15 publish base cursors.
    storage.ordinary_records.fill(0);
    storage.name_arena.fill(0);
    storage.font_records.fill(0);
    storage.ordinary_cursor = 0;
    storage.name_cursor = 0;
    storage.font_cursor = 0;
    storage.initialized = true;
}
void append_material_reload_42be20(MaterialReloadStorage &storage, const std::uint32_t &ordinary_material_count,
                                   std::string_view path)
{
    // New host checks precede all source stores. Original42BE20 is unbounded.
    require(storage.initialized, "material reload storage needs42BDD0 initialization");
    require_count(storage, ordinary_material_count);
    require(storage.ordinary_cursor < 254, "ordinary reload table needs terminal name slot");
    require(storage.name_cursor < storage.name_arena.size() &&
                path.size() < storage.name_arena.size() - storage.name_cursor,
            "shared reload name arena exhausted");
    require(!overlaps_owner(storage, path.data(), path.size()), "source path overlaps reload storage");
    require(path.data() != nullptr || path.empty(), "reload source path is null");
    require(path.find('\0') == std::string_view::npos, "reload source path contains embedded NUL");
    const auto at = storage.ordinary_cursor * 2;
    storage.ordinary_records[at] = storage.name_cursor + 1;     // 42BE34, offset-plus-one handle
    storage.ordinary_records[at + 1] = ordinary_material_count; // 42BE36, live58271C
    storage.ordinary_records[at + 2] = 0;                       // 42BE3E retains the next second word
    ++storage.ordinary_cursor;                                  // 42BE44 publishes before copying the name
    if (!path.empty())
        std::copy_n(path.data(), path.size(), storage.name_arena.data() + storage.name_cursor);
    storage.name_arena[storage.name_cursor + path.size()] = 0;          // 42BE5F/42BE68 include NUL
    storage.name_cursor += static_cast<std::uint32_t>(path.size() + 1); // 42BE7B, last
}
FontReloadViews font_reload_views(MaterialReloadStorage &storage, const std::uint32_t &count)
{
    require(storage.initialized, "font reload views need42BDD0 initialization");
    require_count(storage, count);
    return {storage.name_arena, storage.name_cursor, storage.font_records, storage.font_cursor, count};
}
std::string_view material_reload_c_string_prefix(std::span<const char> input)
{
    require(!input.empty(), "reload source input lacks a bounded NUL terminator");
    const auto end = std::find(input.begin(), input.end(), '\0');
    require(end != input.end(), "reload source input lacks a bounded NUL terminator");
    return {input.data(), static_cast<std::size_t>(end - input.begin())};
}
MaterialReloadRecordView material_reload_record(const MaterialReloadStorage &storage, std::uint32_t index)
{
    require(storage.initialized && storage.ordinary_cursor <= 254 && index < storage.ordinary_cursor,
            "ordinary reload record outside published table");
    const auto handle = storage.ordinary_records[index * 2];
    require(storage.name_cursor <= storage.name_arena.size() && handle != 0 && handle <= storage.name_cursor,
            "ordinary reload name handle outside published arena");
    const auto first = storage.name_arena.begin() + handle - 1;
    const auto limit = storage.name_arena.begin() + storage.name_cursor;
    const auto end = std::find(first, limit, std::uint8_t{0});
    require(end != limit, "ordinary reload name has no published terminator");
    return {{reinterpret_cast<const char *>(std::addressof(*first)), static_cast<std::size_t>(end - first)},
            storage.ordinary_records[index * 2 + 1]};
}
SourceGraphicsRoots source_graphics_roots_from_config(const SourceBootConfigState &config)
{
    return {SourceGraphicsRoot(std::span<const char>(config.graphics_path)),
            SourceGraphicsRoot(std::span<const char>(config.maximum_graphics_path))};
}
bool source_graphics_roots_borrow_config(const SourceGraphicsRoots &roots, const SourceBootConfigState &config) noexcept
{
    return roots.normal.borrows(config.graphics_path) && roots.maximum.borrows(config.maximum_graphics_path);
}
SourceGraphicsRoots source_graphics_roots_from_pe()
{
    // Pinned D3D PE48491C/48499C; source formats4777D4/4777DC add no separator.
    return {"D:\\Game\\Graphics24\\", "D:\\Game\\Graphics24Max\\"};
}
} // namespace sgn::retail
