#include "sgn/source_random_seed.hpp"
#include "sgn/vehicle_math.hpp"

namespace sgn::retail
{
void seed_source_random_42f190(VehicleRandom &random, std::uint32_t seed) noexcept
{
    random.state = seed; // 463283: MOV [004959C0],EAX;42F190 forwards its ECX.
}
} // namespace sgn::retail
