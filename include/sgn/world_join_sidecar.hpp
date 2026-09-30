#pragma once
#include "sgn/world_join_retention.hpp"
#include "sgn/world_join_sha256.hpp"
#include <iosfwd>

namespace sgn::diagnostics
{
struct WorldJoinSidecarView
{
    WorldJoinLookupStatus lookup{WorldJoinLookupStatus::not_retained};
    std::optional<std::uint64_t> accepted_serial, record_tick;
    const WorldJoinResult *result{}; // Synchronous diagnostic borrow only.
    bool core_matches_displayed{};
    WorldJoinRecipeIdentity matched_identity;
    WorldJoinSelection selection;
    WorldJoinRetentionCounters counters;
};
template <class Record>
WorldJoinSidecarView world_join_sidecar_view(const WorldJoinRetention<Record> &table,
                                             const ImmutablePolygonSubmission &displayed) noexcept
{
    const auto found = table.lookup(displayed);
    WorldJoinSidecarView out;
    out.lookup = found.status;
    out.selection = table.selection();
    if (found.status != WorldJoinLookupStatus::foreign_thread)
        out.counters = table.counters();
    if (found.entry)
    {
        out.accepted_serial = found.entry->serial;
        out.record_tick = found.entry->tick;
        out.result = &found.entry->record->result();
        out.core_matches_displayed = found.entry->record->matches_displayed(displayed);
        out.matched_identity = found.entry->identity;
    }
    return out;
}
// Uses the existing displayed-recipe writer exactly once, then fingerprints
// the closed file and checks that it equals the streamed bytes. Exclusive files.
WorldJoinFingerprint write_bound_displayed_recipe(const std::filesystem::path &, const ImmutablePolygonSubmission &);
// Returns true only for an actual-front match with a DERIVED_CHECKED core.
// Missing/incomplete diagnostics get an explicit status and no geometry claims.
bool write_world_join_sidecar(std::ostream &, const WorldJoinFingerprint &, const ImmutablePolygonSubmission &,
                              const WorldJoinSidecarView &);
bool export_displayed_world_join(const std::filesystem::path &recipe_path, const std::filesystem::path &sidecar_path,
                                 const ImmutablePolygonSubmission &, const WorldJoinSidecarView &);
} // namespace sgn::diagnostics
