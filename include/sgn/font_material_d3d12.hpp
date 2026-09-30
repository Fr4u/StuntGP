#pragma once
#include "sgn/material_d3d12.hpp"
#include "sgn/retail_font_resources.hpp"

namespace sgn
{
// Each reference borrows the sole primary word of one physical material slot.
// The rest of its record remains in the caller's owner, never copied or reset.
struct FontMaterialPrimarySlot
{
    std::uint32_t &primary_texture;
};
class FontMaterialD3D12Target final : public retail::FontAtlasUploadTarget
{
    MaterialD3D12 &backend_;
    std::vector<std::uint32_t *> primary_words_; // immutable bindings, not values
    std::uint32_t &material_cursor_;             // 6243B8 normalized physical index, not58271C
    std::uint32_t expected_cursor_, expected_resources_;
    const std::uint64_t epoch_;
    retail::PhysicalMaterialStorage *material_owner_{};
    std::uint64_t material_generation_{};
    void require_binding() const;

  public:
    // Isolated raw-word fixture boundary: it cannot detect a physical reset
    // that restores the same cursor without changing the backend epoch.
    // Borrowed words and cursor must remain live at stable addresses. This
    // synchronous binding expires on backend replacement/release, an unrelated
    // upload, or external cursor advance. It does not synthesize future slots.
    FontMaterialD3D12Target(MaterialD3D12 &, std::span<const FontMaterialPrimarySlot>, std::uint32_t &material_cursor);
    // Production/world binding: also observes the sole physical owner's active
    // tag and reset generation. Scope to one synchronous font initialization.
    FontMaterialD3D12Target(MaterialD3D12 &, retail::PhysicalMaterialStorage &);
    FontMaterialD3D12Target(const FontMaterialD3D12Target &) = delete;
    FontMaterialD3D12Target &operator=(const FontMaterialD3D12Target &) = delete;
    std::uint8_t &current_transparency() override;
    std::uint32_t upload_font_atlas(retail::PreparedMaterialPixels) override;
};
} // namespace sgn
