#pragma once
#include <cstdint>
namespace sgn::retail
{
struct FrontendSourceStorage;
// Full43DD10: discard input byte3, swap bytes0/2, preserve byte1, one store5FF4B8.
// This leaf neither advances43D0B0 nor binds an unproven rendering consumer.
void set_frontend_packed_word_43dd10(FrontendSourceStorage &, std::uint32_t ecx) noexcept;
} // namespace sgn::retail
