#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace sgn::retail
{
struct FontReloadViews;

// One stable owner for57E310,57EB0C,5895C0 and their three cursors.
// 58271C is deliberately borrowed from the ordinary material owner.
struct MaterialReloadStorage
{
    std::array<std::uint32_t, 510> ordinary_records{};
    std::array<std::uint8_t, 8160> name_arena{};
    std::array<std::uint32_t, 16> font_records{};
    std::uint32_t ordinary_cursor{}, name_cursor{}, font_cursor{};
    bool initialized{}; // Host lifetime guard, not a recovered global.
    MaterialReloadStorage() = default;
    MaterialReloadStorage(const MaterialReloadStorage &) = delete;
    MaterialReloadStorage &operator=(const MaterialReloadStorage &) = delete;
    MaterialReloadStorage(MaterialReloadStorage &&) = delete;
    MaterialReloadStorage &operator=(MaterialReloadStorage &&) = delete;
};
void reset_material_reload_42bdd0(MaterialReloadStorage &storage);
void append_material_reload_42be20(MaterialReloadStorage &storage, const std::uint32_t &ordinary_material_count,
                                   std::string_view source_path);
FontReloadViews font_reload_views(MaterialReloadStorage &storage, const std::uint32_t &ordinary_material_count);

// Host adapter: the caller supplies readable storage including a terminator.
// An empty span or an unterminated span is rejected without reading beyond it.
std::string_view material_reload_c_string_prefix(std::span<const char> input);
struct MaterialReloadRecordView
{
    std::string_view source_path;
    std::uint32_t ordinary_material_index;
};
MaterialReloadRecordView material_reload_record(const MaterialReloadStorage &storage,
                                                std::uint32_t ordinary_record_index);

struct SourceBootConfigState;
// NEW bounded borrow of the actual source buffer, including its NUL storage.
// The extent is retained, not the current string length. Every text() rereads
// the current terminator, so later shorter/longer source stores stay visible.
// The caller retains the source owner; this view never owns or copies bytes.
class SourceGraphicsRoot final
{
    std::span<const char> bytes_;

  public:
    explicit SourceGraphicsRoot(std::span<const char> bytes) : bytes_(bytes) {}
    template <std::size_t N> SourceGraphicsRoot(const char (&bytes)[N]) : bytes_(bytes) {}
    std::string_view text() const
    {
        return material_reload_c_string_prefix(bytes_);
    }
    operator std::string_view() const
    {
        return text();
    }
    bool borrows(std::span<const char> bytes) const noexcept
    {
        return bytes_.data() == bytes.data() && bytes_.size() == bytes.size();
    }
};
struct SourceGraphicsRoots
{
    SourceGraphicsRoot normal, maximum;
};
SourceGraphicsRoots source_graphics_roots_from_config(const SourceBootConfigState &);
SourceGraphicsRoots source_graphics_roots_from_config(const SourceBootConfigState &&) = delete;
bool source_graphics_roots_borrow_config(const SourceGraphicsRoots &, const SourceBootConfigState &) noexcept;
// Legacy explicit PE-only fixture views. Never use these for source cold loading;
// the normal caller binds the existing platform's boot_config instead.
SourceGraphicsRoots source_graphics_roots_from_pe();
} // namespace sgn::retail
