#pragma once
#include "sgn/contact_cast.hpp"
#include "sgn/contact_query_internals.hpp"
namespace sgn::retail::contact
{
namespace query_detail
{
// Shared source 420F80, 420C40 and 420CA0. All wrappers borrow the same state.
void select_polygon_list(const TrackGeometry &, ContactCastState &);
bool test_polygon(const TrackGeometry &, solver::Workspace &, ContactCastState &, std::uint32_t,
                  ContactCastPayloadAudit *, ContactQueryInternalsAudit *, solver::PayloadAudit *);
bool choose_closest(const TrackGeometry &, solver::Workspace &, ContactCastState &, ContactCastPayloadAudit *,
                    ContactQueryInternalsAudit *, solver::PayloadAudit *);
} // namespace query_detail
// Full420FE0 and persistent candidate writes. The query owns its existing
// fields and internals; work borrows the shared motion and normal owners.
std::uint32_t cast_track_sphere(const TrackGeometry &, solver::Workspace &, ContactCastState &,
                                ContactCastPayloadAudit *audit = nullptr,
                                ContactQueryInternalsAudit *internals_audit = nullptr,
                                solver::PayloadAudit *payload = nullptr);
} // namespace sgn::retail::contact
