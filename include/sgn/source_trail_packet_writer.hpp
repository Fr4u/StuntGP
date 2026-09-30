#pragma once
#include "sgn/retail_trail_packets.hpp"
#include "sgn/source_transport_storage.hpp"
namespace sgn::retail
{
// Actual4342E0/4343F0 on the source bank pointers5DA24C..260. No preview
// cursor, reconstructed world or W replacement. Input staging must not alias
// the output allocation. The billboard borrows current camera+50 owners.
bool append_source_trail_quad_4342e0(SourceSpritePacketStorage &, const SourceTrailPolygon &);
bool append_source_trail_billboard_4343f0(SourceSpritePacketStorage &, const SourceTrailPolygon &, LightCameraView);
bool append_source_trail_quad_4342e0(SourceSpritePacketStorage &, const SourceTransportStorage<SourceTrailPolygon> &);
bool append_source_trail_billboard_4343f0(SourceSpritePacketStorage &,
                                          const SourceTransportStorage<SourceTrailPolygon> &, LightCameraView);
} // namespace sgn::retail
