#pragma once
#include "sgn/retail_world_polygon_frame.hpp"

namespace sgn::retail
{
// Complete source-PMD leaves at their actual42A220 call sites. The records
// borrow the real61C518/61C51C source allocations; count is the already captured
// caller's normalized eight-byte pointer difference, not a record-container size.
// They use the frame's independent61C52C descriptor,61C530 part and61C510
// transformed cursor, and publish the existing packet pair0/1 or2/3.
// No native ModelResource/ModelSpatialData or host packet mirror is constructed.
// Caller keeps the SAME bank, frame, PMD owners and source platform alive/stable
// on their owner thread. Diagnostic observers may read/throw, never mutate or
// retire these owners. Prior payload/count stores survive a later exception.
void emit_source_world_spatial_clipped_429ea0(SourceAllocationBank &, WorldPolygonFrame &,
                                              const WorldSpatialRecords &captured_records, std::uint32_t captured_count,
                                              WorldPacketEmissionObserver * = nullptr);
void emit_source_world_spatial_inside_42a090(SourceAllocationBank &, WorldPolygonFrame &,
                                             const WorldSpatialRecords &captured_records, std::uint32_t captured_count,
                                             WorldPacketEmissionObserver * = nullptr);
// Complete42A220: clipped call first, then fresh inside count/base capture.
// This is not a resume API for a failed emitter and does not run transforms,
// append the42AC70 tail, or activate the incomplete warm scene entry.
void emit_source_world_spatial_polygons_42a220(SourceAllocationBank &, WorldPolygonFrame &,
                                               WorldPacketEmissionObserver * = nullptr);
} // namespace sgn::retail
