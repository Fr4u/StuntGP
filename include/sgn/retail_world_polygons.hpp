#pragma once
#include "sgn/source_world_span.hpp"
#include "sgn/source_clip_current_pointer.hpp"
#include "sgn/retail_world_polygon_views.hpp"
#include "sgn/retail_viewport.hpp"
#include <cstring>
#include <span>

namespace sgn::retail
{
// Complete32-byte transformed vertex storage, including retained words.
// Float X/Y/Z at0/4/8; clip mask is lowWORD10; camera depth at1C.
struct WorldPolygonVertex
{
    std::array<std::uint32_t, 8> words{};
};
static_assert(sizeof(WorldPolygonVertex) == 32);

// Native borrowed view of the source five-DWORD record: header, indices,
// attributes, transformed vertices, common inside mask. A null header is the
// source427520 rejection marker. No copied source pointers enter native code.
struct WorldPolygonPacket
{
    WorldPolygonHeaderRef header{};
    std::span<const std::uint16_t> indices;
    WorldPolygonAttributeView attributes;
    std::span<WorldPolygonVertex> vertices;
    std::uint32_t inside_mask{};
};

// Source allocations6244C8/CC/D0/D4 are borrowed from one frame owner.
// The four cursor fields retain working pointers6244D8/DC/E0/E4.
// Reset427240 copies the current base identities; allocation bytes remain intact. Vertices may alias the source
// main transformed-vertex allocation at byte offset0x60000 (393216 bytes,427270).
struct WorldPolygonClipStorage
{
    SourceWorldSpan<WorldPolygonAttribute> attributes;
    SourceWorldSpan<WorldPolygonHeader> headers;
    SourceWorldSpan<std::uint16_t> unused_indices;
    SourceWorldSpan<WorldPolygonVertex> vertices;
    SourceClipCurrentPointer attribute_count{}, header_count{}, unused_index_count{}, vertex_count{};
    std::array<std::uint16_t, 16> sequential_indices{}; // 6244A8, initialized by427270
};

// Test-only per-byte undefined-source provenance. It never selects geometry,
// branches, addresses, stored values or the number of emitted vertices.
struct WorldPolygonPacketAudit
{
    const std::array<std::uint8_t, 8> *header{};
    std::span<const std::array<std::uint8_t, 16>> attributes;
    std::span<const std::array<std::uint8_t, 32>> vertices;
};
struct WorldPolygonClipAudit
{
    std::span<std::array<std::uint8_t, 8>> headers;
    std::span<std::array<std::uint8_t, 16>> attributes;
    std::span<std::array<std::uint8_t, 32>> vertices;
};

void reset_world_polygon_clip_storage(WorldPolygonClipStorage &);      // 427240
void initialize_world_polygon_clip_storage(WorldPolygonClipStorage &); // 427270 typed allocation binding
void reject_world_polygon(WorldPolygonPacket &) noexcept;              // 427520
void clip_world_polygon(WorldPolygonPacket &, WorldPolygonClipStorage &, float near,
                        const WorldPolygonPacketAudit *input_audit = nullptr,
                        WorldPolygonClipAudit *output_audit = nullptr); // 427530 +4272A0
void clip_world_polygon_list(std::span<WorldPolygonPacket>, WorldPolygonClipStorage &, float near,
                             WorldPolygonClipAudit *output_audit = nullptr); // 4277B0

// Seven ordered lists:6243E0,624400,624408,624420,624428,624440,624448.
// Their backing records may alias, as in the source. far is the live current
// camera+19C owner and receives the original10,000,000/500,000 stores.
void clip_world_polygon_lists(std::array<std::span<WorldPolygonPacket>, 7>, WorldPolygonClipStorage &, float near,
                              float &far, WorldPolygonClipAudit *output_audit = nullptr); // full4277D0

// Existing429A40 arithmetic only; source owner orchestration captures the
// four viewport fields once and calls this leaf for each reached identity.
void project_world_polygon_vertex(WorldPolygonVertex &, const ViewportState &captured_view);

// Full429A40. Preserve pointer order and duplicate references; do not dedupe.
// Then process the clipped and remaining contiguous source ranges in order.
void project_world_polygon_vertices(std::span<WorldPolygonVertex *const> indexed, std::span<WorldPolygonVertex> clipped,
                                    std::span<WorldPolygonVertex> remaining,
                                    const ViewportState &); // writes only X,Y,depth; retains all other bytes

class WorldIndexedVertexCursor;
// Source DWORD indexed payload is resolved one element at a time; no pointer-array snapshot.
void project_world_polygon_indexed_cursor(const WorldIndexedVertexCursor &, std::span<WorldPolygonVertex> clipped,
                                          std::span<WorldPolygonVertex> remaining, const ViewportState &);
} // namespace sgn::retail
