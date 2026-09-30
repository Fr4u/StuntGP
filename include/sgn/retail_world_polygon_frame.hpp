#pragma once
#include "sgn/retail_world_spatial_producer.hpp"
#include "sgn/source_track_publications.hpp"
#include <memory>
#include <vector>

namespace sgn::retail
{
namespace detail
{
struct WorldPolygonLevelLease;
}
struct WorldPolygonFrame;
// New host lifetime ticket. It cannot retain a level or world owner. A ticket
// from a detached/replaced/destroyed level is invalid even after reattachment.
class WorldPolygonFrameTicket
{
    std::weak_ptr<const detail::WorldPolygonLevelLease> lease_;
    std::uint64_t generation_{};
    friend WorldPolygonFrameTicket capture_world_polygon_level(const WorldPolygonFrame &);
    friend void validate_world_polygon_level(const WorldPolygonFrameTicket &);
    friend void validate_world_polygon_level(const WorldPolygonFrame &, const WorldPolygonFrameTicket &);
};
// Allocation sizes are source42E430. Native pointer records use the same
// element counts; their larger host representation is never byte-reinterpreted.
struct WorldPolygonFrameAllocations
{
    static constexpr std::uint32_t vertex_capacity = 15360, clip_vertex_first = 12288;
    static constexpr std::uint32_t packet_capacity = 3072, indexed_capacity = 12288, spatial_capacity = 512;
    std::array<WorldPolygonVertex, vertex_capacity> vertices{};   // 61C50C
    std::array<WorldPolygonAttribute, 3072> clip_attributes{};    // 6244C8
    std::array<WorldPolygonHeader, 1024> clip_headers{};          // 6244CC
    std::array<std::uint16_t, 1024> clip_unused_indices{};        // 6244D0
    std::array<WorldPolygonVertex *, indexed_capacity> indexed{}; // 61C5EC
    // Source working cursor order6243C0/C8/D0/D8, hence allocation order
    // 61C5FC/61C600/61C5F4/61C5F8: clipped opaque/alpha, inside opaque/alpha.
    std::array<std::array<WorldPolygonPacket, packet_capacity>, 4> packets{};
    std::array<std::array<WorldSpatialBatch, spatial_capacity>, 2> spatial{}; // 61C518/51C
    std::array<WorldPolygonPacketStorageLifetime, 4> packet_lifetimes{
        WorldPolygonPacketStorageLifetime{packets[0]}, WorldPolygonPacketStorageLifetime{packets[1]},
        WorldPolygonPacketStorageLifetime{packets[2]}, WorldPolygonPacketStorageLifetime{packets[3]}};
    // Metadata for this existing typed array, not13 source allocations.
    // Last member: expires borrows before array payload destruction.
    WorldIndexedVertexStorageLifetime indexed_lifetime{indexed};
};
struct WorldPolygonRange
{
    WorldPolygonPacketPointer first; // independent NULL/owner/offset, retained after packet base clear
    std::uint32_t count{};
};
using WorldPolygonGroup = std::array<WorldPolygonRange, 4>;
// NEW repair (PORT-FIX-014), host metadata only, never a source store: the
// slots of the current61C50C allocation that 429880/429730 have written since
// 42E8D0 zero-filled it. 42E8D0 zero-fills the stamp WORD at+12 too, so while
// 61C508 is0 (the first world frame of the process, and after every16-bit
// wrap) the source stamp test alone takes such a never-written slot as already
// transformed in this frame and submits its zero XYZRHW.
class WorldTrackVertexWrites
{
    SourceAllocationRef allocation_;
    std::vector<std::uint8_t> written_;

  public:
    // Marks of `allocation`. Another identity is a new zero-filled 42E8D0
    // allocation (identities are never reused), so its marks start clear.
    std::span<std::uint8_t> marks(const SourceAllocationRef &allocation, std::size_t slots)
    {
        if (!(allocation == allocation_) || written_.size() != slots)
        {
            allocation_ = allocation;
            written_.assign(slots, 0);
        }
        return written_;
    }
    // Read-only query; false for an unmarked slot or another allocation.
    bool written(const SourceAllocationRef &allocation, std::size_t slot) const noexcept
    {
        return allocation == allocation_ && slot < written_.size() && written_[slot] != 0;
    }
};
// One long-lived state. Resource allocation replacement follows42E510/430;
// frame_stamp61C508 and model_view61C428/4E8/4F8 are outside that reset.
// Existing Session.frame_world_view owns61C594; light_vertices/light_vertex_offset
// own624530/624574; RaceSessionState owns screen_base61C5F0 plus retained
// screen_polygons and independent count61C604. No duplicate here.
struct WorldPolygonFrame
{
    std::unique_ptr<WorldPolygonFrameAllocations> allocations;
    WorldModelViewScratch model_view{};
    CameraMatrix bound_matrix{};                                           // 577A90, written only by427B40's44FBD0
    std::uint32_t lighting_stride{};                                       // 62497C, written by42A840/42A870
    std::uint32_t frame_stamp{}, remaining_first{}, spatial_batch_count{}; // 61C508/514/550
    std::uint32_t low_quality_initialized{};       // 57B41C, reset by42E7ED at level initialization only
    std::uint32_t source_word_61c5e8{};            // sole opaque PE-zero DWORD,42E5CA clears it
    std::uint32_t accepted_models{};               // 6249AC, reset42B04B; accepted parts/models increment it
    SourceTrackModelPointer active_track;          // 61C52C: incoming ECX at42ACB8
    SourceTrackPartPointer active_track_base_part; // 61C530: captured[636448]+44 at42ACBE
    WorldTransformedVertexCursor transformed;
    WorldIndexedVertexCursor indexed;
    std::array<WorldPolygonProducerList, 4> packets;
    std::array<WorldSpatialBatchList, 2> spatial;
    WorldPolygonClipStorage clip;
    std::array<WorldPolygonGroup, 4> groups{}; // 6243E0/624400/624420/624440
    std::array<std::uint32_t, 4> totals{};     // 6243C4/CC/D4/DC, published by42B200 only

