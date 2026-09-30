#pragma once
#include "sgn/retail_spline.hpp"
#include "sgn/source_camera_spline.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace sgn::retail
{
struct RailState;
class AiRouteBindings;
class RaceSessionPlatform;
// NEW R18 (DESIGN A1) track-level spline owner seen by the native 418A90
// modules. Set queries return identities (CameraSplineBinding), never a
// copied descriptor; readers resolve them per call with spline_resolution.hpp.
// Writers (initialize_rail, publish_ai_routes) store only identities of the
// same live owner into the persistent world state.
class TrackSplineSource
{
  public:
    virtual ~TrackSplineSource() = default;
    virtual CameraSplineBinding find(std::uint32_t id) const = 0;        // 428800 first match, empty if absent
    virtual CameraSplineBinding first() const = 0;                       // [624474] descriptor 0 (reset spline)
    virtual void initialize_rail(RailState &) const = 0;                 // full 4192F0 including tail 418B10
    virtual void publish_ai_routes(AiRouteBindings &) const = 0;         // 405260 route publication: 3/4/5, fallback 1
    virtual std::optional<std::uint32_t> reset_spacing_word() const = 0; // [624474]+0x24, absent outside the file
};
// The single source world: weak platform only. find = 428800 on the live
// track allocation; initialize_rail writes the platform's sole 4D5C9C/474670.
class SourceTrackSplines final : public TrackSplineSource
{
    std::weak_ptr<RaceSessionPlatform> platform_;

  public:
    explicit SourceTrackSplines(std::weak_ptr<RaceSessionPlatform> platform) : platform_(std::move(platform)) {}
    CameraSplineBinding find(std::uint32_t id) const override;
    CameraSplineBinding first() const override;
    void initialize_rail(RailState &) const override;
    void publish_ai_routes(AiRouteBindings &) const override;
    std::optional<std::uint32_t> reset_spacing_word() const override;
};
// Decoded file form for tests, tools and the preview START path until its
// retirement (WP-F). Borrows the caller's SplineSet, labels and (optional)
// PMD file bytes; owns nothing. Without the PMD bytes reset_spacing_word()
// throws instead of inventing a value.
class DecodedTrackSplines final : public TrackSplineSource
{
    const SplineSet &splines_;
    std::span<const std::string> raw_texture_labels_;
    std::span<const std::uint8_t> pmd_;

  public:
    DecodedTrackSplines(const SplineSet &splines, std::span<const std::string> raw_texture_labels,
                        std::span<const std::uint8_t> pmd = {})
        : splines_(splines), raw_texture_labels_(raw_texture_labels), pmd_(pmd)
    {
    }
    CameraSplineBinding find(std::uint32_t id) const override;
    CameraSplineBinding first() const override;
    void initialize_rail(RailState &) const override;
    void publish_ai_routes(AiRouteBindings &) const override;
    std::optional<std::uint32_t> reset_spacing_word() const override;
};
} // namespace sgn::retail
