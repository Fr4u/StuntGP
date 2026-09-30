#include "sgn/source_race_view_stage.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_camera_tables.hpp"
#include "sgn/host_profiler.hpp"
#include "sgn/host_presentation.hpp"
#include <bit>
#include <cstddef>
#include <type_traits>

extern "C" std::uint32_t sgn_source_effect_angle_45e4b3(const float *) noexcept;
extern "C" std::uint32_t sgn_source_effect_alpha_45e5ad(const float *) noexcept;
extern "C" void sgn_source_impact_scale_41606d(const std::int32_t *, const float *, float *) noexcept;

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using Particle = contact::effects::EffectParticle;
static_assert(std::is_standard_layout_v<Particle>);
static_assert(offsetof(Particle, animation_binding) == 0x80 && offsetof(Particle, color) == 0xcc);
static_assert(offsetof(Particle, alpha) == 0xb8 && offsetof(Particle, angle) == 0xbc);
void admit(RaceSessionPlatform &p, W site)
{
    p.require_owner_thread();
    if (!p.source_construction_only_entry())
        throw SourceAllocationError(site, "race view stage requires actual source platform");
}
LightCameraView camera(RaceSessionState &w, W site)
{
    const auto i = w.controls.current;
    if (i >= w.controls.controllers.size() || i >= w.camera_lifecycle.size() || i >= w.camera_presentation.size() ||
        i >= w.overlays.size())
        throw SourceAllocationError(site, "source current camera leaves actual controller bank");
    return {w.controls.controllers[i].camera,
            w.camera_lifecycle[i],
            w.camera_presentation[i],
            w.overlays[i],
            &w.view_matrices,
            &w.shared_model_identity};
}
SourceSpriteRecord initial_486538()
{
    constexpr std::array<W, 26> words{0x20, 0x800001,   0xffffff,   0xffffff,   0xffffff, 0xffffff, 0, 0,          0,
                                      0,    0x3f800000, 0x3f800000, 0x3f800000, 0,        0,        0, 0xbf800000, 0,
                                      0,    0x3f800000, 0,          0,          1,        0,        0, 2};
    return std::bit_cast<SourceSpriteRecord>(words);
}
float sample(RaceSessionState &w, W index, W site)
{
    // Actual32-bit scaled-address wrap. No modulo8192 and no synthesized trig.
    index &= 0x3fffffffu;
    if (index < camera_detail::trig_bits.size())
        return std::bit_cast<float>(camera_detail::trig_bits[index]);
    for (const auto &a : w.effect_resources.angles.samples())
        if (a.index == index)
            return a.value;
    throw SourceAllocationError(site, "effect angle reaches an unrepresented adjacent source word");
}
std::uint16_t texture(RaceSessionState &w, const Particle &p)
{
    const auto binding = p.animation_binding; // 45E538, after active/render predicates
    if (binding == 0)
        return static_cast<std::uint16_t>(p.texture); // 45E56E
    constexpr W base = EffectAnimationStorage::source_base;
    if (binding < base || (binding - base) % 28u != 0 || (binding - base) / 28u >= EffectAnimationStorage::capacity)
        throw SourceAllocationError(0x45e53e, "effect texture descriptor leaves actual16-record bank");
    // Read the real row, including an untouched source-zero row. The derived
    // authored_count cache is not a substitute for this pointed DWORD.
    const W first = w.effect_resources.animations.source_record((binding - base) / 28u)[0];
    const auto ordinal = std::bit_cast<std::int32_t>(first - 0x6227d4u) / 28;
    const auto frame = std::bit_cast<std::int32_t>(p.animation_frame) / 4096;
    return static_cast<std::uint16_t>(static_cast<W>(ordinal) + static_cast<W>(frame));
}
void mark_colors(RaceSessionState &w, bool undefined)
{
    const auto &current = w.sprite_packets.current_attributes.source_binding();
    const auto &base = w.sprite_packets.attributes.source().source_binding();
    // Diagnostic metadata only; no source count or payload read/write.
    if (current.bank != base.bank || current.allocation != base.allocation || current.byte_offset < base.byte_offset ||
        (current.byte_offset - base.byte_offset) % 16u != 0)
        throw SourceAllocationError(0x45e5fb, "effect color observation lacks actual attribute identity");
    const W end = (current.byte_offset - base.byte_offset) / 16u;
    auto &audit = w.effect_resources.colors.borrow_for(w.sprite_packets);
    if (end < 4 || end > audit.source_undefined.size())
        throw SourceAllocationError(0x45e5fb, "effect color observation leaves actual attribute extent");
    for (W i = end - 4; i < end; ++i)
        audit.source_undefined[i] = undefined;
}
bool source_sprite_full(const SourceSpritePacketStorage &pool)
{
    const auto &a = pool.current_headers.source_binding();
    const auto &b = pool.headers.source().source_binding();
    if (a.bank != b.bank || a.allocation != b.allocation || a.bytes != b.bytes || a.byte_offset < b.byte_offset ||
        a.platform.owner_before(b.platform) || b.platform.owner_before(a.platform) ||
        (a.byte_offset - b.byte_offset) % 8u != 0)
        throw SourceAllocationError(0x43459e, "effect sprite cursor requires its actual header allocation");
    return (a.byte_offset - b.byte_offset) / 8u >= 512u;
}
} // namespace
void SourceRaceViewStageUnavailable::vehicles_418180(RaceSessionPlatform &p)
{
    admit(p, 0x442500);
    throw SourceAllocationError(0x442500, "required actual418180 receiver is not bound");
}
void SourceRaceViewStageUnavailable::impact_trail_41f2f0(RaceSessionPlatform &p,
                                                         const contact::effects::ImpactParticle &, W, W, W, float,
                                                         float)
{
    admit(p, 0x4160e4);
    throw SourceAllocationError(0x4160e4, "required actual41F2F0 ring receiver is not bound");
}
void append_source_effect_particle_sprites_45e4f0(RaceSessionPlatform &platform)
{
    SGN_PROFILE_SCOPE("entities.effect_sprites_45e4f0");
    admit(platform, 0x45e4f0);
    auto &w = platform.state();
    if (w.effect_particles.active_signal == 0)
        return;                     // no camera, descriptor or sprite access
    auto sprite = initial_486538(); // one stack template for the complete64-slot loop
    for (const auto &p : w.effect_particles.particles)
    {
        if (p.active != 1 || p.render_enabled != 1)
            continue;
        sprite.texture = texture(w, p);
        if (p.flags & 4u)
        {
            const auto angle = sgn_source_effect_angle_45e4b3(&p.angle);
            sprite.up.z = 0; // 45E4CA, W retained from the local template/previous iteration
            const auto cosine = sample(w, angle + 2048u, 0x45e4d1);
            const auto sine = sample(w, angle, 0x45e4d4);
            sprite.up.x = cosine;
            sprite.up.y = sine;
        }
        else
            sprite.up = {0, 1, 0, 0}; // four literal DWORDs486580..58C
        const auto alpha = sgn_source_effect_alpha_45e5ad(&p.alpha);
        const auto flags = p.render_flags; // 45E5B5 before alpha publication
        sprite.alpha = static_cast<std::uint16_t>(alpha);
        const auto color = p.color;
        sprite.colors[0] = color;
        sprite.colors[1] = color;
        sprite.colors[2] = color;
        sprite.colors[3] = color;
        const auto x = p.position.x;
        sprite.flags = flags;
        sprite.position.x = x;
        sprite.position.y = p.position.y;
        sprite.position.z = p.position.z;
        const auto opaque = p.position.opaque, scale = std::bit_cast<W>(p.scale);
        sprite.position.w = std::bit_cast<float>(opaque);
        sprite.scale[2] = std::bit_cast<float>(scale); // 45E5EF
        sprite.scale[0] = std::bit_cast<float>(scale);
        sprite.scale[1] = std::bit_cast<float>(scale);
        // Actual source writer uses canonical pointers. It never updates the
        // inactive preview scalar cursors. All earlier particles remain if a
        // later reached descriptor/table/allocator access fails.
        // 43459E's full-pool return precedes even the camera owner borrow.
        if (source_sprite_full(w.sprite_packets))
            continue;
        // NEW HOST (R28): interpolation tag of this particle's sprite (read-only).
        host_presentation::EffectSpriteScope effect(w.sprite_packets, host_presentation::particle_effect(w, p));
        if (append_source_sprite_packet_434590(w.sprite_packets, sprite, camera(w, 0x45e5fb)))
            mark_colors(w, p.source_undefined[6]);
    }
}
void append_source_impact_particle_trails_416050(RaceSessionPlatform &platform, SourceRaceViewStageRequired &child)
{
    SGN_PROFILE_SCOPE("entities.impact_trails_416050");
    admit(platform, 0x416050);
    auto &w = platform.state();
    auto cursor = w.impact_draw_list.count; // one captured61B624, then localESI walks backwards
    if (cursor == contact::effects::ImpactParticleDrawList::null_value || cursor == 0)
        return;
    for (;;)
    {
        if (cursor > w.impact_draw_list.slots.size())
            throw SourceAllocationError(0x416067, "impact draw read leaves actual32-entry list");
        const auto slot = w.impact_draw_list.slots[cursor - 1u];
        --cursor;
        if (slot >= w.impact_particles.particles.size())
            throw SourceAllocationError(0x41606d, "impact draw pointer leaves actual particle bank");
        const auto &p = w.impact_particles.particles[slot];
        float scale;
        sgn_source_impact_scale_41606d(&p.lifetime, &p.scale, &scale);
        const auto ring_cursor = p.field_430; // 4160BE, before count/type
        const auto count = p.field_434;
        const auto type = std::bit_cast<W>(p.type);
        // Captures actual profile address arithmetic; no eager profile/ring
        // validation or64-point temporary. Receiver resolves the reached row.
        child.impact_trail_41f2f0(platform, p, type, ring_cursor, count, scale, scale);
        if (cursor == 0)
            return;
    }
}
void prepare_source_race_view_stage_442500(RaceSessionPlatform &platform, SourceRaceViewStageRequired &children)
{
    admit(platform, 0x442500);
    children.vehicles_418180(platform);    // 442500
    host_presentation::pass_yield_point(); // NEW HOST (R33): may present one interpolated frame, no source work
    append_source_effect_particle_sprites_45e4f0(platform); // 442505
    host_presentation::pass_yield_point(); // NEW HOST (R33): may present one interpolated frame, no source work
    append_source_impact_particle_trails_416050(platform, children); // 44250A
    // 44250F JMP449D60; literalRET. No extra camera, reset, physics or draw.
}
} // namespace sgn::retail
