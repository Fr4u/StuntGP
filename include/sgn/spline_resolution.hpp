#pragma once
#include "sgn/retail_spline.hpp"
#include "sgn/source_camera_spline.hpp"
#include <array>
#include <cstdint>
#include <optional>

namespace sgn::retail
{
class AiRouteBindings;
// NEW R18 (DESIGN A1) reader resolution. Every returned SplineView is a
// call-scoped view of the live owner: valid only for the synchronous caller,
// never stored into persistent state, never copied into another owner.
// Persistent writers publish identities through TrackSplineSource instead.
// `site` is the source function/instruction reported by a failure; it selects
// no behaviour.
//
// Native alternative: returned unchanged (including an empty optional).
// Source alternative: the live 624474+28*index descriptor of the current track
// PMD, borrowed with its checked spline point section as storage.
std::optional<SplineView> resolve_spline(const CameraSplineBinding &, std::uint32_t site);
// Requires the live platform, the current track allocation 624474, a
// descriptor-aligned offset and index<624498. Throws SourceAllocationError(site).
SplineView resolve_source_spline(const SourceCameraSplineRef &, std::uint32_t site);
// 49A158/15C/160 in one representation. Throws SourceAllocationError(site)
// when the cache (or any source route) is unbound.
std::array<SplineView, 3> resolve_ai_routes(const AiRouteBindings &, std::uint32_t site);
} // namespace sgn::retail
