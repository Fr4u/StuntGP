#include "sgn/source_audio_region.hpp"
#include <cstddef>
extern "C" std::uint32_t sgn_source_audio_contains_4278a0(const void *, const void *) noexcept;
namespace sgn::retail
{
std::uint32_t source_audio_environment_contains_4278a0(const AudioEnvironmentRegion &region,
                                                       const CameraVector &point) noexcept
{
    static_assert(sizeof(AudioEnvironmentRegion) == 72 && sizeof(VehicleVector) == 16);
    static_assert(offsetof(AudioEnvironmentRegion, position) == 8 && offsetof(AudioEnvironmentRegion, scale) == 24);
    static_assert(offsetof(AudioEnvironmentRegion, v) == 40 && offsetof(AudioEnvironmentRegion, n) == 56);
    static_assert(sizeof(CameraVector) == 16 && offsetof(CameraVector, y) == 4 && offsetof(CameraVector, z) == 8);
    return sgn_source_audio_contains_4278a0(&region.position, &point);
}
} // namespace sgn::retail
