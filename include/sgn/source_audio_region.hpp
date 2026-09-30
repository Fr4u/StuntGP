#pragma once
#include "sgn/audio_environment.hpp"
#include "sgn/retail_camera.hpp"
namespace sgn::retail
{
// 4278A0 with documented PORT-FIX-001: initialize the cross direction's W to0.
// All other source arithmetic, stores, authored V/N.W and plane sign tests remain.
std::uint32_t source_audio_environment_contains_4278a0(const AudioEnvironmentRegion &, const CameraVector &) noexcept;
} // namespace sgn::retail
