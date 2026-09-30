#pragma once
#include "sgn/retail_light_effects.hpp"
#include "sgn/race_session_platform.hpp"

namespace sgn::retail
{
// Source list writer42D9B0, exact42D870 numeric prefix and existing4320C0
// gamma receiver. Undefined source-stack relative.W retains destination bits
// under an explicit NEW HOST policy; it is not a recovered scalar value.
// Caller supplies actual current camera/overlay owners from this platform.
void prepare_source_light_view_42d9b0(const std::shared_ptr<RaceSessionPlatform> &, LightCameraView,
                                      LightGammaDevice *);
// Complete42DCF0 and44EB70/44FB00. Writes the actual phase/cursor/direction
// owners and the one source61C3A4 rotation scratch. Does not run cameras.
void prepare_source_animated_light_view_42dcf0(const std::shared_ptr<RaceSessionPlatform> &, LightCameraView,
                                               LightGammaDevice *);
// Checked synchronous views used by actual PMD shading. Returned spans may
// not escape the invocation retaining the platform. No payload copies.
std::span<std::uint8_t> source_light_record(const std::shared_ptr<RaceSessionPlatform> &, const SourceSelectedLight &);
SourceSelectedLight source_light_pointer_from_token(const std::shared_ptr<RaceSessionPlatform> &, std::uint32_t);
std::uint32_t source_light_pointer_token(const std::shared_ptr<RaceSessionPlatform> &, const SourceSelectedLight &);
} // namespace sgn::retail
