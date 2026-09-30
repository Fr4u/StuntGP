#pragma once
#include "sgn/race_session_platform.hpp"
#include "sgn/world_polygon_packet_binding.hpp"

namespace sgn::retail
{
// Actual five-DWORD records only. No WorldPolygonPacket shadow or payload copy.
// The sole clipped-output owner is platform.state().polygon_frame.clip.
void clip_source_world_polygon_427530(const std::shared_ptr<RaceSessionPlatform> &, WorldPolygonPacketPointer);
void clip_source_world_polygon_list_4277b0(const std::shared_ptr<RaceSessionPlatform> &, WorldPolygonPacketPointer,
                                           std::uint32_t count);
// Seven groups are captured only when reached, including fresh current-camera
// far stores. Source allocation payloads are not resolved for empty groups.
void clip_source_world_polygon_lists_4277d0(const std::shared_ptr<RaceSessionPlatform> &);
// Capture viewport once, then indexed -> clip -> remaining, lazily. Duplicate
// indexed identities are projected repeatedly; frame stamp belongs to caller.
void project_source_world_polygon_vertices_429a40(const std::shared_ptr<RaceSessionPlatform> &);
} // namespace sgn::retail
