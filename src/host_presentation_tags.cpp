#include "sgn/host_presentation.hpp"
#include "sgn/race_session.hpp"
#include "sgn/host_wide_race_view.hpp"
#include "sgn/scene_resources.hpp"
#include "sgn/model_identity.hpp"
#include <algorithm>
#include <cstring>

// NEW HOST presentation bookkeeping (interp, R19). Read-only observation of the
// canonical source race draw; see host_presentation.hpp. No source owner is
// written and no source function is called from here.
namespace sgn::host_presentation
{
namespace
{
struct Range
{
    std::size_t begin{}, end{};
    std::uint16_t group{};
};
// NEW HOST (R28): packets of the 434590 sprite model are resolved per sprite
// header in submitted_packet (internal marker, never stored in FrameTags).
constexpr std::uint16_t group_sprite_model = 0xfffcu;
constexpr std::size_t sprite_headers = 512; // SourceSpritePacketStorage::headers
struct Collector
{
    bool enabled{};
    bool suppressed{};
    unsigned race_flip_depth{};
    bool building{};
    Region region{Region::overlay2d};
    unsigned list{};
    std::unique_ptr<FrameTags> frame;
    std::vector<std::uint16_t> pending;
    std::array<std::vector<Range>, packet_lists> ranges;
    // Queue model identities of vehicles.models[k].model (k) and .shadow (6+k).
    std::array<std::uint32_t, 2 * vehicle_slots> identities{};
    std::array<bool, 2 * vehicle_slots> identity_valid{};
    RegionObserver region_observer{}; // NEW HOST (R28)
    // NEW HOST (R28) effect tags: sprite header index -> FrameTags::effects index.
    std::array<std::int32_t, sprite_headers> sprite_effects{};
    std::size_t sprite_mapped{}; // one past the highest mapped header
    const retail::SourceAllocationBank *bank{};
    retail::SourceAllocationRef sprite_allocation{};
    std::uint32_t sprite_offset{};
    std::optional<EffectTag> ring_owner;
    // NEW HOST (R30): sizes of the last completed frame; the next frame reserves
    // them once instead of growing its vectors by repeated reallocation inside
    // the source draw (contents unchanged, capacity only).
    std::size_t hint_draws{}, hint_groups{}, hint_effects{};
};
bool within(const void *element, const void *first, std::size_t count, std::size_t size) noexcept
{
    const auto at = reinterpret_cast<std::uintptr_t>(element), base = reinterpret_cast<std::uintptr_t>(first);
    return at >= base && at - base < count * size && (at - base) % size == 0;
}
Collector &collector() noexcept
{
    static Collector state; // owner (draw) thread only
    return state;
}
// Host bookkeeping failure: the frame is presented exactly as captured.
void spoil(Collector &c) noexcept
{
    c.pending.clear();
    if (c.frame)
        c.frame->consistent = false;
}
std::size_t list_offset(const retail::WorldPolygonProducerList &list)
{
    return list.current().offset().value_or(0);
}
std::array<double, 3> xyz(float x, float y, float z)
{
    return {double(x), double(y), double(z)};
}
// NEW HOST (R28): number of sprite headers the source appended so far (the same
// cursor difference 43459E tests); nullopt when not a source-bound pool.
std::optional<std::size_t> sprite_count(const retail::SourceSpritePacketStorage &pool) noexcept
{
    try
    {
        if (!pool.headers.source_bound() || !pool.current_headers.source_bound())
            return std::nullopt;
        const auto &a = pool.current_headers.source_binding();
        const auto &b = pool.headers.source().source_binding();
        if (!(a.allocation == b.allocation) || a.byte_offset < b.byte_offset || (a.byte_offset - b.byte_offset) % 8u)
            return std::nullopt;
        return std::size_t{(a.byte_offset - b.byte_offset) / 8u};
    }
    catch (...)
    {
        return std::nullopt;
    }
}
std::array<double, 3> point3(const void *p) noexcept
{
    float v[3];
    std::memcpy(v, p, sizeof v);
    return xyz(v[0], v[1], v[2]);
}
// Sprite header index of a packet's header token, if it is one of this frame's sprites.
std::optional<std::size_t> sprite_header(const Collector &c, std::uint32_t header) noexcept
{
    if (!c.bank || !header || !c.sprite_allocation)
        return std::nullopt;
    try
    {
        const auto location = c.bank->decode_address(header); // const lookup, no token is created
        if (!(location.allocation == c.sprite_allocation) || location.offset < c.sprite_offset ||
            (location.offset - c.sprite_offset) % 8u)
            return std::nullopt;
        const std::size_t index = (location.offset - c.sprite_offset) / 8u;
        if (index >= sprite_headers)
            return std::nullopt;
        return index;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

// NEW HOST (R28) effect producer hooks (see host_presentation.hpp EffectHooks).
class CollectorHooks final : public EffectHooks
{
  public:
    SpriteMark sprites_begin(const retail::SourceSpritePacketStorage &pool, const EffectTag &tag) noexcept override
    {
        auto &c = collector();
        if (!c.building)
            return {};
        const auto count = sprite_count(pool);
        if (!count)
            return {};
        if (*count < c.sprite_mapped)
        {
            // The cursor went back (434030 reset): older mappings name other sprites now.
            for (std::size_t i = *count; i < std::min(c.sprite_mapped, sprite_headers); ++i)
                c.sprite_effects[i] = -1;
            c.sprite_mapped = *count;
        }
        if (c.frame->effects.size() >= max_effects)
            return {};
        try
        {
            c.frame->effects.push_back(tag);
        }
        catch (...)
        {
            spoil(c);
            return {};
        }
        return {true, *count, static_cast<std::int32_t>(c.frame->effects.size() - 1)};
    }
    void sprites_end(const retail::SourceSpritePacketStorage &pool, const SpriteMark &mark) noexcept override
    {
        auto &c = collector();
        if (!c.building || !mark.active)
            return;
        const auto end = sprite_count(pool);
        if (!end || *end < mark.begin)
            return;
        for (std::size_t i = mark.begin; i < std::min(*end, sprite_headers); ++i)
            c.sprite_effects[i] = mark.effect;
        c.sprite_mapped = std::max(c.sprite_mapped, *end);
    }
    std::optional<EffectTag> exchange_ring_owner(const std::optional<EffectTag> &owner) noexcept override
    {
        auto &c = collector();
        auto previous = c.ring_owner;
        c.ring_owner = c.building ? owner : std::nullopt;
        return previous;
    }
    std::optional<EffectTag> impact_owner(const retail::RaceSessionState &s,
                                          const retail::contact::effects::ImpactParticle &particle) noexcept override
    {
        const auto &bank = s.impact_particles.particles;
        if (!collector().building || !within(&particle, bank.data(), bank.size(), sizeof particle))
            return std::nullopt;
        EffectTag tag;
        tag.owner = EffectOwner::impact_particle;
        tag.slot = static_cast<std::uint32_t>(&particle - bank.data());
        tag.generation = static_cast<std::uint32_t>(particle.initial_lifetime);
        tag.position = xyz(particle.position.x, particle.position.y, particle.position.z);
        tag.velocity = xyz(particle.velocity.x, particle.velocity.y, particle.velocity.z);
        return tag;
    }
    std::optional<EffectTag> ring_head() noexcept override
    {
        const auto &c = collector();
        if (!c.building || !c.ring_owner)
            return std::nullopt;
        auto tag = *c.ring_owner;
        tag.kind = EffectKind::attached;
        return tag;
    }
    std::optional<EffectTag> ring_segment(std::uint32_t segment, const void *newest_point,
                                          const void *older_point) noexcept override
    {
        const auto &c = collector();
        // Only the newest quad reaches the moving owner; older quads are world-fixed.
        if (!c.building || !c.ring_owner || segment != 0 || !newest_point || !older_point)
            return std::nullopt;
        auto tag = *c.ring_owner;
        tag.kind = EffectKind::trail_segment;
        tag.newest = point3(newest_point);
        tag.older = point3(older_point);
        return tag;
    }
    std::optional<EffectTag> particle(const retail::RaceSessionState &s,
                                      const retail::contact::effects::EffectParticle &particle) noexcept override
    {
        const auto &bank = s.effect_particles.particles;
        if (!collector().building || !within(&particle, bank.data(), bank.size(), sizeof particle))
            return std::nullopt;
        EffectTag tag;
        tag.kind = EffectKind::attached;
        tag.owner = EffectOwner::effect_particle;
        tag.slot = static_cast<std::uint32_t>(&particle - bank.data());
        tag.generation = particle.lifetime;
        tag.position = xyz(particle.position.x, particle.position.y, particle.position.z);
        tag.velocity = xyz(particle.velocity.x, particle.velocity.y, particle.velocity.z);
        return tag;
    }
};
CollectorHooks &collector_hooks() noexcept
{
    static CollectorHooks hooks;
    return hooks;
}
} // namespace

void set_collection_enabled(bool enabled) noexcept
{
    auto &c = collector();
    c.enabled = enabled;
    effect_hooks = enabled ? &collector_hooks() : nullptr; // NEW HOST (R28)
    if (!enabled)
    {
        c.ring_owner.reset();
        c.building = false;
        c.frame.reset();
        c.pending.clear();
        c.suppressed = false;
    }
}
bool collection_enabled() noexcept
{
    return collector().enabled;
}
void set_race_present_suppressed(bool suppressed) noexcept
{
    collector().suppressed = suppressed;
}
bool suppress_current_present() noexcept
{
    const auto &c = collector();
    return c.enabled && c.suppressed && c.race_flip_depth != 0;
}
bool race_present_suppression_armed() noexcept
{
    const auto &c = collector();
    return c.enabled && c.suppressed;
}
RaceFlipScope::RaceFlipScope() noexcept
{
    ++collector().race_flip_depth;
}
RaceFlipScope::~RaceFlipScope()
{
    --collector().race_flip_depth;
}

void begin_frame() noexcept
{
    auto &c = collector();
    c.building = false;
    c.pending.clear();
    c.frame.reset();
    if (!c.enabled)
        return;
    try
    {
        c.frame = std::make_unique<FrameTags>();
        c.frame->draws.reserve(c.hint_draws + c.hint_draws / 8);
        c.frame->groups.reserve(c.hint_groups + c.hint_groups / 8);
        c.frame->effects.reserve(c.hint_effects + c.hint_effects / 8);
    }
    catch (...)
    {
        c.frame.reset();
        return;
    } // not building: canonical presentation
    for (auto &ranges : c.ranges)
        ranges.clear();
    c.region = Region::overlay2d;
    c.list = 0;
    c.building = true;
}
void note_draw(std::size_t vertices, bool accepted) noexcept
{
    auto &c = collector();
    if (!c.building)
        return;
    auto &f = *c.frame;
    try
    {
        if (!c.pending.empty())
        {
            if (accepted)
            {
                if (c.pending.size() != vertices)
                    f.consistent = false;
                DrawTags draw;
                draw.vertices = static_cast<std::uint32_t>(vertices);
                draw.per_vertex = true;
                draw.first = static_cast<std::uint32_t>(f.groups.size());
                f.groups.insert(f.groups.end(), c.pending.begin(), c.pending.end());
                f.draws.push_back(draw);
            }
            c.pending.clear();
            return;
        }
        if (!accepted)
            return; // a refused draw appends no capture command
        DrawTags draw;
        draw.vertices = static_cast<std::uint32_t>(vertices);
        draw.uniform = c.region == Region::overlay2d ? group_overlay : group_static;
        f.draws.push_back(draw);
    }
    catch (...)
    {
        spoil(c);
    }
}
ImmutableFrameTags end_frame() noexcept
{
    auto &c = collector();
    if (!c.building)
        return {};
    c.building = false;
    if (!c.pending.empty())
        c.frame->consistent = false; // tags without a draw
    c.pending.clear();
    c.hint_draws = c.frame->draws.size();
    c.hint_groups = c.frame->groups.size();
    c.hint_effects = c.frame->effects.size();
    try
    {
        return ImmutableFrameTags(std::move(c.frame));
    }
    catch (...)
    {
        c.frame.reset();
        return {};
    } // no tags: presented exactly as captured
}
void abandon_frame() noexcept
{
    auto &c = collector();
    c.building = false;
    c.frame.reset();
    c.pending.clear();
}

void begin_race_draw(retail::RaceSessionPlatform &platform) noexcept
{
    auto &c = collector();
    if (!c.building)
        return;
    try
    {
        auto &s = platform.state();
        c.frame->race_view = true;
        c.frame->simulation_tick = s.simulation_tick;
        // NEW HOST (R28): sprite headers of this draw (read-only metadata).
        c.sprite_effects.fill(-1);
        c.sprite_mapped = 0;
        c.ring_owner.reset();
        c.bank = nullptr;
        c.sprite_allocation = {};
        if (s.sprite_packets.headers.source_bound())
        {
            const auto &headers = s.sprite_packets.headers.source().source_binding();
            c.bank = headers.bank;
            c.sprite_allocation = headers.allocation;
            c.sprite_offset = headers.byte_offset;
        }
        const auto &models = platform.source_scene_resources().models();
        for (unsigned k = 0; k < vehicle_slots; ++k)
        {
            const auto &storage = s.vehicles.models[k];
            const bool active = s.vehicles.race[k].active == 1; // 418180 draws only active slots
            c.identity_valid[k] = active && storage.model.has_value();
            c.identities[k] = c.identity_valid[k] ? models.identity(storage.model) : 0u;
            c.identity_valid[vehicle_slots + k] = active && storage.shadow.has_value();
            c.identities[vehicle_slots + k] =
                c.identity_valid[vehicle_slots + k] ? models.identity(storage.shadow) : 0u;
        }
        // A model shared by two slots cannot name one rigid pose: present canonically.
        for (unsigned i = 0; i < 2 * vehicle_slots; ++i)
            for (unsigned j = i + 1; j < 2 * vehicle_slots; ++j)
                if (c.identity_valid[i] && c.identity_valid[j] && c.identities[i] == c.identities[j] &&
                    i % vehicle_slots != j % vehicle_slots)
                    spoil(c);
    }
    catch (...)
    {
        c.identity_valid.fill(false);
        spoil(c);
    }
}
void note_world_view(const retail::RaceSessionState &s) noexcept
{
    auto &c = collector();
    if (!c.building)
        return;
    auto &f = *c.frame;
    ++f.world_views;
    const auto i = s.controls.current;
    if (i >= s.controls.controllers.size() || i >= s.camera_lifecycle.size())
    {
        f.view.valid = false;
        return;
    }
    const auto &controller = s.controls.controllers[i];
    const auto &camera = controller.camera;
    auto &v = f.view;
    v.controller = i;
    v.camera_mode = camera.mode;
    v.controller_vehicle = controller.vehicle;
    v.camera_position = xyz(camera.position.x, camera.position.y, camera.position.z);
    const auto &m = camera.world_to_camera_rotation;
    for (unsigned r = 0; r < 3; ++r)
        for (unsigned col = 0; col < 3; ++col)
            v.world_to_camera[r * 3 + col] = double(m[r * 4 + col]);
    v.camera_translation = xyz(m[12], m[13], m[14]);
    v.projection_x = double(s.frame_projection[0]);
    v.projection_y = double(s.frame_projection[5]);
    v.clip_near = double(s.camera_lifecycle[i].clip_near);
    if (const auto *wide = host_wide::race_view())
    {
        v.wide_factor_x = wide->factor_x;
        v.wide_factor_y = wide->factor_y;
    } // NEW HOST (R20)
    else
    {
        v.wide_factor_x = 1;
        v.wide_factor_y = 1;
    }
    v.valid = true;
    for (unsigned k = 0; k < vehicle_slots; ++k)
    {
        auto &out = f.vehicles[k];
        const auto &body = s.vehicles.physics[k];
        out.active = s.vehicles.race[k].active == 1;
        out.position = xyz(body.position.x, body.position.y, body.position.z);
        out.up = xyz(body.up.x, body.up.y, body.up.z);
        out.forward = xyz(body.basis_z.x, body.basis_z.y, body.basis_z.z);
    }
}
void set_region_observer(RegionObserver observer) noexcept
{
    collector().region_observer = observer;
}
RegionScope::RegionScope(Region region) noexcept : previous_(collector().region)
{
    auto &c = collector();
    c.region = region;
    if (c.region_observer)
        c.region_observer(previous_, region); // NEW HOST (R28) frame phase marker
}
RegionScope::~RegionScope()
{
    auto &c = collector();
    if (c.region_observer)
        c.region_observer(c.region, previous_); // NEW HOST (R28) frame phase marker
    c.region = previous_;
}

std::uint16_t model_group(std::uint32_t identity) noexcept
{
    const auto &c = collector();
    for (unsigned k = 0; k < 2 * vehicle_slots; ++k)
        if (c.identity_valid[k] && c.identities[k] == identity)
            return static_cast<std::uint16_t>(k < vehicle_slots ? group_first_vehicle + k
                                                                : group_first_shadow + (k - vehicle_slots));
    return group_static;
}
ModelPacketScope::ModelPacketScope(const retail::WorldPolygonFrame &frame, std::uint32_t identity) noexcept
{
    auto &c = collector();
    if (!c.building)
        return;
    group_ = identity == retail::sprite_model_identity ? group_sprite_model : model_group(identity); // R28
    try
    {
        for (unsigned k = 0; k < packet_lists; ++k)
            begin_[k] = list_offset(frame.packets[k]);
    }
    catch (...)
    {
        spoil(c);
        return;
    }
    frame_ = &frame;
}
ModelPacketScope::~ModelPacketScope()
{
    auto &c = collector();
    if (!frame_ || !c.building)
        return;
    try
    {
        for (unsigned k = 0; k < packet_lists; ++k)
        {
            const auto end = list_offset(frame_->packets[k]);
            if (end > begin_[k])
                c.ranges[k].push_back({begin_[k], end, group_});
        }
        c.frame->model_hooks = true;
    }
    catch (...)
    {
        spoil(c);
    }
}
SubmissionListScope::SubmissionListScope(unsigned list) noexcept : previous_(collector().list)
{
    collector().list = list;
}
SubmissionListScope::~SubmissionListScope()
{
    collector().list = previous_;
}
void submitted_packet(std::size_t ordinal, bool sky, std::uint32_t corners, std::uint32_t header) noexcept
{
    auto &c = collector();
    if (!c.building)
        return;
    c.frame->packet_hooks = true;
    std::uint16_t group = group_static;
    if (sky)
        group = group_sky;
    else if (c.list < packet_lists)
    {
        // Producers append in increasing list offsets: ranges are sorted by begin.
        const auto &ranges = c.ranges[c.list];
        auto it = std::upper_bound(ranges.begin(), ranges.end(), ordinal,
                                   [](std::size_t value, const Range &range) { return value < range.begin; });
        if (it != ranges.begin())
        {
            --it;
            if (ordinal < it->end)
                group = it->group;
        }
    }
    if (group == group_sprite_model)
    {
        // NEW HOST (R28): the sprite this packet was emitted from (429E.. stores
        // the header token as packet word 0), or world-fixed when untagged.
        group = group_static;
        if (const auto index = sprite_header(c, header); index && c.sprite_effects[*index] >= 0)
            group = static_cast<std::uint16_t>(group_first_effect + c.sprite_effects[*index]);
    }
    try
    {
        c.pending.insert(c.pending.end(), corners, group);
    }
    catch (...)
    {
        spoil(c);
    }
}
} // namespace sgn::host_presentation
