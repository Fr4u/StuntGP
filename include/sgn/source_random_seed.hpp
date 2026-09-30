#pragma once
#include <cstdint>

namespace sgn::retail
{
struct VehicleRandom;
class RaceSession;
class RaceSessionPlatform;
// Complete42F190 ->46327F state effect: one DWORD store to the existing owner
// of4959C0. All32 seed bits are retained, including0 andFFFFFFFF. No RNG draw.
// CPU registers, stack/calling convention and wall-clock acquisition are not
// modeled here.42F1A0 first needs actual4632A7; supplying a constant is not it.
void seed_source_random_42f190(VehicleRandom &, std::uint32_t seed) noexcept;
// Before or after Session attachment: borrow the same platform RNG.
// No construction, cache initialization, source time query or RNG draw.
void seed_platform_source_random_42f190(RaceSessionPlatform &, std::uint32_t seed);
// Compatibility delegate to that SAME platform binding, on its owner
// thread. No source startup, constructor call, scene reset or automatic seed.
void seed_session_source_random_42f190(RaceSession &, std::uint32_t seed);
} // namespace sgn::retail
