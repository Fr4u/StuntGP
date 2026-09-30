#include "sgn/session_effect_resources.hpp"
#include "sgn/retail_world_sprite_model.hpp"
#include "sgn/session_world_frame.hpp"
#include <algorithm>

namespace sgn::retail
{
void EffectAnimationStorage::refresh_projection(std::uint32_t slot) noexcept
{
    const auto &row = words_[slot];
    // 45E53B..45E56D: wrapping32-bit pointer subtraction, signed /28.
    // The later sprite narrows to16 bits. Never substitute row+4 GPU handle.
    const auto index = std::bit_cast<std::int32_t>(row[0] - 0x6227d4u) / 28;
    const auto identity = source_base + slot * 28u;
    textures_[slot] = {identity, static_cast<std::uint32_t>(index)};
    frames_[slot] = {identity, row[2]}; // 45E1A9 descriptor+8
}
void EffectAnimationStorage::clear_scene_42e510() noexcept
{
    words_ = {};
    cursor_ = 0;
    cursor_active_ = false;
    authored_count_ = 0;
    textures_ = {};
    frames_ = {}; // invalidate derived views, not extra source writes
}
void EffectAnimationStorage::reset_loading_cursor_42c480() noexcept
{
    cursor_ = 0;
    cursor_active_ = true;
}
void EffectAnimationStorage::recovered_entry_425a70(const PhysicalMaterialStorage &materials, std::uint32_t ecx,
                                                    std::uint32_t edx)
{
    // Host bounds are checked before mutations. They restrict the source's
    // unchecked pointer accesses to the actual typed allocations.
    if (!cursor_active_ || cursor_ >= capacity)
        throw FormatError("effect descriptor cursor outside source bank");
    if (!materials.active())
        throw FormatError("effect descriptor requires initialized physical material cursor");
    const auto first = materials.source_cursor() - ecx * 28u; // 425A70..425A83, modulo32
    const auto offset = first - 0x6227d4u;
    if (offset % 28u != 0 || offset / 28u >= PhysicalMaterialStorage::capacity)
        throw FormatError("effect descriptor first physical row outside source allocation");
    const auto first_word = materials.source_word(offset / 28u, 0); // 425A8C
    const auto slot = cursor_;
    words_[slot] = {first, first_word, ecx, 0u, 1u, edx, 2u}; // 425A8A..425AA7
    ++cursor_;                                                // 425AAE/425AB3
    if (edx == 0)
        flag_577658_ = 1; // 425AB8..425ABA; otherwise RETAIN old flag
    authored_count_ = std::max(authored_count_, cursor_);
    refresh_projection(slot);
}
std::span<const std::uint32_t, 7> EffectAnimationStorage::source_record(std::uint32_t slot) const &
{
    if (slot >= capacity)
        throw FormatError("effect descriptor source slot outside bank");
    return words_[slot];
}
void EffectSpriteColorStorage::initialize_allocation_434090(WorldModelDrawState &model)
{
    initialize_world_sprite_model(model, packets_);
    audit_ = {}; // new diagnostic lifetime, ONLY alongside the actual allocation init
}
void EffectSpriteColorStorage::initialize_allocation_434090(WorldModelDrawState &model, SceneResources &scene)
{
    initialize_world_sprite_model(model, packets_, scene);
    audit_ = {};
}
contact::effects::EffectSpriteColorAudit &EffectSpriteColorStorage::borrow_for(SourceSpritePacketStorage &packets) &
{
    if (&packets != &packets_)
        throw FormatError("effect color audit belongs to another sprite allocation");
    return audit_;
}
void SessionEffectResourceStorage::publish(SourceSpritePacketStorage &packets, SessionWorldFrameResources &resources) &
{
    // Validate the allocation identity before changing any caller view.
    auto &audit = colors.borrow_for(packets);
    resources.effect_animations = animations.textures();
    resources.adjacent_angles = angles.samples();
    resources.effect_colors = &audit;
}
} // namespace sgn::retail
