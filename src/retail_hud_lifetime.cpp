#include "sgn/retail_hud_lifetime.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_signed;
bool below_or_unordered(double a, double b)
{
    return a < b || std::isnan(a) || std::isnan(b);
}
} // namespace
void update_overlay_transition(ControllerOverlayTransition &state)
{
    if (state.active == 0)
        return;
    const auto divisor = as_signed(state.hold_ticks);
    const auto age = std::min(as_signed(state.age), divisor);
    const auto numerator = as_signed((state.target - state.value) * std::bit_cast<std::uint32_t>(age));
    if (divisor == 0 || (numerator == std::numeric_limits<std::int32_t>::min() && divisor == -1))
        throw FormatError("overlay interpolation outside defined source division domain");
    state.initial_value = std::bit_cast<std::uint32_t>(numerator / divisor) + state.value;
    if (state.duration)
        --state.duration;
    else
        ++state.age;
    if (as_signed(state.flags) > 0 && as_signed(state.age) >= as_signed(state.flags))
        state.active = 0;
}
void update_controller_overlays(contact::player::Controls &controls, std::span<ControllerOverlayTransition, 4> overlays)
{
    for (unsigned i = 0; i < 4; ++i)
    {
        controls.current = i;
        if (controls.controllers[i].active == 1)
            update_overlay_transition(overlays[i]);
    }
}
void update_hud_element(HudElementView element)
{
    if (element.active == 0)
        return;
    const double value = element.value, target = std::bit_cast<float>(element.phase);
    constexpr double minimum = double(0.1f);
    if (below_or_unordered(value, target))
    {
        double delta = (100.0 - (target - value)) * 0.0625;
        if (below_or_unordered(delta, minimum))
            delta = minimum;
        const double next = value + delta;
        element.value = vehicle_f32(next);
        if (next > target)
        {
            element.active = 0;
            element.value = std::bit_cast<float>(element.phase);
        }
    }
    else
    {
        double delta = (value - target) * 0.0625;
        if (below_or_unordered(delta, minimum))
            delta = minimum;
        const double next = value - delta;
        element.value = vehicle_f32(next);
        if (below_or_unordered(next, target))
        {
            element.active = 0;
            element.value = std::bit_cast<float>(element.phase);
        }
    }
}
void age_controller_notices(contact::player::Controls &controls) noexcept
{
    for (auto &controller : controls.controllers)
        if (controller.active == 1)
        {
            contact::player::Notice *chosen = nullptr;
            for (auto &notice : controller.notices)
                if (notice.remaining != 0 && (!chosen || chosen->tick > notice.tick))
                    chosen = &notice;
            if (chosen)
                chosen->remaining = as_signed(std::bit_cast<std::uint32_t>(chosen->remaining) - 1u);
        }
}
void update_race_hud(RaceHudView view, std::span<const VehicleState, 6> physics,
                     std::span<const VehicleRaceState, 6> race)
{
    for (unsigned i = 0; i < 4; ++i)
    {
        auto &controller = view.controls.controllers[i];
        if (controller.active != 1)
            continue;
        if (controller.vehicle >= 6)
            throw FormatError("HUD controller vehicle identity");
        view.controls.current = i;
        view.selected_hud = i;
        if (race[controller.vehicle].lap_timer != -1)
            for (unsigned element : {3u, 4u, 5u})
                update_hud_element(hud_element(view, i, element));
        if (physics[controller.vehicle].special_mode == 1)
            update_hud_element(hud_element(view, i, 6));
        auto &countdown = view.countdown[i];
        if (countdown.enabled == 1)
        {
            if (countdown.ticks)
                --countdown.ticks;
            else
                update_hud_element(hud_element(view, i, 0));
        }
        auto notice = hud_element(view, i, 8);
        if (notice.active == 1)
            update_hud_element(notice);
    }
    view.controls.current = 0;
    age_controller_notices(view.controls);
}
} // namespace sgn::retail
