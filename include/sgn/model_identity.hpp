#pragma once
#include <cstdint>
#include <stdexcept>

namespace sgn::retail
{
inline constexpr std::uint32_t strip_model_identity = 0xfffffffeu;
inline constexpr std::uint32_t sprite_model_identity = 0xffffffffu;
// New host registry policy. Zero terminates a queue; the top two identities
// name generated source owners and may never be issued to an imported PMD.
inline std::uint32_t checked_imported_model_identity(std::uint64_t existing_count)
{
    if (existing_count >= std::uint64_t(strip_model_identity) - 1)
        throw std::length_error("imported model identity capacity");
    return static_cast<std::uint32_t>(existing_count + 1);
}
} // namespace sgn::retail
