#include "sgn/session_world_join.hpp"
#include "sgn/race_session.hpp"

namespace sgn::diagnostics
{
namespace
{
WorldJoinFrameView view(const retail::WorldPolygonFrame &frame, bool clipped)
{
    WorldJoinFrameView out;
    for (unsigned p = 0; p < 4; ++p)
        out.pools[p] = retail::world_polygon_packet_pool(frame, p);
    const auto group = [&](unsigned g, unsigned p) {
        const auto packets = retail::world_polygon_group_packets(frame, g, p);
        return WorldJoinRange{p, frame.groups[g][p].first.offset(), packets.size()};
    };
    const auto alpha = [&](unsigned p) {
        const auto packets = retail::world_polygon_alpha_packets(frame, p);
        return WorldJoinRange{p, frame.packets[p].base().offset(), packets.size()};
    };
    out.lists = {group(0, 0), group(1, 0), group(1, 2), group(2, 0), group(2, 2),
                 group(3, 0), group(3, 2), alpha(1),    alpha(3)};
    out.clipping = {group(0, 0), group(1, 0), group(1, 1), group(2, 0), group(2, 1), group(3, 0), group(3, 1)};
    if (clipped)
    {
        if (frame.clip.header_count > frame.clip.headers.size())
            throw FormatError("diagnostic clip headers outside storage");
        out.clip_headers = frame.clip.headers.first(frame.clip.header_count);
    }
    return out;
}
} // namespace
bool SessionWorldJoin::prepare(retail::RaceSession &session, const PolygonDeviceContext &device, std::uint64_t serial,
                               std::uint64_t tick) noexcept
{
    if (!enabled_ || session_)
        return false;
    try
    {
        session.shared_platform()->require_owner_thread();
        auto &state = session.state();
        auto ticket = retail::capture_world_polygon_level(state.polygon_frame);
        std::vector<WorldHeaderRange> owners;
        auto &models = session.models();
        for (std::size_t handle = 0; handle < models.size(); ++handle)
        {
            if (models.source_backed(handle))
                continue; // No raw-pointer escape from checked PMD storage.
            auto &model = models.at(handle);
            WorldHeaderOwner info;
            info.registry_token = models.identity(handle);
            info.model_handle = handle;
            info.source_name = model.source_name();
            for (unsigned slot = 0; slot < 4; ++slot)
            {
                try
                {
                    info.source_lods[slot] = model.source_lod_index(slot);
                }
                catch (const std::exception &)
                {
                }
            }
            info.kind = WorldHeaderKind::model_table;
            owners.push_back({info, retail::WorldPolygonHeaderView(model.source_attribute_table())});
            for (unsigned lod = 0; lod < model.lod_count(); ++lod)
                for (unsigned part = 0; part < model.part_count(lod); ++part)
                {
                    info.kind = WorldHeaderKind::model_part;
                    info.lod = lod;
                    info.part = part;
                    owners.push_back({info, retail::WorldPolygonHeaderView(model.source_attributes(lod, part))});
                }
        }
        // Storage kind is authoritative here. No invented PMD or effect identity.
        WorldHeaderOwner sprite;
        sprite.kind = WorldHeaderKind::sprite;
        if (!state.sprite_packets.headers.source_bound())
            owners.push_back({sprite, retail::WorldPolygonHeaderView(state.sprite_packets.headers.view())});
        WorldHeaderOwner strip;
        strip.kind = WorldHeaderKind::strip;
        owners.push_back({strip, retail::WorldPolygonHeaderView(std::span(state.strip.polygons))});
        if (!capture_.begin(serial, tick, device, owners))
            return false;
        ticket_ = std::move(ticket);
        session_ = &session;
        return true;
    }
    catch (...)
    {
        capture_.abort(WorldJoinError::owner);
        return false;
    }
}
void SessionWorldJoin::preclip(retail::RaceSession &session) noexcept
{
    if (!enabled_)
        return;
    try
    {
        if (session_ != &session || !ticket_)
            throw FormatError("diagnostic Session differs");
        const auto &s = session.state();
        retail::validate_world_polygon_level(s.polygon_frame, *ticket_);
        (void)capture_.preclip(view(s.polygon_frame, false), s.controls.current);
    }
    catch (...)
    {
        capture_.abort(WorldJoinError::owner);
    }
}
void SessionWorldJoin::before_submission(retail::RaceSession &session, const retail::PolygonSubmissionLists &lists,
                                         const retail::PolygonSubmissionInputs &inputs,
                                         const retail::PolygonSubmissionSink &sink) noexcept
{
    if (!enabled_)
        return;
    try
    {
        if (session_ != &session || !ticket_)
            throw FormatError("diagnostic Session differs");
        const auto &f = session.state().polygon_frame;
        retail::validate_world_polygon_level(f, *ticket_);
        const auto current = view(f, true);
        for (unsigned i = 0; i < 9; ++i)
        {
            const auto actual = i < 7 ? lists.categories[i] : lists.alpha[i - 7];
            const auto &r = current.lists[i];
            const auto pool = current.pools[r.pool];
            if ((!r.first && r.count) || (r.first && (*r.first > pool.size() || r.count > pool.size() - *r.first)))
                throw FormatError("diagnostic list outside pool");
            const auto expected =
                r.first ? pool.subspan(*r.first, r.count) : std::span<const retail::WorldPolygonPacket>{};
            if (actual.size() != expected.size() || actual.data() != expected.data())
                throw FormatError("diagnostic list differs from submitted list");
        }
        (void)capture_.before_submission(current, inputs, sink);
    }
    catch (...)
    {
        capture_.abort(WorldJoinError::owner);
    }
}
bool SessionWorldJoin::finish(const ImmutablePolygonSubmission &recipe) noexcept
{
    const auto ok = capture_.finish(recipe);
    ticket_.reset();
    session_ = nullptr;
    return ok;
}
} // namespace sgn::diagnostics
