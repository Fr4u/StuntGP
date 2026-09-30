#include "sgn/source_world_polygon_clipping.hpp"
#include "sgn/source_clip_packet_codec.hpp"
#include "sgn/detail/world_polygon_clip_kernel.hpp"
#include "sgn/race_session.hpp"
#include "sgn/host_wide_race_view.hpp"
#include <bit>
#include <cmath>
#include <limits>

namespace sgn::retail
{
namespace
{
void admit(const std::shared_ptr<RaceSessionPlatform> &platform, std::uint32_t site)
{
    if (!platform || !platform->source_construction_only_entry())
        throw SourceAllocationError(site, "source polygon stage requires its actual source platform");
    platform->require_owner_thread();
}
CameraLifecycle &current_camera(RaceSessionState &state)
{
    const auto current = state.controls.current;
    if (current >= state.controls.controllers.size() || current >= state.camera_lifecycle.size())
        throw SourceAllocationError(0x4277df, "current source camera is outside its platform");
    return state.camera_lifecycle[current];
}
struct SourceClipAccess
{
    static constexpr bool source = true;
    SourceClipPacketCodec &packet;
    RaceSessionState &state;
    std::uint32_t indices{};
    WorldPolygonHeaderRef capture_header()
    {
        indices = packet.capture_indices_427543(); // pointer word only, before427549/427555
        return packet.header();                    // borrow original pointer, never its byte snapshot
    }
    std::uint16_t index(unsigned i) const
    {
        return packet.index_at_427576(indices, i);
    }
    WorldPolygonAttribute attribute(unsigned i) const
    {
        const auto attributes = packet.attributes(); // fresh packet+08 for every corner
        if (i >= attributes.size())
            throw SourceAllocationError(0x427584, "source corner attribute outside owner");
        return attributes.value(i);
    }
    WorldPolygonVertex vertex(unsigned i) const
    {
        const auto vertices = packet.vertices(); // fresh packet+0C after the four attribute words
        if (i >= vertices.size())
            throw SourceAllocationError(0x4275b2, "source corner vertex outside owner");
        return vertices[i];
    }
    // Optional provenance APIs are meaningful but are never reached by this
    // source entry: it passes no audit or fabricated input mirror.
    std::size_t attribute_size() const
    {
        return packet.attributes().size();
    }
    std::size_t vertex_size() const
    {
        return packet.vertices().size();
    }
    std::uint32_t mask() const
    {
        return packet.mask();
    }
    float near_distance() const
    {
        return current_camera(state).clip_near;
    }
    void reject() const
    {
        packet.reject_427520();
    }
    detail::ClipOutputPublication publish_output(WorldPolygonClipStorage &storage, unsigned) const
    {
        if (&storage != &state.polygon_frame.clip)
            throw SourceAllocationError(0x4276c8, "source clipping output belongs to another frame");
        return {packet.publish_output_4276c8(), 0, 0}; // actual four packet stores before header/payload access
    }
};
void require_source_owner(const SourceWorldBufferBinding &binding, const std::shared_ptr<RaceSessionPlatform> &platform,
                          std::uint32_t site)
{
    if (binding.bank != &platform->source_allocations() || binding.platform.owner_before(platform) ||
        platform.owner_before(binding.platform))
        throw SourceAllocationError(site, "source projection buffer belongs to another platform");
}
std::uint32_t clip_count(const SourceWorldSpan<WorldPolygonVertex> &base, const SourceClipCurrentPointer &current,
                         const std::shared_ptr<RaceSessionPlatform> &platform)
{
    const auto &a = base.source_binding();
    const auto &b = current.source_pointer();
    require_source_owner(a, platform, 0x429abc);
    require_source_owner(b, platform, 0x429ac1);
    if (a.allocation != b.allocation || a.bytes != b.bytes)
        throw SourceAllocationError(0x429ac7, "source clip subtraction crosses allocation identities");
    if (b.byte_offset <= a.byte_offset)
        return 0; // signed nonpositive difference, no payload access
    const auto delta = b.byte_offset - a.byte_offset;
    if (delta % 32 || delta > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
        throw SourceAllocationError(0x429ac7, "source clip difference outside represented signed/aligned domain");
    return delta / 32;
}
// == for(i=first;i<end;++i)project_world_polygon_vertex(span[i],view): the
// checked view is taken at the first reached element (as operator[] did) and
// reused; each element keeps the operator[] bound check and message.
void project_span_elements(const SourceWorldSpan<WorldPolygonVertex> &span, std::uint32_t first, std::uint32_t end,
                           const ViewportState &view)
{
    std::span<WorldPolygonVertex> elements;
    bool viewed = false;
    for (auto i = first; i < end; ++i)
    {
        if (!viewed)
        {
            elements = span.view();
            viewed = true;
        }
        if (i >= elements.size())
            throw SourceAllocationError(0x42e430, "source world buffer index outside owner");
        project_world_polygon_vertex(elements[i], view);
    }
}
std::uint32_t indexed_count(const WorldIndexedVertexPointer &first, const WorldIndexedVertexPointer &last)
{
    if (first.is_null() && last.is_null())
        return 0;
    if (first.is_null() || last.is_null() || !first.same_storage(last))
        throw SourceAllocationError(0x429a73, "source indexed pointer subtraction crosses owners");
    if (!first.source_bound() || !last.source_bound())
        throw SourceAllocationError(0x429a73, "source projection received native pointer storage");
    // 429A73/SAR2/JLE: no index or vertex payload read on a nonpositive count.
    const auto a = *first.offset(), b = *last.offset();
    if (b <= a)
        return 0;
    if (b - a > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max() / 4))
        throw SourceAllocationError(0x429a73, "source indexed difference outside represented signed domain");
    return static_cast<std::uint32_t>(b - a);
}
} // namespace
void clip_source_world_polygon_427530(const std::shared_ptr<RaceSessionPlatform> &platform,
                                      WorldPolygonPacketPointer pointer)
{
    admit(platform, 0x427530);
    SourceClipPacketCodec packet(platform, std::move(pointer));
    auto &state = platform->state();
    SourceClipAccess access{packet, state};
    detail::clip_polygon_kernel(access, state.polygon_frame.clip);
}
void clip_source_world_polygon_list_4277b0(const std::shared_ptr<RaceSessionPlatform> &platform,
                                           WorldPolygonPacketPointer pointer, std::uint32_t count)
{
    admit(platform, 0x4277b0);
    // The zero path must not attempt to construct a record codec or dereference
    // the captured list pointer. Each later record's current mask is reloaded.
    //  NEW host performance (perf_world R19): the per-record codec admission
    //  and its mask() read are performed through one checked record run over
    //  this local pointer register, in the codec's order and with its errors;
    //  a codec is constructed only for a reached427530 kernel. The codec's
    //  "packet platform expired" test cannot fail here: its weak borrow is
    //  made from the admitted live shared owner held by this call. Kernel
    //  stores are raw packet/clip payload stores and token encodes only.
    WorldPolygonPacketPointer::SourceRun run(pointer);
    for (std::uint32_t remaining = count; remaining; --remaining)
    {
        platform->require_owner_thread(); // codec constructor
        if (!pointer.source_bound())
            throw SourceAllocationError(0x4276c8, "packet codec requires raw source records");
        run.require_source_platform(platform);
        platform->require_owner_thread(); // mask(): bank()
        if (run.source_word(4) != 63)
        {
            SourceClipPacketCodec packet(platform, pointer);
            auto &state = platform->state();
            SourceClipAccess access{packet, state};
            detail::clip_polygon_kernel(access, state.polygon_frame.clip);
        }
        run.advance(); // 4277C7, including the final local pointer advance
    }
}
void clip_source_world_polygon_lists_4277d0(const std::shared_ptr<RaceSessionPlatform> &platform)
{
    admit(platform, 0x4277d0);
    auto &state = platform->state();
    auto &frame = state.polygon_frame;
    reset_world_polygon_clip_storage(frame.clip);
    auto count = frame.groups[0][0].count;
    if (count)
    {
        auto &camera = current_camera(state);
        const auto first = frame.groups[0][0].first;
        camera.clip_far = std::bit_cast<float>(0x4a989680u);
        clip_source_world_polygon_list_4277b0(platform, first, count);
    }
    auto &next_camera = current_camera(state);
    count = frame.groups[1][0].count;
    next_camera.clip_far = std::bit_cast<float>(0x48f42400u);
    if (count)
        clip_source_world_polygon_list_4277b0(platform, frame.groups[1][0].first, count);
    count = frame.groups[1][1].count;
    if (count)
        clip_source_world_polygon_list_4277b0(platform, frame.groups[1][1].first, count);
    current_camera(state).clip_far = std::bit_cast<float>(0x48f42400u);
    count = frame.groups[2][0].count;
    if (count)
        clip_source_world_polygon_list_4277b0(platform, frame.groups[2][0].first, count);
    count = frame.groups[2][1].count;
    if (count)
        clip_source_world_polygon_list_4277b0(platform, frame.groups[2][1].first, count);
    count = frame.groups[3][0].count;
    if (count)
        clip_source_world_polygon_list_4277b0(platform, frame.groups[3][0].first, count);
    count = frame.groups[3][1].count;
    if (count)
        clip_source_world_polygon_list_4277b0(platform, frame.groups[3][1].first, count);
}
void project_source_world_polygon_vertices_429a40(const std::shared_ptr<RaceSessionPlatform> &platform)
{
    admit(platform, 0x429a40);
    auto &state = platform->state();
    auto &frame = state.polygon_frame;
    ViewportState view;                      // invocation-local four-scalar capture, not a stored viewport owner
    view.scale_x = state.viewport.scale_x;   // 429A46
    view.scale_y = state.viewport.scale_y;   // 429A4B
    view.center_x = state.viewport.center_x; // 429A51
    view.center_y = state.viewport.center_y; // 429A5A
    if (const auto *wide = host_wide::race_view())
    {
        // NEW R20 HOST (wide_3d) Hor+: inside the race 3D stages the projection
        //  words are P/k, so the view scalars are scaled by k about the canvas
        //  centre. Invocation-local copy only; ViewportState is not written.
        view.scale_x = wide->widened.half_width;
        view.scale_y = wide->widened.half_height;
        view.center_x = wide->widened.center_x;
        view.center_y = wide->widened.center_y;
    }
    const auto indexed_first = frame.indexed.base();   // 429A62
    const auto indexed_last = frame.indexed.current(); // 429A6B
    const auto count = indexed_count(indexed_first, indexed_last);
    if (count)
    {
        require_source_owner(frame.transformed.allocation.source_binding(), platform, 0x429a82);
        indexed_first.require_source_vertices(frame.transformed.allocation);
    }
    // NEW host performance (perf_world R19): checked views are admitted once,
    // at the first element each loop reaches, and every element keeps its
    // index/identity checks. Projection stores are raw vertex payload stores.
    {
        WorldIndexedVertexPointer::SourceReadRun indexed(indexed_first);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            auto *point = indexed.vertex_at(*indexed_first.offset() + i); // fresh stored pointer, duplicates retained
            if (!point)
                throw SourceAllocationError(0x429a88, "reached null source indexed vertex");
            project_world_polygon_vertex(*point, view);
        }
    }
    const auto clipped = frame.clip.vertices;          // 429ABC, only after all indexed stores
    const auto clip_current = frame.clip.vertex_count; // 429AC1
    const auto clipped_count = clip_count(clipped, clip_current, platform);
    project_span_elements(clipped, 0, clipped_count, view);
    const auto remaining_end = frame.transformed.count; // 429B0E, only after all clip stores
    const auto remaining_first = frame.remaining_first; // 429B14
    const auto remaining = frame.transformed.allocation;
    // Source signed nonpositive pointer difference skips this loop; no eager
    // three-span admission may discard earlier successful indexed/clip writes.
    if (remaining_end > remaining_first)
    {
        require_source_owner(remaining.source_binding(), platform, 0x429b26);
        if (remaining_end - remaining_first > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max() / 32))
            throw SourceAllocationError(0x429b19, "source remaining difference outside represented signed domain");
        project_span_elements(remaining, remaining_first, remaining_end, view);
    }
}
} // namespace sgn::retail
