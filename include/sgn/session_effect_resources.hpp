#pragma once
#include "sgn/effect_particle_lifetime.hpp"
#include "sgn/effect_particle_sprites.hpp"
#include "sgn/retail_material_storage.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <span>

namespace sgn::retail
{
class RaceSession;
struct SessionWorldFrameResources;
struct WorldModelDrawState;

// Sole source61C608..61C7C7 words,61C7C8 cursor and577658 flag. These are
// texture-frame descriptors, unrelated to PMD texture UV motion at624488.
// NEW bounds and projection-cache lifetime checks; no source callback is
// inferred from the existence of recovered entry425A70.
class EffectAnimationStorage final
{
    std::array<std::array<std::uint32_t, 7>, 16> words_{}; // pinned PE BSS
    std::uint32_t cursor_{};
    bool cursor_active_{};
    std::uint32_t flag_577658_{}; // pinned PE0; scene/loading resets retain it
    // Host metadata: source-authored rows stay available after cursor reset.
    // Untouched zero descriptors are not promoted to invented animations.
    std::uint32_t authored_count_{};
    std::array<contact::effects::EffectAnimationTexture, 16> textures_{};
    std::array<contact::effects::EffectAnimationFrames, 16> frames_{};
    void refresh_projection(std::uint32_t slot) noexcept;

  public:
    static constexpr std::uint32_t source_base = 0x61c608u;
    static constexpr std::uint32_t capacity = 16;
    EffectAnimationStorage() = default;
    EffectAnimationStorage(const EffectAnimationStorage &) = delete;
    EffectAnimationStorage &operator=(const EffectAnimationStorage &) = delete;
    EffectAnimationStorage(EffectAnimationStorage &&) = delete;
    EffectAnimationStorage &operator=(EffectAnimationStorage &&) = delete;
    // Called ONLY with the corresponding real source resource event.
    void clear_scene_42e510() noexcept;          // 448B zero, cursor=null; flag retained
    void reset_loading_cursor_42c480() noexcept; // cursor=base; rows/flag retained
    // Exact recovered425A70 body for inputs ECX/EDX and current6243B8.
    // Export has no incoming call/data references: do not call from a loader,
    // particle emitter or frontend until that source invocation is recovered.
    // Returns no fabricated particle binding; source EAX ends at NEXT cursor.
    void recovered_entry_425a70(const PhysicalMaterialStorage &, std::uint32_t ecx, std::uint32_t edx);
    std::uint32_t source_cursor() const noexcept
    {
        return cursor_active_ ? source_base + 28u * cursor_ : 0u;
    }
    std::uint32_t source_flag_577658() const noexcept
    {
        return flag_577658_;
    }
    std::span<const std::uint32_t, 7> source_record(std::uint32_t slot) const &;
    std::span<const std::uint32_t, 7> source_record(std::uint32_t) const && = delete;
    // Read-only derived views. No independent animation state or phase.
    // Remain allocated through this owner; republish BETWEEN child calls
    // after any source clear/reset/write (old span sizes are not live counts).
    std::span<const contact::effects::EffectAnimationTexture> textures() const & noexcept
    {
        return {textures_.data(), authored_count_};
    }
    std::span<const contact::effects::EffectAnimationFrames> frames() const & noexcept
    {
        return {frames_.data(), authored_count_};
    }
    std::span<const contact::effects::EffectAnimationTexture> textures() const && = delete;
    std::span<const contact::effects::EffectAnimationFrames> frames() const && = delete;
};

// The known extra read is4945D8 itself: its DWORD0048C5D8 is also the source
// cosine-base pointer. Expose the RAW bits as float; do not dereference a
// native pointer, generate sin(2pi), clamp, modulo or substitute zero.
// Fixed-base pinned EXE has stripped relocations and no relocation directory.
class EffectAdjacentAngleStorage final
{
    const float word_4945d8_{std::bit_cast<float>(std::uint32_t{0x0048c5d8})};
    const std::array<contact::effects::EffectAngleSample, 1> samples_{{{10240u, word_4945d8_}}};

  public:
    EffectAdjacentAngleStorage() = default;
    EffectAdjacentAngleStorage(const EffectAdjacentAngleStorage &) = delete;
    EffectAdjacentAngleStorage &operator=(const EffectAdjacentAngleStorage &) = delete;
    EffectAdjacentAngleStorage(EffectAdjacentAngleStorage &&) = delete;
    EffectAdjacentAngleStorage &operator=(EffectAdjacentAngleStorage &&) = delete;
    std::span<const contact::effects::EffectAngleSample> samples() const & noexcept
    {
        return samples_;
    }
    std::span<const contact::effects::EffectAngleSample> samples() const && = delete;
};

// NEW diagnostic owner, one per actual sprite allocation lifetime. Declaration
// belongs immediately after RaceSessionState::sprite_packets. Constructor is
// for that fresh allocation, never for attaching to historical packet bytes.
class EffectSpriteColorStorage final
{
    SourceSpritePacketStorage &packets_;
    contact::effects::EffectSpriteColorAudit audit_{};

  public:
    explicit EffectSpriteColorStorage(SourceSpritePacketStorage &packets) : packets_(packets) {}
    EffectSpriteColorStorage(const EffectSpriteColorStorage &) = delete;
    EffectSpriteColorStorage &operator=(const EffectSpriteColorStorage &) = delete;
    EffectSpriteColorStorage(EffectSpriteColorStorage &&) = delete;
    EffectSpriteColorStorage &operator=(EffectSpriteColorStorage &&) = delete;
    // Actual full434090 allocation initialization, followed by fresh audit.
    // There is deliberately no cursor-reset hook:434030 retains old tags.
    void initialize_allocation_434090(WorldModelDrawState &);
    void initialize_allocation_434090(WorldModelDrawState &, class SceneResources &);
    contact::effects::EffectSpriteColorAudit &borrow_for(SourceSpritePacketStorage &) &;
    contact::effects::EffectSpriteColorAudit &borrow_for(SourceSpritePacketStorage &) && = delete;
};

// Own this alongside the packets in persistent RaceSessionPlatform state,
// not in SessionOuterFrameServices, a per-level material wrapper or a frame.
class SessionEffectResourceStorage final
{
  public:
    EffectAnimationStorage animations;
    EffectAdjacentAngleStorage angles;
    EffectSpriteColorStorage colors;
    explicit SessionEffectResourceStorage(SourceSpritePacketStorage &packets) : colors(packets) {}
    SessionEffectResourceStorage(const SessionEffectResourceStorage &) = delete;
    SessionEffectResourceStorage &operator=(const SessionEffectResourceStorage &) = delete;
    SessionEffectResourceStorage(SessionEffectResourceStorage &&) = delete;
    SessionEffectResourceStorage &operator=(SessionEffectResourceStorage &&) = delete;
    void publish(SourceSpritePacketStorage &, SessionWorldFrameResources &) &;
    void publish(SourceSpritePacketStorage &, SessionWorldFrameResources &) && = delete;
};
// Requires the integration patch's real persistent state member. Publishes
// only effects resources. Gamma, geometry audit and submission budget retain
// their independent real owners. Call between synchronous world children.
void publish_session_effect_resources(RaceSession &, SessionWorldFrameResources &);
std::span<const contact::effects::EffectAnimationFrames> borrow_session_effect_animation_frames(RaceSession &);
} // namespace sgn::retail
