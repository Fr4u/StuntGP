#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace sgn::retail
{
struct RaceSessionState;
struct WorldPolygonFrame;
class RaceSessionPlatform;
struct SourceSpritePacketStorage;
namespace contact::effects
{
struct EffectParticle;
struct ImpactParticle;
} // namespace contact::effects
} // namespace sgn::retail

// NEW HOST presentation bookkeeping (interp, R19). This is not recovered game
// behaviour. It observes the canonical source race draw (4427B0) read-only and
// records, per captured draw, which vertices belong to which rigid object so
// that the host can present interpolated frames between 60 Hz ticks by
// re-projecting the canonical capture (see host_presentation_presenter.hpp).
// Nothing here writes a source owner, and no source function is called.
// All functions are owner-thread only (the single source draw thread). While
// collection is disabled every entry point returns after one flag test. Hooks
// are noexcept: a host bookkeeping failure (allocation) never reaches the
// source draw; it only marks that frame as not interpolatable (it is then
// presented exactly as captured).
namespace sgn::host_presentation
{

// Per captured vertex group ids.
inline constexpr std::uint16_t group_static = 0;        // world geometry (camera re-projection only)
inline constexpr std::uint16_t group_first_vehicle = 1; // 1..6 = vehicle slot+1 (rigid vehicle pose)
inline constexpr std::uint16_t group_sky = 0xfffdu;     // camera-anchored (rotation only)
inline constexpr std::uint16_t group_overlay = 0xffffu; // 2D, presented exactly as captured
inline constexpr unsigned vehicle_slots = 6;
inline constexpr unsigned packet_lists = 4; // WorldPolygonFrame::packets
// NEW HOST (R28 smooth effects). With Settings::smooth_effects off the presenter
// maps these groups exactly like R27 did (shadow = its vehicle, effects = static).
inline constexpr std::uint16_t group_first_shadow = 7;       // 7..12 = vehicle slot+7 blob shadow model
inline constexpr std::uint16_t group_first_effect = 0x1000u; // + index into FrameTags::effects
inline constexpr std::size_t max_effects = 0xe000u;          // keeps effect groups below group_sky

enum class Region : std::uint8_t
{
    overlay2d,
    world3d,
    screen3d
};

// Plain values read from the actual owners inside the canonical draw.
struct ViewSnapshot
{
    bool valid{};
    std::uint32_t controller{}, camera_mode{}, controller_vehicle{};
    std::array<double, 3> camera_position{};    // CameraState.position (world)
    std::array<double, 9> world_to_camera{};    // CameraState.world_to_camera_rotation rows 0..2, cols 0..2
    std::array<double, 3> camera_translation{}; // same matrix, row 3 (applied after the rotation)
    double projection_x{}, projection_y{};      // frame_projection[0]/[5] (61C3E8/61C3FC)
    double clip_near{};                         // CameraLifecycle.clip_near of the drawn controller
    // NEW HOST (R20 wide_3d): Hor+ factors active when this view was drawn. The
    // projection words above are then P/k and 429A40 used the widened view
    // (host_wide::widened_view(factor_x,factor_y,draw viewport)). 1 = original.
    float wide_factor_x{1}, wide_factor_y{1};
};
struct VehicleSnapshot
{
    bool active{};
    std::array<double, 3> position{}, up{}, forward{}; // VehicleState position/up/basis_z
};
struct DrawTags
{
    std::uint32_t vertices{};
    std::uint16_t uniform{group_overlay}; // used when per_vertex is false
    bool per_vertex{};
    std::uint32_t first{}; // index into FrameTags::groups when per_vertex
};
// NEW HOST (R28): one source effect sprite (434590 particle/glow, 4342E0 trail
// quad, 4343F0 trail billboard) and the owner that moves it between ticks.
enum class EffectKind : std::uint8_t
{
    attached,      // moves rigidly with its owner (vehicle glow, trail head billboard)
    trail_segment, // newest trail quad: weight 1 at `newest` (owner), 0 at `older` (world-fixed)
};
enum class EffectOwner : std::uint8_t
{
    vehicle,         // VehicleState slot `slot` (interpolated rigid pose)
    effect_particle, // EffectParticleBank slot (45E4F0), position interpolated N-1 -> N
    impact_particle, // ImpactParticles slot (416050 rings), position interpolated N-1 -> N
};
struct EffectTag
{
    EffectKind kind{};
    EffectOwner owner{};
    std::uint32_t slot{};
    // Particle continuity: the same particle in two captures has the same slot and
    // generation (its spawn lifetime) and moved at most about its own velocity.
    std::uint32_t generation{};
    std::array<double, 3> position{}, velocity{}; // particle owner: world position, velocity per tick
    std::array<double, 3> newest{}, older{};      // trail_segment: the two ring points (world)
};
struct FrameTags
{
    bool race_view{};       // begun by the canonical 4427B0 race draw
    bool consistent{true};  // every accepted draw has exactly its vertex count of tags
    bool packet_hooks{};    // 433F40 packet tagging was observed (vehicle groups available)
    bool model_hooks{};     // 42AF30/42AF90 model tagging was observed
    unsigned world_views{}; // 4425D0 world submissions (split screen draws more than one)
    std::uint32_t simulation_tick{};
    ViewSnapshot view;
    std::array<VehicleSnapshot, vehicle_slots> vehicles{};
    std::vector<DrawTags> draws; // accepted capture draws, in capture order
    std::vector<std::uint16_t> groups;
    std::vector<EffectTag> effects; // NEW HOST (R28), indexed by group - group_first_effect
};
using ImmutableFrameTags = std::shared_ptr<const FrameTags>;

// Host switches (presenter).
void set_collection_enabled(bool) noexcept;
bool collection_enabled() noexcept;
// Present suppression applies only to the 432A60 Flip inside a canonical race draw
// (4427C9); loading/menu Flips are never suppressed.
void set_race_present_suppressed(bool) noexcept;
bool suppress_current_present() noexcept;
// NEW HOST (R22): the presenter will suppress the next canonical race Present (it is
// active). The backend may then defer rasterizing that race frame's image.
bool race_present_suppression_armed() noexcept;
class RaceFlipScope
{
  public:
    RaceFlipScope() noexcept;
    ~RaceFlipScope();
    RaceFlipScope(const RaceFlipScope &) = delete;
    RaceFlipScope &operator=(const RaceFlipScope &) = delete;
};

// NEW HOST (R31) cooperative presentation inside the source race pass. The race
// draw (source_core_race_draw.cpp) calls pass_yield_point() between its stages
// (after 442810 has returned, between the world stages 442500 / 42B040 / 4277D0 /
// 429A40 / 433F40, after the HUD and before EndScene). The installed hook (the
// R18 pacing adapter) may present one interpolated frame from the presenter's
// immutable history there. A yield does no source work: no source function
// runs and no source owner is written; with no hook installed (tests, oracles,
// interpolation off) it is a single null test.
using PassYield = void (*)(void *owner) noexcept;
struct PassYieldHook
{
    PassYield yield{};
    void *owner{};
};
inline PassYieldHook pass_yield_hook{}; // owner (draw) thread only
inline void set_pass_yield(PassYield yield, void *owner) noexcept
{
    pass_yield_hook = {yield, owner};
}
inline void pass_yield_point() noexcept
{
    if (pass_yield_hook.yield)
        pass_yield_hook.yield(pass_yield_hook.owner);
}

// Presentation backend (BeginScene/EndScene/draws of every source frame).
void begin_frame() noexcept;
void note_draw(std::size_t vertices, bool accepted) noexcept;
ImmutableFrameTags end_frame() noexcept;
void abandon_frame() noexcept;

// Canonical race draw (source_core_race_draw.cpp).
void begin_race_draw(retail::RaceSessionPlatform &) noexcept;    // after 433EE0 BeginScene
void note_world_view(const retail::RaceSessionState &) noexcept; // 44260B, before 433F40
// NEW HOST (R28): observer of every RegionScope transition (host_frame_phase
// markers). Called on entry (previous, new) and on exit (current, restored),
// independent of tag collection. nullptr (default) disables it.
using RegionObserver = void (*)(Region from, Region to) noexcept;
void set_region_observer(RegionObserver) noexcept;
class RegionScope
{
    Region previous_;

