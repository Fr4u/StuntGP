#include "sgn/font_material_d3d12.hpp"
#include "sgn/format_require.hpp"
#include <algorithm>

namespace sgn
{
namespace
{
using sgn::format_check::require;
std::uint32_t &checked_cursor(MaterialD3D12 &backend, retail::PhysicalMaterialStorage &owner)
{
    backend.require_font_upload_ready();
    return owner.active_cursor();
}
} // namespace
FontMaterialD3D12Target::FontMaterialD3D12Target(MaterialD3D12 &backend, std::span<const FontMaterialPrimarySlot> slots,
                                                 std::uint32_t &cursor)
    : backend_(backend), material_cursor_(cursor), expected_cursor_(0), expected_resources_(backend.resource_entries()),
      epoch_(backend.resource_epoch())
{
    // Backend call above checks its owner thread before any borrowed value read.
    backend_.require_font_upload_ready();
    require(!slots.empty() && slots.size() <= 255, "font physical primary binding extent outside source bank");
    require(cursor <= slots.size(), "font physical material cursor outside bound bank");
    expected_cursor_ = cursor;
    primary_words_.reserve(slots.size());
    for (const auto &slot : slots)
    {
        auto *pointer = &slot.primary_texture;
        require(pointer != &cursor &&
                    std::find(primary_words_.begin(), primary_words_.end(), pointer) == primary_words_.end(),
                "font physical primary bindings alias another owner");
        primary_words_.push_back(pointer);
    }
    require_binding();
}
FontMaterialD3D12Target::FontMaterialD3D12Target(MaterialD3D12 &backend, retail::PhysicalMaterialStorage &owner)
    : backend_(backend), material_cursor_(checked_cursor(backend, owner)), expected_cursor_(material_cursor_),
      expected_resources_(backend.resource_entries()), epoch_(backend.resource_epoch()), material_owner_(&owner),
      material_generation_(owner.generation())
{
    primary_words_.reserve(retail::PhysicalMaterialStorage::capacity);
    for (std::uint32_t slot = 0; slot < retail::PhysicalMaterialStorage::capacity; ++slot)
        primary_words_.push_back(&owner.record(slot).primary_texture);
    require_binding();
}
void FontMaterialD3D12Target::require_binding() const
{
    backend_.require_font_upload_ready(); // owner thread/failure state first
    const auto resources = backend_.resource_entries();
    require(resources == expected_resources_ && backend_.resource_epoch() == epoch_,
            "font target resource generation or allocation cursor changed");
    if (material_owner_)
        require(material_owner_->active() && material_owner_->generation() == material_generation_,
                "font target physical material lifetime changed");
    require(material_cursor_ == expected_cursor_ && material_cursor_ <= primary_words_.size(),
            "font target physical material cursor changed");
}
std::uint8_t &FontMaterialD3D12Target::current_transparency()
{
    require_binding();
    return backend_.current_transparency();
}
std::uint32_t FontMaterialD3D12Target::upload_font_atlas(retail::PreparedMaterialPixels pixels)
{
    require_binding();
    require(material_cursor_ < primary_words_.size(), "font target has no bound next-free material slot");
    const auto index = material_cursor_;
    const auto token = backend_.upload_font_primary(pixels);
    // The only material write in432785. All other physical words remain in
    // their original owners, including a nonzero430D40 next-free alternate.
    *primary_words_[index] = token;
    ++material_cursor_;
    ++expected_cursor_;
    ++expected_resources_;
    return index;
}
} // namespace sgn
