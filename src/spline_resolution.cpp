#include "sgn/spline_resolution.hpp"
#include "sgn/race_session.hpp"
#include "sgn/source_pmd_spline.hpp"
#include "sgn/vehicle_ai_automatic.hpp"

// NEW R18 (DESIGN A1) host resolution of the two disjoint spline pointer
// representations. No source instruction is executed here: the source reads
// [binding]+0/+4/+8/+18 directly from the live track allocation, and the native
// math receives the same bytes as a call-scoped view. Nothing is cached.
namespace sgn::retail
{
namespace
{
[[noreturn]] void fail(std::uint32_t site, const char *reason)
{
    throw SourceAllocationError(site, reason);
}
} // namespace
SplineView resolve_source_spline(const SourceCameraSplineRef &ref, std::uint32_t site)
{
    const auto platform = ref.platform().lock();
    if (!platform)
        fail(site, "source spline platform expired");
    platform->require_owner_thread();
    auto &scene = platform->source_scene_resources();
    const auto &models = scene.models();
    // 624474/624498: the live track globals published by 42EE50..42EEBE.
    const auto &track = models.source_loading_state().track;
    if (!track.splines.allocation || !(ref.allocation() == track.splines.allocation))
        fail(site, "source spline binding is not in the live 624474 track allocation");
    if (ref.offset() < track.splines.offset || (ref.offset() - track.splines.offset) % 28u != 0)
        fail(site, "source spline binding is not a 28-byte 624474 descriptor");
    const auto index = (ref.offset() - track.splines.offset) / 28u;
    if (index >= track.spline_count)
        fail(site, "source spline binding lies beyond 624498 descriptors");
    // [636448]: the track PMD descriptor whose file holds 624474.
    const auto handle = scene.model_handle(3);
    if (!handle)
        fail(site, "source spline resolution reached a null [636448] track PMD");
    const auto model = models.source_at(*handle);
    if (!(model.allocation() == ref.allocation()))
        fail(site, "[636448] track PMD is not the 624474 allocation");
    if (!(model.track().spline_descriptor(index).location() == SourcePmdLocation{ref.allocation(), ref.offset()}))
        fail(site, "624474 descriptor identity disagrees with the [636448] PMD spline section");
    auto view = SourcePmdSplineSet(model).view(index);
    if (!view)
        fail(site, "source spline descriptor has no current PMD view");
    return *view;
}
std::optional<SplineView> resolve_spline(const CameraSplineBinding &binding, std::uint32_t site)
{
    if (const auto *source = binding.source())
        return resolve_source_spline(*source, site);
    return binding.native();
}
std::array<SplineView, 3> resolve_ai_routes(const AiRouteBindings &routes, std::uint32_t site)
{
    if (!routes)
        fail(site, "AI route cache 49A158/15C/160 is not initialized");
    if (!routes.source_form())
        return *routes;
    std::array<SplineView, 3> result{};
    for (unsigned i = 0; i < 3; ++i)
    {
        const auto view = resolve_spline(routes.source(i), site);
        if (!view)
            fail(site, "AI route cache holds a null route");
        result[i] = *view;
    }
    return result;
}
} // namespace sgn::retail
