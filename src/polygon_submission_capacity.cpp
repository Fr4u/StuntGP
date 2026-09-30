#include "sgn/polygon_submission_capacity.hpp"
#include "sgn/format_require.hpp"
#include <algorithm>
#include <limits>
#include <optional>

namespace sgn
{
namespace
{
using namespace retail;
using sgn::format_check::require;
std::size_t sum(std::size_t a, std::size_t b)
{
    require(b <= std::numeric_limits<std::size_t>::max() - a, "submission preflight size overflow");
    return a + b;
}
std::size_t product(std::size_t a, std::size_t b)
{
    require(!a || b <= std::numeric_limits<std::size_t>::max() / a, "submission preflight byte overflow");
    return a * b;
}
std::size_t payload(std::size_t indexed, std::size_t alpha, std::size_t indices)
{
    return sum(product(sum(indexed, alpha), sizeof(PolygonSubmissionVertex)), product(indices, sizeof(std::uint16_t)));
}
std::size_t inspect(const WorldPolygonPacket &p, const PolygonSubmissionInputs &in, bool sky, bool alpha,
                    bool needs_motion)
{
    const auto count = std::size_t(p.header.byte(0));
    require(count >= 3 && (!alpha || count <= 15), "submission preflight corner count outside qualified domain");
    require(count <= p.indices.size(), "submission preflight corner list outside owner");
    const auto id = std::uint32_t(p.header.byte(1));
    require(id < in.materials.size(), "submission preflight material outside owner");
    if (sky)
    {
        const std::uint32_t index = id - in.sky_material_base;
        require(index < in.sky.size(), "submission preflight sky motion outside owner");
    }
    else if (needs_motion)
    {
        require(in.materials[id].motion_index < in.motions.size(), "submission preflight motion outside owner");
    }
    for (std::size_t i = 0; i < count; ++i)
    {
        require(p.indices[i] < p.attributes.size(), "submission preflight attribute outside owner");
        const auto attribute = p.attributes.value(p.indices[i]);
        require(attribute.vertex < p.vertices.size(), "submission preflight transformed vertex outside owner");
    }
    return count;
}
template <class T> std::optional<std::vector<T>> replacement(const std::vector<T> &old, std::size_t required)
{
    if (required <= old.size())
        return std::nullopt;
    // Count constructor value-initializes appended payload. Copying existing
    // trivially represented words retains every bit, including unused UVs.
    std::vector<T> next(required);
    std::copy(old.begin(), old.end(), next.begin());
    return next;
}
} // namespace

PolygonSubmissionCapacityPlan plan_polygon_submission_capacity(const retail::PolygonSubmissionLists &lists,
                                                               const retail::PolygonSubmissionInputs &in,
                                                               const retail::PolygonSubmissionState &state,
                                                               PolygonSubmissionBudget budget)
{
    PolygonSubmissionCapacityPlan plan;
    for (const auto packets : lists.categories)
        plan.packet_visits = sum(plan.packet_visits, packets.size());
    for (const auto packets : lists.alpha)
        plan.packet_visits = sum(plan.packet_visits, packets.size());
    require(plan.packet_visits <= budget.packet_visits, "submission preflight packet budget exceeded");
    // Every category remains indexed even if quality/capability selects its
    // opaque fallback. Each source function resets its own material cache.
    for (std::size_t group = 0; group < lists.categories.size(); ++group)
    {
        const auto packets = lists.categories[group];
        const bool sky = group == 0;
        const bool multi = group >= 5 && in.multitexture_capability != 0 && in.quality != 0;
        // 433C50 binds record0 alternate even for a nonempty all-null list.
        require(!multi || packets.empty() || !in.materials.empty(),
                "submission preflight alternate material outside owner");
        std::uint32_t material = std::numeric_limits<std::uint32_t>::max();
        std::size_t vertices = 0, indices = 0;
        for (const auto &p : packets)
        {
            if (!p.header)
                continue;
            const auto count = inspect(p, in, sky, false, !sky && !multi);
            const auto id = std::uint32_t(p.header.byte(1));
            if (id != material)
            {
                vertices = 0;
                indices = 0;
                material = id;
            }
            vertices = sum(vertices, count);
            indices = sum(indices, product(count - 2, 3));
            require(vertices <= 65536, "submission preflight WORD vertex domain exceeded");
            plan.indexed_vertices = std::max(plan.indexed_vertices, vertices);
            plan.indices = std::max(plan.indices, indices);
        }
    }
    for (const auto packets : lists.alpha)
        for (const auto &p : packets)
        {
            if (p.header)
                plan.alpha_vertices = std::max(plan.alpha_vertices, inspect(p, in, false, true, true));
        }
    const auto indexed = std::max(state.indexed_vertices.size(), plan.indexed_vertices);
    const auto alpha = std::max(state.alpha_vertices.size(), plan.alpha_vertices);
    const auto indices = std::max(state.indices.size(), plan.indices);
    require(indexed <= state.indexed_vertices.max_size() && alpha <= state.alpha_vertices.max_size() &&
                indices <= state.indices.max_size(),
            "submission preflight vector maximum exceeded");
    plan.growth_mask = (indexed > state.indexed_vertices.size() ? 1u : 0u) |
                       (alpha > state.alpha_vertices.size() ? 2u : 0u) | (indices > state.indices.size() ? 4u : 0u);
    plan.retained_payload_bytes = payload(indexed, alpha, indices);
    const auto staged = payload((plan.growth_mask & 1u) ? indexed : 0, (plan.growth_mask & 2u) ? alpha : 0,
                                (plan.growth_mask & 4u) ? indices : 0);
    plan.transaction_payload_bytes =
        sum(payload(state.indexed_vertices.size(), state.alpha_vertices.size(), state.indices.size()), staged);
    require(plan.retained_payload_bytes <= budget.retained_payload_bytes,
            "submission preflight retained payload budget exceeded");
    require(plan.transaction_payload_bytes <= budget.transaction_payload_bytes,
            "submission preflight transaction payload budget exceeded");
    return plan;
}

PolygonSubmissionCapacityPlan prepare_polygon_submission_capacity(const retail::PolygonSubmissionLists &lists,
                                                                  const retail::PolygonSubmissionInputs &in,
                                                                  retail::PolygonSubmissionState &state,
                                                                  PolygonSubmissionBudget budget)
{
    const auto plan = plan_polygon_submission_capacity(lists, in, state, budget);
    if (!plan.growth_mask)
        return plan;
    auto indexed = replacement(state.indexed_vertices, plan.indexed_vertices);
    auto alpha = replacement(state.alpha_vertices, plan.alpha_vertices);
    auto indices = replacement(state.indices, plan.indices);
    static_assert(noexcept(state.indexed_vertices.swap(state.indexed_vertices)));
    static_assert(noexcept(state.indices.swap(state.indices)));
    if (indexed)
        state.indexed_vertices.swap(*indexed);
    if (alpha)
        state.alpha_vertices.swap(*alpha);
    if (indices)
        state.indices.swap(*indices);
    return plan;
}
void prepare_polygon_submission_ui_capacity(retail::PolygonSubmissionState &state, PolygonSubmissionBudget budget)
{
    const auto count = std::max<std::size_t>(8, state.alpha_vertices.size());
    const auto old = payload(state.indexed_vertices.size(), state.alpha_vertices.size(), state.indices.size());
    const auto retained = payload(state.indexed_vertices.size(), count, state.indices.size());
    const auto transaction =
        sum(old, count > state.alpha_vertices.size() ? product(count, sizeof(retail::PolygonSubmissionVertex)) : 0);
    require(retained <= budget.retained_payload_bytes, "UI scratch retained payload budget exceeded");
    require(transaction <= budget.transaction_payload_bytes, "UI scratch transaction payload budget exceeded");
    auto next = replacement(state.alpha_vertices, count);
    if (next)
        state.alpha_vertices.swap(*next);
}
} // namespace sgn