  public:
    explicit RegionScope(Region) noexcept;
    ~RegionScope();
    RegionScope(const RegionScope &) = delete;
    RegionScope &operator=(const RegionScope &) = delete;
};

// World producers (perf_world): around one queued model's produce(...) in 42AF30/42AF90.
class ModelPacketScope
{
    std::array<std::size_t, packet_lists> begin_{};
    const retail::WorldPolygonFrame *frame_{};
    std::uint16_t group_{group_static};

  public:
    ModelPacketScope(const retail::WorldPolygonFrame &, std::uint32_t model_identity) noexcept;
    ~ModelPacketScope();
    ModelPacketScope(const ModelPacketScope &) = delete;
    ModelPacketScope &operator=(const ModelPacketScope &) = delete;
};
// World submission (perf_world, 433F40): the packet list being expanded, then one
// call per packet whose `corners` vertices were appended to the current batch.
class SubmissionListScope
{
    unsigned previous_;

  public:
    explicit SubmissionListScope(unsigned list) noexcept;
    ~SubmissionListScope();
    SubmissionListScope(const SubmissionListScope &) = delete;
    SubmissionListScope &operator=(const SubmissionListScope &) = delete;
};
// NEW HOST (R28): `header` is the packet's header token (433F40 word 0). Packets of
// the 434590 sprite model are resolved per sprite header to their effect tag.
void submitted_packet(std::size_t ordinal, bool sky, std::uint32_t corners, std::uint32_t header = 0) noexcept;

// NEW HOST (R28) effect producers (entities 442500, read-only). They live in
// race_session, which cannot link host_presentation (it links race_session), so
// the producers reach the collector only through this interface, installed by
// set_collection_enabled(true) and removed by set_collection_enabled(false)
// (between source frames). With no hooks installed every call below is a
// single null test.
struct SpriteMark
{
    bool active{};
    std::size_t begin{};
    std::int32_t effect{-1};
};
class EffectHooks
{
  public:
    virtual SpriteMark sprites_begin(const retail::SourceSpritePacketStorage &, const EffectTag &) noexcept = 0;
    virtual void sprites_end(const retail::SourceSpritePacketStorage &, const SpriteMark &) noexcept = 0;
    virtual std::optional<EffectTag> exchange_ring_owner(const std::optional<EffectTag> &) noexcept = 0;
    virtual std::optional<EffectTag> impact_owner(const retail::RaceSessionState &,
                                                  const retail::contact::effects::ImpactParticle &) noexcept = 0;
    virtual std::optional<EffectTag> ring_head() noexcept = 0;
    virtual std::optional<EffectTag> ring_segment(std::uint32_t segment, const void *newest_point,
                                                  const void *older_point) noexcept = 0;
    virtual std::optional<EffectTag> particle(const retail::RaceSessionState &,
                                              const retail::contact::effects::EffectParticle &) noexcept = 0;

