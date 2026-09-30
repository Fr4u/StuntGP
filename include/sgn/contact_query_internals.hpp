#pragma once
#include "sgn/track_contact.hpp"
#include "sgn/contact_geometry_pointer.hpp"
#include <optional>
namespace sgn::retail::contact
{
// Additional fields of the existing single ContactCastState owner. Geometry
// pointers carry either a native index or an actual weak source allocation.
struct ContactQueryInternals
{
    ContactGeometryPointer plane_vertex, plane_normal;       // 49815C/160
    float endpoint_plane_distance{}, start_plane_distance{}; // 498168/16C
    Vector4 auxiliary{};                                     // 498188; retained by face candidates
    float candidate_distance{};                              // 4981C8; independent of outer closest distance
    std::uint32_t tree_tier{}, contact_kind{};               // 498240/5715CC
    // 401D40 source pointer publications, retained with the same contact owner.
    // These are normalized allocation identities/offsets, never payload copies.
    SourceAddressLocation track_coordinates_498230, track_vertices_498234, track_attributes_498238;
};
struct ContactQueryInternalsAudit
{
    bool auxiliary{};
};
} // namespace sgn::retail::contact
