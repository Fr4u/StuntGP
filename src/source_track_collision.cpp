#include "sgn/source_track_collision.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/scene_resources.hpp"
#include <algorithm>
#include <vector>

namespace sgn::retail
{
namespace
{
constexpr const char *diverge = "animated track geometry overlaps collision; decoded collision would diverge";
}
void require_static_track_collision(const SourcePmdTrackView &track, const contact::TrackGeometry &g)
{
    const auto model = track.model();
    // Absolute allocation offsets of the relocated collision ranges (42EDDC
    // [+8] vertices, 42EDF4 [+C] normals) and of the first generated normal
    // (42EF9E [624490] = descriptor+28 + [0xE4]*16, the 426ED0 cursor origin).
    const std::uint64_t vertex_base = track.collision_vertices().location().offset;
    const std::uint64_t normal_base = track.collision_normals().location().offset;
    const std::uint64_t generated = track.generated_normals().location().offset;
    // Reachable polygons only: every consumer enters a polygon list through
    // find_polygon_list (420F80/420F00) from these roots. g is already validated.
    std::vector<std::uint64_t> referenced;
    std::vector<std::uint8_t> seen_node(g.nodes.size()), seen_polygon(g.polygons.size());
    std::vector<std::uint32_t> pending{g.roots[0], g.roots[1]};
    while (!pending.empty())
    {
        const auto at = pending.back();
        pending.pop_back();
        if (seen_node.at(at))
            continue;
        seen_node[at] = 1;
        const auto &node = g.nodes.at(at);
        for (unsigned j = 0; j < 8; ++j)
        {
            if (node.child[j])
            {
                pending.push_back(g.roots[0] + node.child[j]);
                continue;
            }
            for (std::size_t entry = node.leaf[j];; ++entry)
            {
                const auto id = g.polygon_lists.at(entry);
                if (id == 0xffff)
                    break;
                if (seen_polygon.at(id))
                    continue;
                seen_polygon[id] = 1;
                const auto &p = g.polygons.at(id);
                if (normal_base + 16ull * p.normal >= generated)
                    throw FormatError(diverge);
                for (unsigned k = 0; k < p.vertex_count; ++k)
                    referenced.push_back(vertex_base + 16ull * g.indices.at(std::size_t(p.first_index) + k));
            }
        }
    }
    std::sort(referenced.begin(), referenced.end());
    referenced.erase(std::unique(referenced.begin(), referenced.end()), referenced.end());
    // Writers of track vertices after load, all through descriptor+48 (LOD0):
    //  - 426260/425DF0: part index 0.. when its [624484]+64i+30 kind is 1..3;
    //    stores vertices [0,2*side), side=(WORD+24>>1)*2+2 (425FE5..4260F9).
    //  - 426ED0 index 1..count-1 (426EEB/426F04): kinds 1..3 deform stores
    //    2i,2i+1 and back+2i,back+2i+1, rows<=(WORD+24>>1)+1, back=WORD+28>>1
    //    (4266A7..4266AF/426789..426791). Other kinds write poses/coordinates.
    // Every part p>0 is checked regardless of its current record kind; the
    // extent is the union of the declared range and both write extents.
    const auto parts = model.part_count(); // 426EE7/426288 descriptor+24
    for (std::uint32_t p = 0; p < parts; ++p)
    {
        if (p == 0)
        {
            const auto kind = track.texture_index_bytes().field<std::uint32_t>(0x30).value(0x4262b1);
            if (kind < 1 || kind > 3)
                continue; // 426260 prepares part 0 only for kinds 1..3; 426ED0 starts at 1
        }
        const auto part = model.part(0, p);
        const auto vertices = part.vertices();
        const std::uint64_t start = vertices.location().offset, declared = vertices.size();
        const std::uint64_t half = part.attribute_count().value() >> 1;
        const std::uint64_t written = std::max({declared, 4 * half + 4, (declared >> 1) + 2 * (half + 1)});
        const auto first = std::lower_bound(referenced.begin(), referenced.end(), start);
        if (first != referenced.end() && *first < start + 16 * written)
            throw FormatError(diverge);
    }
}
const contact::TrackGeometry &SourceTrackCollision::get(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    auto &scene = platform.source_scene_resources();
    const auto key = scene.models().source_loading_state().track.collision; // 624480 (42EDA1)
    if (decoded_ && !(decoded_->key == key))
        decoded_.reset(); // another level: rebuild
    // Any failure below drops the copy before the exception propagates (no
    // catch): a stale, released or foreign copy is never served.
    struct Admission
    {
        std::optional<Decoded> &decoded;
        bool admitted{};
        ~Admission()
        {
            if (!admitted)
                decoded.reset();
        }
    } admission{decoded_};
    if (!key.allocation)
        throw FormatError("source track collision [624480] is NULL");
    const auto handle = scene.model_handle(3); // 636448, the scene track slot (426275, race_session.cpp:209)
    if (!handle)
        throw FormatError("source track collision requires the scene track model");
    // source_at checks the actual bank generation: after 42EAA0 this throws and
    // the admission guard drops the copy of the released level.
    const auto track = scene.models().source_at(*handle).track();
    if (!(track.collision_descriptor().location() == key))
        throw FormatError("[624480] does not name the scene track's collision descriptor");
    if (!decoded_)
    {
        auto geometry = contact::decode_track_collision(track);
        require_static_track_collision(track, geometry);
        decoded_.emplace(Decoded{key, std::move(geometry)});
    }
    admission.admitted = true;
    return decoded_->geometry;
}
} // namespace sgn::retail
