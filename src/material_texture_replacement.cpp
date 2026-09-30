#include "sgn/material_texture_replacement.hpp"

namespace sgn
{
MaterialTextureReplacement::MaterialTextureReplacement(MaterialD3D12 &backend,
                                                       retail::PhysicalMaterialStorage &materials)
    : backend_(backend), materials_(materials), material_generation_(0)
{
    const auto count = backend_.resource_entries();
    if (count == 0 || !materials_.active() || count != materials_.size())
        throw FormatError("material replacement requires a completed physical material owner");
    material_generation_ = materials_.generation();
    primary_tokens_.reserve(count);
    for (std::uint32_t slot = 0; slot < count; ++slot)
        primary_tokens_.push_back(backend_.resource_primary_token(slot));
}
void MaterialTextureReplacement::require_binding(std::uint32_t slot) const
{
    const auto resource_count = backend_.resource_entries(); // Owner-thread check precedes all borrowed material reads.
    if (slot >= primary_tokens_.size() || !materials_.active() || materials_.generation() != material_generation_ ||
        materials_.size() != resource_count || resource_count < primary_tokens_.size() ||
        backend_.resource_primary_token(slot) != primary_tokens_[slot])
        throw FormatError("material replacement owner was released, changed, or has an invalid slot");
}
std::uint16_t MaterialTextureReplacement::destination_width(std::uint32_t slot) const
{
    require_binding(slot);
    return materials_.dimension(slot); // 6227DC+28*slot, independent of GPU descriptor.
}
void MaterialTextureReplacement::upload_to_destination(std::uint32_t slot, const Image &image)
{
    require_binding(slot);
    if (image.width != materials_.dimension(slot) || std::uint64_t(image.width) * image.height * 4 != image.rgba.size())
        throw FormatError("material replacement image does not match its destination");
    // Reject the one-past source address (it aliases6243B8) before GPU work.
    (void)materials_.next_index();
    std::vector<std::uint16_t> words;
    words.reserve(image.rgba.size() / 4);
    for (std::size_t i = 0; i < image.rgba.size(); i += 4)
    {
        // Exact inverse of decode_tm's5bit expansion for recovered PC assets.
        // This adapter does not infer a material transparency byte from alpha.
        const auto r = (std::uint32_t(image.rgba[i]) * 31 + 127) / 255;
        const auto g = (std::uint32_t(image.rgba[i + 1]) * 31 + 127) / 255;
        const auto b = (std::uint32_t(image.rgba[i + 2]) * 31 + 127) / 255;
        words.push_back(static_cast<std::uint16_t>((image.rgba[i + 3] ? 0x8000u : 0u) | (r << 10) | (g << 5) | b));
    }
    backend_.replace_primary(slot, {image.width, image.height, words});
    // 430F24..430F2B: this cursor is the next free record, not slot15. No
    // destination alternate token, dimension, or published count is written.
    auto &next = materials_.record(materials_.next_index());
    next.primary_texture = primary_tokens_[slot];
    next.alternate_texture = primary_tokens_[slot];
}
} // namespace sgn
