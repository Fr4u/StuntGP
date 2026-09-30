#pragma once
#include <array>
#include <cstdint>

namespace sgn
{
// New window-local SDR presentation state. Channel-major R[256], G[256], B[256].
// It never aliases or initializes the source LightFadeState/ramp/last_gamma.
using SdrGammaRamp = std::array<std::uint16_t, 768>;
constexpr SdrGammaRamp identity_sdr_gamma() noexcept
{
    SdrGammaRamp result{};
    for (unsigned channel = 0; channel < 3; ++channel)
        for (unsigned i = 0; i < 256; ++i)
            result[channel * 256 + i] = static_cast<std::uint16_t>(i * 257u);
    return result;
}
} // namespace sgn