    WorldTrackVertexWrites track_vertex_writes; // NEW repair (PORT-FIX-014), host metadata only
    WorldPolygonFrame();
    ~WorldPolygonFrame();
    WorldPolygonFrame(const WorldPolygonFrame &) = delete;
    WorldPolygonFrame &operator=(const WorldPolygonFrame &) = delete;
    WorldPolygonFrame(WorldPolygonFrame &&) = delete;
    WorldPolygonFrame &operator=(WorldPolygonFrame &&) = delete;

  private:
    std::shared_ptr<detail::WorldPolygonLevelLease> level_lease_;
    friend void attach_world_polygon_level(WorldPolygonFrame &);
    friend void detach_world_polygon_level(WorldPolygonFrame &) noexcept;
    friend WorldPolygonFrameTicket capture_world_polygon_level(const WorldPolygonFrame &);
    friend void validate_world_polygon_level(const WorldPolygonFrame &, const WorldPolygonFrameTicket &);
};
struct WorldPolygonFrameStoreObserver
{
    virtual ~WorldPolygonFrameStoreObserver() = default;
    // NEW synchronous read-only source-word observation. Throw retains writes.
    virtual void stored(std::uint32_t instruction, std::uint32_t address, const WorldPolygonFrame &) = 0;
};
// Host guards, separate from all recovered stores. Source packet/vertex bytes,
// counts and frame stamps survive detach. Call before destroying level models.
void attach_world_polygon_level(WorldPolygonFrame &);
void detach_world_polygon_level(WorldPolygonFrame &) noexcept;
WorldPolygonFrameTicket capture_world_polygon_level(const WorldPolygonFrame &);
void validate_world_polygon_level(const WorldPolygonFrameTicket &);
void validate_world_polygon_level(const WorldPolygonFrame &, const WorldPolygonFrameTicket &);
// Source resource clearing/allocation and427270 typed binding. Allocation bytes
// are initially zero because42E8D0 clears them. This is never a per-frame reset.
void allocate_world_polygon_frame(WorldPolygonFrame &, WorldPolygonFrameStoreObserver * = nullptr);
void reset_world_polygon_vertex_cursors(WorldPolygonFrame &); // full42E800
void reset_world_polygon_groups(WorldPolygonFrame &,
                                WorldPolygonFrameStoreObserver * = nullptr); // full42B100, retains totals
void begin_world_polygon_group(WorldPolygonFrame &, unsigned group,
                               WorldPolygonFrameStoreObserver * = nullptr); // full42B140
void finish_world_polygon_group(WorldPolygonFrame &, unsigned group,
                                WorldPolygonFrameStoreObserver * = nullptr);                        // full42B180
void publish_world_polygon_totals(WorldPolygonFrame &, WorldPolygonFrameStoreObserver * = nullptr); // full42B200
// Checked synchronous projections shared by runtime and diagnostic adapters.
std::span<WorldPolygonPacket> world_polygon_packet_pool(const WorldPolygonFrame &, unsigned pool);
std::span<WorldPolygonPacket> world_polygon_group_packets(const WorldPolygonFrame &, unsigned group, unsigned pool);
std::span<WorldPolygonPacket> world_polygon_alpha_packets(const WorldPolygonFrame &, unsigned pool);
std::array<std::span<WorldPolygonPacket>, 7> world_polygon_clip_lists(WorldPolygonFrame &);
// These runtime views require an attached level on its owning thread. They
// are synchronous borrows only; a deferred presentation must own its copies.
std::array<std::span<WorldPolygonPacket>, 9> world_polygon_submission_lists(WorldPolygonFrame &);
std::span<WorldPolygonVertex> world_polygon_remaining_vertices(WorldPolygonFrame &);
std::span<WorldPolygonVertex *> world_polygon_indexed_projection_vertices(const WorldPolygonFrame &);
} // namespace sgn::retail
