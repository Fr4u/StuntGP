#include "sgn/retail_pause_gate.hpp"

namespace sgn::retail
{
namespace
{
void store(std::uint32_t &destination, std::uint32_t value) noexcept
{
    // Retain the original ordered stores, including the transient inhibit=1
    // when countdown1 becomes0. This is not a cross-thread synchronization API.
    static_cast<volatile std::uint32_t &>(destination) = value;
}
} // namespace
std::uint32_t advance_pause_gate_4423d0(PauseGateWorld world) noexcept
{
    const auto entry_pause = world.frame_pause; // 4423D0, reused for the return
    if (entry_pause == 1u)
    {
        const auto age = world.age;
        if (age < 18600u)
            store(world.age, age + 1u); // 4423E8..4423F1, unsigned cap
        if (world.activity == 1u)
            store(world.age, 0u); // 4423F7..4423FF, after increment
    }
    const auto countdown = world.phase_ticks;
    if (countdown > 0u)
    {
        const auto remaining = countdown - 1u;
        store(world.inhibit, 1u);            // 442410
        store(world.phase_ticks, remaining); // 442415
        if (remaining == 0u)
            store(world.inhibit, 0u); // 44241D
    }
    if (world.other_gate == 1u)
        return 1u;                      // 442423..44242C
    return entry_pause == 1u ? 1u : 0u; // 442430..442435
}
} // namespace sgn::retail
