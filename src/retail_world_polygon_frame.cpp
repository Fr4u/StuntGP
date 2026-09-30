#include "sgn/retail_world_polygon_frame.hpp"
#include <limits>
#include <thread>

namespace sgn::retail
{
namespace detail
{
struct WorldPolygonLevelLease
{
    std::uint64_t generation{};
    bool attached{};
    std::thread::id owner_thread;
};
} // namespace detail
namespace
{
void require_allocated(const WorldPolygonFrame &frame)
{
    frame.indexed.require_owner_thread();
    if (!frame.allocations)
        throw FormatError("world polygon allocations are absent");
}
// Shared scalar/pointer publications accept the actual source owners. Native
// packet-view functions below still require the native array explicitly.
void require_frame_binding(const WorldPolygonFrame &frame)
{
    if (!frame.transformed.allocation.source_bound())
    {
        require_allocated(frame);
        return;
    }
    frame.indexed.require_owner_thread();
    const auto &binding = frame.transformed.allocation.source_binding();
    const auto platform = binding.platform.lock();
    if (!platform || !binding.bank)
        throw FormatError("source world frame platform expired");
    (void)source_world_buffer_bytes(binding); // actual generation, thread, extent
    for (const auto &packets : frame.packets)
        packets.base().require_source_platform(platform);
    frame.indexed.base().require_source_vertices(frame.transformed.allocation);
}
std::span<WorldPolygonPacket> range(const WorldPolygonFrame &frame, unsigned group, unsigned pool)
{
    require_allocated(frame);
    const auto &r = frame.groups.at(group).at(pool);
    if (!r.first.is_null())
        r.first.require_storage(frame.allocations->packet_lifetimes.at(pool));
    return r.first.range(r.count);
}
std::span<WorldPolygonPacket> alpha_range(const WorldPolygonFrame &frame, unsigned pool)
{
    require_allocated(frame);
    const auto base = frame.packets.at(pool).base();
    if (!base.is_null())
        base.require_storage(frame.allocations->packet_lifetimes.at(pool));
    return base.range(frame.totals.at(pool));
}
void observe(WorldPolygonFrameStoreObserver *observer, std::uint32_t pc, std::uint32_t address,
             const WorldPolygonFrame &frame)
{
    if (observer)
        observer->stored(pc, address, frame);
}
} // namespace
WorldPolygonFrame::WorldPolygonFrame() : level_lease_(std::make_shared<detail::WorldPolygonLevelLease>()) {}
WorldPolygonFrame::~WorldPolygonFrame()
{
    detach_world_polygon_level(*this);
}
void attach_world_polygon_level(WorldPolygonFrame &frame)
{
    require_frame_binding(frame);
    auto &lease = *frame.level_lease_;
    if (lease.attached)
        throw FormatError("world polygon level already attached");
    if (lease.generation == std::numeric_limits<std::uint64_t>::max())
        throw FormatError("world polygon level generation exhausted");
    ++lease.generation;
    lease.owner_thread = std::this_thread::get_id();
    lease.attached = true;
}
void detach_world_polygon_level(WorldPolygonFrame &frame) noexcept
{
    frame.level_lease_->attached = false;
}
WorldPolygonFrameTicket capture_world_polygon_level(const WorldPolygonFrame &frame)
{
    WorldPolygonFrameTicket ticket;
    ticket.lease_ = frame.level_lease_;
    ticket.generation_ = frame.level_lease_->generation;
    validate_world_polygon_level(ticket);
    return ticket;
}
void validate_world_polygon_level(const WorldPolygonFrameTicket &ticket)
{
    const auto lease = ticket.lease_.lock();
    if (!lease || !lease->attached || lease->generation != ticket.generation_)
        throw FormatError("world polygon level is detached or replaced");
    if (lease->owner_thread != std::this_thread::get_id())
        throw FormatError("world polygon borrow used outside its owner thread");
}
void validate_world_polygon_level(const WorldPolygonFrame &frame, const WorldPolygonFrameTicket &ticket)
{
    validate_world_polygon_level(ticket);
    if (ticket.lease_.lock() != frame.level_lease_)
        throw FormatError("world polygon ticket belongs to a different frame");
}
void allocate_world_polygon_frame(WorldPolygonFrame &frame, WorldPolygonFrameStoreObserver *observer)
{
    frame.indexed.require_owner_thread(); // host admission before any owner replacement
    // Build the new owners before publishing any borrowed views. Allocation
    // failure leaves the prior frame storage and its aliases intact.
    auto allocation = std::make_unique<WorldPolygonFrameAllocations>();
    detach_world_polygon_level(frame);
    frame.allocations = std::move(allocation);
    auto &a = *frame.allocations;
    frame.transformed = {a.vertices, 0};
    frame.indexed.publish_base(a.indexed_lifetime.pointer()); // 42E487;61C520 is not written here
    for (const auto p : {2u, 3u, 0u, 1u})
    {
        constexpr std::array<unsigned, 4> pc{0x42e4ba, 0x42e4cb, 0x42e498, 0x42e4a9};
        constexpr std::array<unsigned, 4> address{0x61c5fc, 0x61c600, 0x61c5f4, 0x61c5f8};
        frame.packets[p].publish_base(a.packet_lifetimes[p].pointer());
        observe(observer, pc[p], address[p], frame);
    } // Source allocation publishes only bases; current/groups/totals survive.
    for (unsigned p = 0; p < 2; ++p)
        frame.spatial[p] = {a.spatial[p], 0};
    frame.clip = {a.clip_attributes, a.clip_headers, a.clip_unused_indices,
                  std::span(a.vertices).subspan(WorldPolygonFrameAllocations::clip_vertex_first)};
    initialize_world_polygon_clip_storage(frame.clip);
    frame.remaining_first = 0;
}
void reset_world_polygon_vertex_cursors(WorldPolygonFrame &frame)
{
    require_frame_binding(frame);
    frame.transformed.count = 0;
    frame.indexed.copy_base_to_current(); // 42E805..42E810
}
void reset_world_polygon_groups(WorldPolygonFrame &frame, WorldPolygonFrameStoreObserver *observer)
{
    require_frame_binding(frame);
    for (unsigned g = 0; g < 4; ++g)
        for (unsigned p = 0; p < 4; ++p)
        {
            auto &r = frame.groups[g][p];
            const auto address = 0x6243e0 + g * 32 + p * 8;
            r.first = {};
            observe(observer, 0x42b10d, address, frame);
            r.count = 0;
            observe(observer, 0x42b10d, address + 4, frame);
        }
    const auto p0 = frame.packets[0].base(), p1 = frame.packets[1].base(), p2 = frame.packets[2].base();
    frame.packets[0].publish_current(p0);
    observe(observer, 0x42b120, 0x6243c0, frame);
    const auto p3 = frame.packets[3].base();
    frame.packets[1].publish_current(p1);
    observe(observer, 0x42b12a, 0x6243c8, frame);
    frame.packets[2].publish_current(p2);
    observe(observer, 0x42b130, 0x6243d0, frame);
    frame.packets[3].publish_current(p3);
    observe(observer, 0x42b136, 0x6243d8, frame);
}
void begin_world_polygon_group(WorldPolygonFrame &frame, unsigned group, WorldPolygonFrameStoreObserver *observer)
{
    require_frame_binding(frame);
    auto &ranges = frame.groups.at(group);
    const auto address = 0x6243e0 + group * 32;
    const auto p3 = frame.packets[3].current(), p2 = frame.packets[2].current();
    ranges[3].first = p3;
    observe(observer, 0x42b14b, address + 24, frame);
    const auto p0 = frame.packets[0].current();
    ranges[2].first = p2;
    observe(observer, 0x42b154, address + 16, frame);
    ranges[0].first = p0;
    observe(observer, 0x42b157, address, frame);
    const auto p1 = frame.packets[1].current();
    ranges[2].count = 0;
    observe(observer, 0x42b161, address + 20, frame);
    ranges[3].count = 0;
    observe(observer, 0x42b164, address + 28, frame);
    ranges[0].count = 0;
    observe(observer, 0x42b167, address + 4, frame);
    ranges[1].first = p1;
    observe(observer, 0x42b16a, address + 8, frame);
    ranges[1].count = 0;
    observe(observer, 0x42b16d, address + 12, frame);
}
void finish_world_polygon_group(WorldPolygonFrame &frame, unsigned group, WorldPolygonFrameStoreObserver *observer)
{
    require_frame_binding(frame);
    auto &ranges = frame.groups.at(group);
    for (const auto p : {2u, 3u, 0u, 1u})
    {
        constexpr std::array<unsigned, 4> pc{0x42b1dc, 0x42b1f8, 0x42b19f, 0x42b1bd};
        auto &r = ranges[p];
        r.count = std::bit_cast<std::uint32_t>(world_polygon_packet_difference(r.first, frame.packets[p].current()));
        observe(observer, pc[p], 0x6243e4 + group * 32 + p * 8, frame);
    }
}
void publish_world_polygon_totals(WorldPolygonFrame &frame, WorldPolygonFrameStoreObserver *observer)
{
    frame.indexed.require_owner_thread();
    constexpr std::array<unsigned, 4> pc{0x42b22a, 0x42b24f, 0x42b274, 0x42b28e};
    for (unsigned p = 0; p < 4; ++p)
    {
        frame.totals[p] = frame.packets[p].count();
        observe(observer, pc[p], 0x6243c4 + p * 8, frame);
    }
}
std::span<WorldPolygonPacket> world_polygon_packet_pool(const WorldPolygonFrame &frame, unsigned pool)
{
    (void)capture_world_polygon_level(frame);
    require_allocated(frame);
    return frame.allocations->packet_lifetimes.at(pool).pointer().storage();
}
std::span<WorldPolygonPacket> world_polygon_group_packets(const WorldPolygonFrame &frame, unsigned group, unsigned pool)
{
    (void)capture_world_polygon_level(frame);
    return range(frame, group, pool);
}
std::span<WorldPolygonPacket> world_polygon_alpha_packets(const WorldPolygonFrame &frame, unsigned pool)
{
    (void)capture_world_polygon_level(frame);
    if (pool != 1 && pool != 3)
        throw FormatError("world alpha projection requires an alpha pool");
    return alpha_range(frame, pool);
}
std::array<std::span<WorldPolygonPacket>, 7> world_polygon_clip_lists(WorldPolygonFrame &frame)
{
    (void)capture_world_polygon_level(frame);
    return {range(frame, 0, 0), range(frame, 1, 0), range(frame, 1, 1), range(frame, 2, 0),
            range(frame, 2, 1), range(frame, 3, 0), range(frame, 3, 1)};
}
std::array<std::span<WorldPolygonPacket>, 9> world_polygon_submission_lists(WorldPolygonFrame &frame)
{
    (void)capture_world_polygon_level(frame);
    require_allocated(frame);
    return {range(frame, 0, 0), range(frame, 1, 0), range(frame, 1, 2),    range(frame, 2, 0),   range(frame, 2, 2),
            range(frame, 3, 0), range(frame, 3, 2), alpha_range(frame, 1), alpha_range(frame, 3)};
}
std::span<WorldPolygonVertex> world_polygon_remaining_vertices(WorldPolygonFrame &frame)
{
    (void)capture_world_polygon_level(frame);
    require_allocated(frame);
    if (frame.remaining_first > frame.transformed.count ||
        frame.transformed.count > frame.transformed.allocation.size())
        throw FormatError("world polygon projection range outside allocation");
    return frame.transformed.allocation.subspan(frame.remaining_first, frame.transformed.count - frame.remaining_first);
}
std::span<WorldPolygonVertex *> world_polygon_indexed_projection_vertices(const WorldPolygonFrame &frame)
{
    const auto ticket = capture_world_polygon_level(frame);
    validate_world_polygon_level(frame, ticket);
    require_allocated(frame);
    if (!frame.indexed.base().is_null())
        frame.indexed.base().require_storage(frame.allocations->indexed_lifetime);
    if (!frame.indexed.current().is_null())
        frame.indexed.current().require_storage(frame.allocations->indexed_lifetime);
    return frame.indexed.projection_vertices();
}
} // namespace sgn::retail
