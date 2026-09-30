#include "sgn/track_spline_source.hpp"
#include "sgn/contact_rail.hpp"
#include "sgn/vehicle_ai_automatic.hpp"
#include <cstdint>

// NEW R18 (DESIGN A1): decoded file form of TrackSplineSource for tests, tools
// and the preview START path until WP-F retirement. Each member is exactly the
// former SplineSet/RaceSession expression it replaces; nothing is cached.
namespace sgn::retail
{
namespace
{
std::uint32_t le32(std::span<const std::uint8_t> bytes, std::size_t at)
{
    return std::uint32_t(bytes[at]) | (std::uint32_t(bytes[at + 1]) << 8) | (std::uint32_t(bytes[at + 2]) << 16) |
           (std::uint32_t(bytes[at + 3]) << 24);
}
} // namespace
CameraSplineBinding DecodedTrackSplines::find(std::uint32_t id) const
{
    return splines_.find(id);
}
CameraSplineBinding DecodedTrackSplines::first() const
{
    // Former descriptors().front() + find(front.id): the first match of
    // descriptor0's own ID is descriptor0.
    const auto descriptors = splines_.descriptors();
    if (descriptors.empty())
        return {};
    return splines_.find(descriptors.front().id);
}
void DecodedTrackSplines::initialize_rail(RailState &state) const
{
    sgn::retail::initialize_rail(state, splines_, raw_texture_labels_); // 4192F0/418B10 decoded form
}
void DecodedTrackSplines::publish_ai_routes(AiRouteBindings &routes) const
{
    routes = ai_spline_routes(splines_); // 405260 route selection, decoded form
}
std::optional<std::uint32_t> DecodedTrackSplines::reset_spacing_word() const
{
    // Former RaceSession::reset_parameters: file word 0x148+[0x78]+0x24.
    if (pmd_.size() < 0x7c)
        throw FormatError("decoded track splines carry no PMD header for [624474]+0x24");
    const auto at = 0x148 + std::size_t(le32(pmd_, 0x78)) + 0x24;
    if (at <= pmd_.size() && pmd_.size() - at >= 4)
        return le32(pmd_, at);
    return std::nullopt;
}
} // namespace sgn::retail
