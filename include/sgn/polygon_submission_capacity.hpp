#pragma once
#include "sgn/retail_polygon_submission.hpp"

namespace sgn
{
// New host limits, not recovered allocation sizes. Payload means vector size
// times element size; allocator metadata/reserved capacity are not included.
struct PolygonSubmissionBudget
{
    std::size_t retained_payload_bytes;
    std::size_t transaction_payload_bytes; // old live payload plus staged replacements
    std::size_t packet_visits;             // all nine spans, including null-header entries
};
inline constexpr PolygonSubmissionBudget desktop_polygon_submission_budget{8u * 1024u * 1024u, 16u * 1024u * 1024u,
                                                                           1'000'000u};

struct PolygonSubmissionCapacityPlan
{
    std::size_t indexed_vertices{}, alpha_vertices{}, indices{}; // required high water marks
    std::size_t retained_payload_bytes{}, transaction_payload_bytes{}, packet_visits{};
    std::uint32_t growth_mask{}; // 1 indexed vertices,2 alpha vertices,4 indices
};
// Entire full433F40 preflight: exact same-material runs in seven independent
// indexed lists and per-polygon demand in both alpha lists. Null headers do not
// split a run. Checks every reachable owner bound; no source calls or writes.
// All borrowed owners and quality/capability values must remain stable from
// preflight through submission. This is not a validated token for later reuse.
PolygonSubmissionCapacityPlan plan_polygon_submission_capacity(const retail::PolygonSubmissionLists &,
                                                               const retail::PolygonSubmissionInputs &,
                                                               const retail::PolygonSubmissionState &,
                                                               PolygonSubmissionBudget);

// Repeats preflight, allocates every needed replacement, then commits with
// noexcept swaps. On any validation/allocation failure the original pointers,
// sizes, capacity, bytes and helper words remain unchanged. No-growth vectors
// are not touched. Existing words survive; only appended elements are zeroed
// as an explicit NEW HOST initialization policy. Never shrinks or calls source.
PolygonSubmissionCapacityPlan prepare_polygon_submission_capacity(const retail::PolygonSubmissionLists &,
                                                                  const retail::PolygonSubmissionInputs &,
                                                                  retail::PolygonSubmissionState &,
                                                                  PolygonSubmissionBudget);

// New host preparation for the shared UI/border scratch. Keeps at least eight
// actual elements, preserves retained bits, and checks the same payload budgets
// before committing a replacement. Does not call source or shrink any vector.
void prepare_polygon_submission_ui_capacity(retail::PolygonSubmissionState &, PolygonSubmissionBudget);
} // namespace sgn
