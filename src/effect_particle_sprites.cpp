#include "sgn/effect_particle_sprites.hpp"
#include "sgn/retail_camera_tables.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail::contact::effects
{
namespace
{
std::uint32_t integer(double value)
{
    if (!std::isfinite(value) || value < -9223372036854775808.0 || value >= 9223372036854775808.0)
        throw FormatError("effect sprite integer conversion outside signed64 domain");
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
}
float sample(std::uint32_t index, std::span<const EffectAngleSample> adjacent)
{
    if (index < camera_detail::trig_bits.size())
        return std::bit_cast<float>(camera_detail::trig_bits[index]);
    const float *found = nullptr;
    for (const auto &entry : adjacent)
        if (entry.index == index)
        {
            if (found)
                throw FormatError("duplicate adjacent effect angle sample");
            found = &entry.value;
        }
    if (!found)
        throw FormatError("effect sprite requires adjacent source angle sample");
    return *found;
}
std::uint32_t texture(const EffectParticle &p, std::span<const EffectAnimationTexture> animations)
{
    if (!p.animation_binding)
        return p.texture;
    const EffectAnimationTexture *found = nullptr;
    for (const auto &entry : animations)
        if (entry.identity == p.animation_binding)
        {
            if (found)
                throw FormatError("duplicate effect texture animation identity");
            found = &entry;
        }
    if (!found)
        throw FormatError("unresolved effect texture animation identity");
    return found->first_texture + static_cast<std::uint32_t>(std::bit_cast<std::int32_t>(p.animation_frame) / 4096);
}
CameraVector direction(const EffectParticle &p, std::span<const EffectAngleSample> adjacent)
{
    if (!(p.flags & 4))
        return {0, 1, 0, 0};
    const auto index = integer(double(p.angle) * std::bit_cast<float>(std::uint32_t{0x41b60b61}));
    // Literal45E4B0 has no modulo. Cosine uses source4945D8=48C5D8,
    // exactly2048 words after the sine base, with source32-bit address wrap.
    return {sample(index + 2048u, adjacent), sample(index, adjacent), 0, 0};
}
SourceSpriteRecord initial()
{
    // Exact104-byte source template486538; fields not rewritten by45E4F0
    // remain source words, including opaque rotation/scale payload.
    constexpr std::array<std::uint32_t, 26> words{
        32, 8388609, 16777215, 16777215,   16777215, 16777215, 0,          0, 0, 0, 1065353216, 1065353216, 1065353216,
        0,  0,       0,        3212836864, 0,        0,        1065353216, 0, 0, 1, 0,          0,          2};
    return std::bit_cast<SourceSpriteRecord>(words);
}
} // namespace
void append_effect_particle_sprites(const EffectParticleBank &bank, SourceSpritePacketStorage &storage,
                                    LightCameraView view, std::span<const EffectAnimationTexture> animations,
                                    std::span<const EffectAngleSample> adjacent, EffectSpriteColorAudit *audit)
{
    if (bank.active_signal == 0)
        return;
    for (const auto &p : bank.particles)
        if (p.active == 1 && p.render_enabled == 1)
        {
            if (p.source_undefined[6] && !audit)
                throw FormatError("effect sprite color has no source writer");
            (void)texture(p, animations);
            (void)direction(p, adjacent);
            (void)integer(p.alpha);
        }
    auto sprite = initial();
    for (const auto &p : bank.particles)
    {
        if (p.active != 1 || p.render_enabled != 1)
            continue;
        sprite.texture = static_cast<std::uint16_t>(texture(p, animations));
        sprite.up = direction(p, adjacent);
        sprite.alpha = static_cast<std::uint16_t>(integer(p.alpha));
        sprite.flags = p.render_flags;
        sprite.colors.fill(p.color);
        sprite.position = {p.position.x, p.position.y, p.position.z, std::bit_cast<float>(p.position.opaque)};
        sprite.scale.fill(p.scale);
        const auto first_attribute = storage.attribute_count;
        if (append_sprite_packet(storage, sprite, view) && audit)
            for (auto i = first_attribute; i < storage.attribute_count; ++i)
                audit->source_undefined[i] = p.source_undefined[6];
    }
}
} // namespace sgn::retail::contact::effects
