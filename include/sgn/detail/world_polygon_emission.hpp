#pragma once
#include "sgn/retail_world_polygon_producer.hpp"

namespace sgn::retail::detail
{
enum class PacketEmissionPath
{
    part_clipped,
    part_inside,
    spatial_clipped,
    spatial_inside
};
// Invocation-local registers and borrowed diagnostic context, never retained.
struct PacketEmissionCursors
{
    WorldPolygonProducerList &output_opaque;
    WorldPolygonProducerList &output_alpha;
    WorldPolygonPacketPointer opaque, alpha;
    WorldPacketEmissionObserver *observer;
    PacketEmissionCursors(WorldPolygonProducerList &a, WorldPolygonProducerList &b, WorldPacketEmissionObserver *o)
        : output_opaque(a), output_alpha(b), observer(o)
    {
        a.require_owner_thread();
        b.require_owner_thread();
        opaque = a.current();
        alpha = b.current();
    }
    void observe(std::uint32_t pc, WorldPacketWriteKind kind, bool is_alpha, const WorldPolygonPacketPointer &p)
    {
        if (observer)
            observer->stored({pc, kind, is_alpha, p}, output_opaque, output_alpha);
    }
    void publish(PacketEmissionPath, bool empty);
};
void emit_packet_polygon(const WorldPolygonPartSource &, std::size_t header, std::span<WorldPolygonVertex>,
                         const VehicleVector &, PacketEmissionCursors &, PacketEmissionPath);
} // namespace sgn::retail::detail