  protected:
    ~EffectHooks() = default;
};
inline EffectHooks *effect_hooks = nullptr; // owner (draw) thread only

// Maps every sprite header the source appends while alive to one effect tag.
class EffectSpriteScope
{
    const retail::SourceSpritePacketStorage *pool_{};
    SpriteMark mark_{};

  public:
    EffectSpriteScope(const retail::SourceSpritePacketStorage &pool, const std::optional<EffectTag> &tag) noexcept
    {
        if (effect_hooks && tag)
        {
            mark_ = effect_hooks->sprites_begin(pool, *tag);
            pool_ = &pool;
        }
    }
    ~EffectSpriteScope()
    {
        if (effect_hooks && pool_ && mark_.active)
            effect_hooks->sprites_end(*pool_, mark_);
    }
    EffectSpriteScope(const EffectSpriteScope &) = delete;
    EffectSpriteScope &operator=(const EffectSpriteScope &) = delete;
};
// Owner of the 41F2F0 ring being rendered (vehicle ring or impact particle ring).
class RingOwnerScope
{
    bool installed_{};
    std::optional<EffectTag> previous_;

    void install(const std::optional<EffectTag> &owner) noexcept
    {
        if (!effect_hooks)
            return;
        previous_ = effect_hooks->exchange_ring_owner(owner);
        installed_ = true;
    }

  public:
    explicit RingOwnerScope(unsigned vehicle_slot) noexcept
    {
        if (!effect_hooks || vehicle_slot >= vehicle_slots)
            return;
        EffectTag owner;
        owner.owner = EffectOwner::vehicle;
        owner.slot = vehicle_slot;
        install(owner);
    }
    RingOwnerScope(const retail::RaceSessionState &s, const retail::contact::effects::ImpactParticle &p) noexcept
    {
        if (effect_hooks)
            install(effect_hooks->impact_owner(s, p));
    }
    ~RingOwnerScope()
    {
        if (installed_ && effect_hooks)
            (void)effect_hooks->exchange_ring_owner(previous_);
    }
    RingOwnerScope(const RingOwnerScope &) = delete;
    RingOwnerScope &operator=(const RingOwnerScope &) = delete;
};
// Tags for EffectSpriteScope; empty when there is nothing to move (no ring
// owner, a segment other than the newest, collection off).
inline std::optional<EffectTag> ring_head_effect() noexcept
{
    return effect_hooks ? effect_hooks->ring_head() : std::nullopt;
}
inline std::optional<EffectTag> ring_segment_effect(std::uint32_t segment, const void *newest_point,
                                                    const void *older_point) noexcept // 3 floats each (world)
{
    return effect_hooks ? effect_hooks->ring_segment(segment, newest_point, older_point) : std::nullopt;
}
inline std::optional<EffectTag> particle_effect(const retail::RaceSessionState &s,
                                                const retail::contact::effects::EffectParticle &p) noexcept
{
    return effect_hooks ? effect_hooks->particle(s, p) : std::nullopt;
}
inline std::optional<EffectTag> attached_effect(unsigned vehicle_slot) noexcept
{
    if (!effect_hooks || vehicle_slot >= vehicle_slots)
        return std::nullopt;
    EffectTag tag;
    tag.kind = EffectKind::attached;
    tag.owner = EffectOwner::vehicle;
    tag.slot = vehicle_slot;
    return tag;
}

// Group of a queued model identity (vehicle body -> slot+1, NEW HOST (R28) its shadow
// -> slot+7, else static).
std::uint16_t model_group(std::uint32_t model_identity) noexcept;
} // namespace sgn::host_presentation
