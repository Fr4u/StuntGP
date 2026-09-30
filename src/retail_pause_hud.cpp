#include "sgn/retail_pause_hud.hpp"

namespace sgn::retail
{
void hide_selected_pause_hud_447ba0(RaceHudView view, std::uint32_t ticks) noexcept
{
    const auto current = view.controls.current;
    static_cast<volatile unsigned &>(view.selected_hud) = current; // 447BAA
    auto &countdown = view.countdown[current];
    static_cast<volatile std::uint32_t &>(countdown.ticks) = ticks;      // 447BAF
    static_cast<volatile std::uint32_t &>(countdown.enabled) = 1;        // 447BB2
    static_cast<volatile float &>(countdown.value) = 0.0f;               // 447BB9, raw00000000
    static_cast<volatile std::uint32_t &>(countdown.phase) = 0x42c80000; // 447BC0
}
void hide_pause_hud_447c30(RaceHudView view, std::span<const VehicleState, 6> vehicles, std::uint32_t ticks) noexcept
{
    const auto saved = view.controls.current;                    // 447C31
    static_cast<volatile unsigned &>(view.controls.current) = 0; // 447C3B
    for (unsigned i = 0; i < 4; ++i)
    {
        if (vehicles[view.controls.controllers[i].vehicle].energy_exempt == 0) // 447C4F..5D
            hide_selected_pause_hud_447ba0(view, ticks);                       // 447C61
        static_cast<volatile unsigned &>(view.controls.current) = i + 1;       // 447C73, includes4
    }
    static_cast<volatile unsigned &>(view.controls.current) = saved; // 447C7C
}
} // namespace sgn::retail
