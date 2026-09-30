#include "sgn/world_packet_join.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <type_traits>

namespace sgn::diagnostics
{
namespace
{
constexpr auto absent = (std::numeric_limits<std::size_t>::max)();
template <class A, class B> bool same_owner(const A &a, const B &b) noexcept
{
    return !a.owner_before(b) && !b.owner_before(a);
}
bool valid(const WorldJoinRange &r, const WorldJoinFrameView &f) noexcept
{
    return r.pool < 4 && ((!r.first && r.count == 0) || (r.first && *r.first <= f.pools[r.pool].size() &&
                                                         r.count <= f.pools[r.pool].size() - *r.first));
}
} // namespace
WorldJoinViewport world_join_viewport(const retail::ViewportState &v) noexcept
{
    static_assert(sizeof(v) == sizeof(WorldJoinViewport) && std::is_trivially_copyable_v<retail::ViewportState>);
    WorldJoinViewport out;
    std::memcpy(out.data(), &v, sizeof(v));
    return out;
}
WorldPacketJoinCapture::WorldPacketJoinCapture(bool enabled, WorldJoinLimits limits) noexcept
    : limits_(limits), enabled_(enabled), thread_(std::this_thread::get_id())
{
    if (!enabled_)
        return;
    try
    {
        result_.origins.reserve(limits.origins);
        origin_headers_.reserve(limits.origins);
        result_.occurrences.reserve(limits.occurrences);
        result_.memberships.reserve(limits.memberships);
        result_.invocations.reserve(limits.views);
        segments_.reserve(limits.segments);
        result_.owners.reserve(limits.owners);
    }
    catch (...)
    {
        enabled_ = false;
        fail(WorldJoinError::allocation);
    }
}
bool WorldPacketJoinCapture::fail(WorldJoinError e, WorldJoinStatus s) noexcept
{
    result_.status = s;
    result_.error = e;
    return false;
}
bool WorldPacketJoinCapture::active() noexcept
{
    if (!enabled_ || !started_ || finished_ || result_.status != WorldJoinStatus::collecting)
        return false;
    if (thread_ != std::this_thread::get_id())
        return fail(WorldJoinError::foreign_thread);
    return true;
}
bool WorldPacketJoinCapture::begin(std::uint64_t serial, std::uint64_t tick, const PolygonDeviceContext &device,
                                   std::span<const WorldHeaderRange> owners) noexcept
{
    if (!enabled_ || started_)
        return false; // Never overwrite a retained serial.
    if (thread_ != std::this_thread::get_id())
        return fail(WorldJoinError::foreign_thread);
    started_ = true;
    result_.accepted_serial = serial;
    result_.source_tick = tick;
    if (owners.size() > limits_.owners)
        return fail(WorldJoinError::capacity);
    try
    {
        for (const auto &o : owners)
        {
            if (o.headers.size() > limits_.header_entries - header_index_.size())
            {
                release_borrows();
                return fail(WorldJoinError::capacity);
            }
            const auto owner = result_.owners.size();
            result_.owners.push_back(o.owner);
            for (std::size_t i = 0; i < o.headers.size(); ++i)
                header_index_.emplace(o.headers.at(i).identity(), WorldHeaderMembership{owner, i});
        }
    }
    catch (...)
    {
        release_borrows();
        return fail(WorldJoinError::allocation);
    }
    device_ = &device;
    device_identity_ = device.identity();
    result_.status = WorldJoinStatus::collecting;
    return true;
}
bool WorldPacketJoinCapture::memberships(retail::WorldPolygonHeaderRef h, std::size_t &first,
                                         std::size_t &count) noexcept
{
    first = result_.memberships.size();
    count = 0;
    const auto [begin, end] = header_index_.equal_range(h.identity());
    for (auto i = begin; i != end; ++i)
    {
        if (result_.memberships.size() == limits_.memberships)
            return fail(WorldJoinError::capacity);
        result_.memberships.push_back(i->second);
        ++count;
    }
    std::sort(result_.memberships.begin() + first, result_.memberships.end(), [](const auto &a, const auto &b) {
        return a.owner < b.owner || (a.owner == b.owner && a.offset < b.offset);
    });
    return true;
}
bool WorldPacketJoinCapture::preclip(const WorldJoinFrameView &f, unsigned controller) noexcept
{
    if (!active())
        return false;
    if (phase_ != Phase::preclip)
        return fail(WorldJoinError::order);
    if (result_.invocations.size() == limits_.views)
        return fail(WorldJoinError::capacity);
    for (unsigned p = 0; p < 4; ++p)
    {
        if (f.pools[p].size() > 3072)
            return fail(WorldJoinError::capacity);
        pending_pools_[p] = f.pools[p].data();
        lookup_[p].fill(absent);
    }
    for (const auto &r : f.lists)
        if (!valid(r, f))
            return fail(WorldJoinError::packet);
    for (const auto &r : f.clipping)
        if (!valid(r, f))
            return fail(WorldJoinError::packet);
    WorldJoinInvocation invocation;
    invocation.controller = controller;
    invocation.ranges = f.lists;
    invocation.clipping = f.clipping;
    const auto ordinal = result_.invocations.size();
    result_.invocations.push_back(invocation);
    try
    {
        for (const auto &r : f.lists)
            for (std::size_t e = 0; e < r.count; ++e)
            {
                const auto slot = *r.first + e;
                if (lookup_[r.pool][slot] != absent)
                    continue;
                if (result_.origins.size() == limits_.origins)
                    return fail(WorldJoinError::capacity);
                const auto &packet = f.pools[r.pool][slot];
                WorldPacketOrigin out;
                out.invocation = ordinal;
                out.pool = r.pool;
                out.slot = slot;
                out.present = bool(packet.header);
                out.inside_mask = packet.inside_mask;
                if (out.present)
                    out.header = packet.header.value();
                if (!memberships(packet.header, out.membership_begin, out.membership_count))
                    return false;
                for (unsigned c = 0; c < 7; ++c)
                {
                    const auto &cr = f.clipping[c];
                    if (cr.pool == r.pool && cr.first && slot >= *cr.first && slot - *cr.first < cr.count)
                        out.clipping_lists |= 1u << c;
                }
                lookup_[r.pool][slot] = result_.origins.size();
                result_.origins.push_back(out);
                origin_headers_.push_back(packet.header.identity());
            }
    }
    catch (...)
    {
        return fail(WorldJoinError::packet);
    }
    phase_ = Phase::submission;
    return true;
}
bool WorldPacketJoinCapture::before_submission(const WorldJoinFrameView &f, const retail::PolygonSubmissionInputs &in,
                                               const retail::PolygonSubmissionSink &sink) noexcept
{
    if (!active())
        return false;
    if (phase_ != Phase::submission)
        return fail(WorldJoinError::order);
    if (static_cast<const retail::PolygonSubmissionSink *>(device_) != &sink)
        return fail(WorldJoinError::sink);
    auto &v = result_.invocations.back();
    v.viewport = world_join_viewport(in.viewport);
    v.depth = in.depth_capability;
    v.quality = in.quality;
    v.restriction = in.legacy_driver_restriction;
    v.multitexture = in.multitexture_capability;
    if (!in.materials.empty())
        v.alternate_token = in.materials[0].alternate_texture;
    for (unsigned p = 0; p < 4; ++p)
        if (pending_pools_[p] != f.pools[p].data() || f.pools[p].size() > 3072)
            return fail(WorldJoinError::owner);
    try
    {
        for (unsigned list = 0; list < 9; ++list)
        {
            const auto &r = f.lists[list];
            const auto &old = v.ranges[list];
            if (!valid(r, f) || r.pool != old.pool || r.first != old.first || r.count != old.count)
                return fail(WorldJoinError::packet);
            v.list_begin[list] = result_.occurrences.size();
            v.list_count[list] = r.count;
            for (std::size_t e = 0; e < r.count; ++e)
            {
                if (result_.occurrences.size() == limits_.occurrences)
                    return fail(WorldJoinError::capacity);
                const auto slot = *r.first + e;
                const auto origin = lookup_[r.pool][slot];
                if (origin == absent)
                    return fail(WorldJoinError::packet);
                const auto &packet = f.pools[r.pool][slot];
                WorldPacketOccurrence out;
                out.origin = origin;
                out.list = list;
                out.entry = e;
                out.present = bool(packet.header);
                out.relation =
                    out.present ? (origin_headers_[origin] == packet.header.identity() ? WorldPreclipRelation::unchanged
                                                                                       : WorldPreclipRelation::replaced)
                                : (result_.origins[origin].present ? WorldPreclipRelation::rejected
                                                                   : WorldPreclipRelation::absent);
                if (out.present)
                {
                    out.header = packet.header.value();
                    if (out.header[0] < 3 || out.header[1] >= in.materials.size())
                        return fail(WorldJoinError::packet);
                    out.primary_token = in.materials[out.header[1]].primary_texture;
                    out.clip_header = retail::WorldPolygonHeaderView(f.clip_headers).find(packet.header);
                    if (out.relation == WorldPreclipRelation::replaced && !out.clip_header)
                        return fail(WorldJoinError::owner);
                }
                if (!memberships(packet.header, out.membership_begin, out.membership_count))
                    return false;
                result_.occurrences.push_back(out);
            }
        }
    }
    catch (...)
    {
        return fail(WorldJoinError::packet);
    }
    // These are already recorded CPU context words, not a device API call.
    v.before = device_->snapshot().operation_sequence;
    phase_ = Phase::submitted;
    return true;
}
bool WorldPacketJoinCapture::after_submission(const retail::PolygonSubmissionSink &sink) noexcept
{
    if (!active())
        return false;
    if (phase_ != Phase::submitted)
        return fail(WorldJoinError::order);
    if (static_cast<const retail::PolygonSubmissionSink *>(device_) != &sink)
        return fail(WorldJoinError::sink);
    auto &v = result_.invocations.back();
    v.after = device_->snapshot().operation_sequence;
    if (v.after < v.before)
        return fail(WorldJoinError::sequence);
    for (auto &h : origin_headers_)
        h = nullptr;
    pending_pools_ = {};
    phase_ = Phase::preclip;
    return true;
}
void WorldPacketJoinCapture::release_borrows() noexcept
{
    device_ = nullptr;
    header_index_.clear();
    origin_headers_.clear();
    pending_pools_ = {};
}
void WorldPacketJoinCapture::abort(WorldJoinError e) noexcept
{
    if (!enabled_ || finished_)
        return;
    fail(e);
    finished_ = true;
    release_borrows();
}

bool WorldPacketJoinCapture::resolve(const CapturedPolygonSubmission &submission)
{
    using Op = PolygonDeviceOperation;
    using Route = retail::PolygonSubmissionRoute;
    struct Event
    {
        const PolygonDeviceCall *call;
        const CapturedPolygonCommand *command;
        std::size_t segment, ordinal, global;
    };
    std::vector<Event> events;
    if (submission.events > limits_.events || submission.segments.size() > limits_.segments)
        return fail(WorldJoinError::capacity);
    events.reserve(submission.events);
    std::uint64_t sequence = submission.initial.operation_sequence;
    std::size_t global = 0;
    for (std::size_t s = 0; s < submission.segments.size(); ++s)
    {
        const auto &segment = submission.segments[s];
        if (!segment.device_frame || !segment.device_frame->polygon_commands)
            return fail(WorldJoinError::event);
        const auto &f = *segment.device_frame;
        const auto &commands = f.polygon_commands->commands;
        if (!same_owner(device_identity_, f.device) || f.initial.operation_sequence != sequence ||
            f.polygon_commands->source_tick != result_.source_tick)
            return fail(WorldJoinError::identity);
        std::size_t next = 0;
        for (std::size_t i = 0; i < f.events.size(); ++i)
        {
            const auto &e = f.events[i];
            if (events.size() == limits_.events || sequence == (std::numeric_limits<std::uint64_t>::max)() ||
                e.sequence != ++sequence || !e.cpu_accepted || e.hresult || e.rejection != PolygonDeviceRejection::none)
                return fail(WorldJoinError::event);
            const CapturedPolygonCommand *command = nullptr;
            const bool has_command = e.operation == Op::render_state || e.operation == Op::texture ||
                                     e.operation == Op::texture_stage_state || e.operation == Op::draw ||
                                     e.operation == Op::draw_indexed;
            if (e.polygon_command.has_value() != has_command)
                return fail(WorldJoinError::event);
            if (e.polygon_command)
            {
                if (*e.polygon_command != next || next >= commands.size())
                    return fail(WorldJoinError::event);
                command = &commands[next++];
                bool equal = false;
                if (const auto *r = std::get_if<CapturedRenderState>(command))
                    equal = e.operation == Op::render_state &&
                            e.arguments == std::array<std::uint32_t, 3>{r->state, r->value, 0};
                if (const auto *t = std::get_if<CapturedTexture>(command))
                    equal = e.operation == Op::texture &&
                            e.arguments == std::array<std::uint32_t, 3>{t->stage, t->token, 0};
                if (const auto *t = std::get_if<CapturedTextureStageState>(command))
                    equal = e.operation == Op::texture_stage_state &&
                            e.arguments == std::array<std::uint32_t, 3>{t->stage, t->state, t->value};
                if (const auto *d = std::get_if<CapturedPolygonDraw>(command))
                    equal = e.operation == (d->indexed ? Op::draw_indexed : Op::draw) &&
                            e.arguments == std::array{d->type, d->fvf, d->flags};
                if (!equal)
                    return fail(WorldJoinError::event);
            }
            events.push_back({&e, command, s, i, global + (e.polygon_command ? *e.polygon_command : 0)});
        }
        if (next != commands.size() || f.final.operation_sequence != sequence)
            return fail(WorldJoinError::event);
        segments_.push_back({segment.device_frame, f.initial.operation_sequence, f.final.operation_sequence});
        global += commands.size();
    }
    if (events.size() != submission.events || global != submission.commands ||
        sequence != submission.final.operation_sequence)
        return fail(WorldJoinError::event);
    std::uint64_t prior = submission.initial.operation_sequence;
    for (std::size_t invocation = 0; invocation < result_.invocations.size(); ++invocation)
    {
        const auto &v = result_.invocations[invocation];
        if (v.before < prior || v.after < v.before || v.after > sequence)
            return fail(WorldJoinError::sequence);
        prior = v.after;
        auto at = std::lower_bound(events.begin(), events.end(), v.before,
                                   [](const Event &e, std::uint64_t seq) { return e.call->sequence <= seq; });
        const auto expect = [&](Op op, std::array<std::uint32_t, 3> args) -> const Event * {
            if (at == events.end() || at->call->sequence > v.after || at->call->operation != op ||
                at->call->arguments != args)
                throw WorldJoinError::event;
            return &*at++;
        };
        const auto rs = [&](std::uint32_t state, std::uint32_t value) { expect(Op::render_state, {state, value, 0}); };
        const auto ts = [&](std::uint32_t stage, std::uint32_t state, std::uint32_t value) {
            expect(Op::texture_stage_state, {stage, state, value});
        };
        const auto texture = [&](unsigned stage, std::uint32_t token) { expect(Op::texture, {stage, token, 0}); };
        const auto depth = [&] {
            if (v.depth)
            {
                rs(23, 7);
                rs(7, 1);
            }
        };
        const auto spec = [&](bool on) {
            if (v.restriction != 1)
            {
                rs(29, on ? 1u : 0u);
                rs(24, on ? 1u : 160u);
            }
        };
        const auto draw = [&](bool indexed, std::size_t vertices,
                              const std::vector<WorldPacketSpan> &spans) -> const Event * {
            const auto *e = expect(indexed ? Op::draw_indexed : Op::draw, {indexed ? 4u : 5u, 0x2c4, 0x1c});
            if (!e->command)
                throw WorldJoinError::draw;
            const auto *d = std::get_if<CapturedPolygonDraw>(e->command);
            if (!d || d->vertices.size() != vertices || world_join_viewport(d->viewport) != v.viewport)
                throw WorldJoinError::draw;
            if (!indexed)
            {
                if (!d->indices.empty())
                    throw WorldJoinError::draw;
                return e;
            }
            std::size_t index = 0;
            for (const auto &p : spans)
                for (std::size_t i = 1; i + 1 < p.vertex_count; ++i)
                {
                    for (const auto expected : {p.vertex_begin, p.vertex_begin + i, p.vertex_begin + i + 1})
                    {
                        if (index >= d->indices.size() || d->indices[index++] != expected)
                            throw WorldJoinError::draw;
                    }
                }
            if (index != d->indices.size())
                throw WorldJoinError::draw;
            return e;
        };
        depth();
        rs(24, 160);
        for (unsigned list = 0; list < 7; ++list)
        {
            const auto requested = list == 0
                                       ? Route::sky
                                       : (list < 3 ? Route::opaque
                                                   : (list < 5 ? (v.quality == 1 ? Route::specular : Route::opaque)
                                                               : Route::multitexture));
            const auto route =
                requested == Route::multitexture && (!v.multitexture || !v.quality) ? Route::opaque : requested;
            if (v.list_count[list] == 0)
                continue;
            if (route == Route::sky)
                rs(23, 8);
            const bool special = (route == Route::specular || route == Route::multitexture) && v.quality == 1;
            if (special)
                spec(true);
            if (route == Route::multitexture)
            {
                texture(1, v.alternate_token);
                ts(1, 11, 1);
                ts(1, 1, 7);
            }
            std::vector<WorldPacketSpan> spans;
            std::size_t vertices = 0, indices = 0;
            unsigned material = 256;
            const auto flush = [&] {
                if (!vertices)
                    return;
                const auto *e = draw(true, vertices, spans);
                result_.batches.push_back({invocation, e->segment, e->ordinal, *e->call->polygon_command, e->global,
                                           e->call->sequence, list, requested, route, std::move(spans)});
                spans.clear();
                vertices = indices = 0;
            };
            for (std::size_t i = 0; i < v.list_count[list]; ++i)
            {
                const auto ordinal = v.list_begin[list] + i;
                const auto &p = result_.occurrences[ordinal];
                if (!p.present)
                    continue;
                if (p.header[1] != material)
                {
                    flush();
                    material = p.header[1];
                    texture(0, p.primary_token);
                }
                const auto n = std::size_t(p.header[0]);
                if (vertices + n > 65536)
                    throw WorldJoinError::draw;
                spans.push_back({ordinal, vertices, n, indices, (n - 2) * 3});
                vertices += n;
                indices += (n - 2) * 3;
            }
            flush();
            if (route == Route::multitexture)
                ts(1, 1, 1);
            if (special)
                spec(false);
            if (route == Route::sky)
                depth();
        }
        rs(24, 1);
        for (unsigned list = 7; list < 9; ++list)
        {
            if (v.list_count[list] == 0)
                continue;
            rs(14, 0);
            rs(27, 1);
            unsigned material = 256, mode = 256;
            for (std::size_t i = 0; i < v.list_count[list]; ++i)
            {
                const auto &p = result_.occurrences[v.list_begin[list] + i];
                if (!p.present)
                    continue;
                const auto next = unsigned(p.header[3] & 0x78);
                if (next != mode)
                {
                    mode = next;
                    rs(19, mode == 0x70 ? 9u : mode == 0x58 ? 2u : 5u);
                    rs(20, mode == 0x70 ? 1u : (mode == 0x48 || mode == 0x50) ? 6u : 2u);
                    ts(0, 2, 2);
                }
                if (p.header[1] != material)
                {
                    material = p.header[1];
                    texture(0, p.primary_token);
                }
                if (p.header[0] > 15)
                    throw WorldJoinError::draw;
                (void)draw(false, p.header[0], {});
            }
            rs(19, 2);
            rs(20, 1);
            ts(0, 2, 2);
            ts(0, 3, 0);
            rs(27, 0);
            rs(14, 1);
            depth();
        }
        if (at != events.end() && at->call->sequence <= v.after)
            throw WorldJoinError::event;
        // No absent events or gaps can be hidden at the end of this scope.
        if (v.after > v.before && (at == events.begin() || (at - 1)->call->sequence != v.after))
            throw WorldJoinError::sequence;
    }
    return true;
}
bool WorldPacketJoinCapture::finish(const ImmutablePolygonSubmission &submission) noexcept
{
    if (!active())
    {
        if (started_ && !finished_)
        {
            finished_ = true;
            release_borrows();
        }
        return false;
    }
    finished_ = true;
    if (phase_ != Phase::preclip)
    {
        release_borrows();
        return fail(WorldJoinError::order);
    }
    if (!submission)
    {
        release_borrows();
        return fail(WorldJoinError::identity, WorldJoinStatus::no_recipe);
    }
    try
    {
        if (submission->source_tick != result_.source_tick || !same_owner(device_identity_, submission->device))
        {
            release_borrows();
            return fail(WorldJoinError::identity, WorldJoinStatus::mismatch);
        }
        const auto good = resolve(*submission);
        release_borrows();
        if (!good)
        {
            result_.batches.clear();
            segments_.clear();
            return false;
        }
        initial_sequence_ = submission->initial.operation_sequence;
        final_sequence_ = submission->final.operation_sequence;
        command_count_ = submission->commands;
        event_count_ = submission->events;
        result_.status = WorldJoinStatus::derived_checked;
        return true;
    }
    catch (WorldJoinError e)
    {
        release_borrows();
        result_.batches.clear();
        segments_.clear();
        return fail(e, WorldJoinStatus::mismatch);
    }
    catch (...)
    {
        release_borrows();
        result_.batches.clear();
        segments_.clear();
        return fail(WorldJoinError::allocation);
    }
}
bool WorldPacketJoinCapture::matches_displayed(const ImmutablePolygonSubmission &displayed) const noexcept
{
    if (thread_ != std::this_thread::get_id() || result_.status != WorldJoinStatus::derived_checked || !displayed ||
        displayed->source_tick != result_.source_tick || !same_owner(device_identity_, displayed->device) ||
        displayed->segments.size() != segments_.size() || displayed->initial.operation_sequence != initial_sequence_ ||
        displayed->final.operation_sequence != final_sequence_ || displayed->commands != command_count_ ||
        displayed->events != event_count_)
        return false;
    for (std::size_t i = 0; i < segments_.size(); ++i)
    {
        const auto &actual = displayed->segments[i].device_frame;
        const auto &wanted = segments_[i];
        // Temporary CPU-frame lock only; no recipe or texture-bank ownership.
        const auto retained = wanted.owner.lock();
        if (!actual || !retained || retained.get() != actual.get() || !same_owner(wanted.owner, actual) ||
            actual->initial.operation_sequence != wanted.before || actual->final.operation_sequence != wanted.after)
            return false;
    }
    return !segments_.empty();
}
} // namespace sgn::diagnostics
