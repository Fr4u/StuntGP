#include "sgn/retail_race_hud.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
namespace sgn::retail
{
namespace
{
float f32(double value)
{
    return static_cast<float>(value);
}
using sgn::bit_reinterpret::as_float;
void selected(const RaceHudView &view)
{
    if (view.selected_hud >= 4)
        throw FormatError("selected HUD outside four controllers");
}
void current(const RaceHudView &view)
{
    if (view.controls.current >= 4)
        throw FormatError("HUD current controller outside four slots");
}
void clear_all_notices(contact::player::Controls &controls)
{
    for (auto &controller : controls.controllers)
        contact::player::clear_notices(controller.notices);
}
} // namespace
HudElementView hud_element(RaceHudView view, unsigned hud, unsigned element)
{
    if (hud >= 4 || element >= 18)
        throw FormatError("HUD element outside native storage");
    auto &layout = view.layouts[hud];
    if (element == 0)
        return {layout.elements[0], view.countdown[hud].enabled, view.countdown[hud].value, view.countdown[hud].phase};
    if (element == 6)
        return {layout.elements[6], view.controls.controllers[hud].respawn_transition.state, layout.value[5],
                view.controls.controllers[hud].respawn_transition.phase};
    const auto index = element < 6 ? element - 1 : element - 2;
    return {layout.elements[element], layout.active[index], layout.value[element - 1], layout.phase[index]};
}
void initialize_selected_hud_layout(RaceHudView view)
{
    selected(view);
    current(view);
    const auto slot = view.selected_hud;
    const double x = view.margins.x, y = view.margins.y;
    const float stored_y = f32(y);
    view.countdown[slot].active = 1;
    view.countdown[slot].enabled = 0;
    view.countdown[slot].value = 100;
    view.countdown[slot].phase = 0x42c80000;
    auto a = hud_element(view, slot, 1);
    a.layout.x = f32(640.0 - (x + as_float(0x43266667)));
    a.layout.y = f32(480.0 - (y + as_float(0x42d4cccd)));
    a.value = 0;
    a.layout.word_14 = 256;
    a.layout.word_18 = 0;
    auto b = hud_element(view, slot, 2);
    b.layout.x = as_float(0x437f3333);
    b.layout.y = f32(double(stored_y) + 6.0);
    b.value = 0;
    b.layout.word_14 = 0;
    b.layout.word_18 = 0xffffffa0;
    auto c = hud_element(view, slot, 7);
    c.layout.x = f32(x + as_float(0x420ccccd));
    c.layout.y = f32(448.0 - stored_y);
    c.value = 0;
    c.layout.word_14 = 0xffffffa0;
    c.layout.word_18 = 0;
    const float repeated_x = f32(double(as_float(0x440ecccd)) - x),
                repeated_y = f32(double(stored_y) + as_float(0x404ccccd));
    for (unsigned i = 3; i <= 5; ++i)
    {
        auto element = hud_element(view, slot, i);
        element.layout.x = repeated_x;
        element.layout.y = repeated_y;
        element.value = 0;
        element.layout.word_14 = 160 + (i - 3) * 96;
        element.layout.word_18 = 0;
    }
    auto d = hud_element(view, slot, 6);
    d.layout.x = f32(x + as_float(0x404ccccd));
    d.layout.y = f32(double(stored_y) + as_float(0x4199999a));
    d.value = 0;
    d.layout.word_14 = 0xffffff40;
    d.layout.word_18 = 0;
    // 447B70 changes selected HUD to current controller, not the loop's slot.
    set_hud_countdown(view.countdown, view.controls.current, view.selected_hud, 60);
    for (const unsigned i : {4u, 3u, 5u})
    {
        auto element = hud_element(view, view.selected_hud, i);
        element.active = 1;
        element.value = 100;
    }
}
void initialize_all_hud_layouts(RaceHudView view)
{
    current(view);
    for (unsigned i = 0; i < 4; ++i)
    {
        view.selected_hud = i;
        initialize_selected_hud_layout(view);
    }
    view.selected_hud = 0;
}
void initialize_selected_hud_ranking(RaceHudView view)
{
    selected(view);
    auto base = hud_element(view, view.selected_hud, 8);
    base.active = 0;
    base.value = 0;
    for (unsigned i = 0; i < 6; ++i)
    {
        auto element = hud_element(view, view.selected_hud, 12 + i);
        element.layout.x = as_float(0x42cccccd);
        element.layout.y = as_float(0x42cccccd);
        element.active = 0;
        element.value = 100;
        element.phase = 0;
        element.layout.word_14 =
            static_cast<std::uint32_t>(static_cast<std::int32_t>(double(i * 80) * as_float(0x3f4ccccd) + 640.0));
        element.layout.word_18 = 0;
    }
    auto a = hud_element(view, view.selected_hud, 9);
    a.layout.x = as_float(0x414ccccd);
    a.layout.y = as_float(0x41cccccd);
    a.active = 1;
    a.value = 100;
    a.phase = 0;
    a.layout.word_14 = 0xfffffee0;
    a.layout.word_18 = 0;
    auto b = hud_element(view, view.selected_hud, 10);
    b.layout.x = 624;
    b.layout.y = 32;
    b.active = 1;
    b.value = 100;
    b.phase = 0;
    b.layout.word_14 = 230;
    b.layout.word_18 = 0;
    auto c = hud_element(view, view.selected_hud, 11);
    c.layout.x = 320;
    c.layout.y = 440;
    c.active = 1;
    c.value = 100;
    c.phase = 0;
    c.layout.word_14 = 0;
    c.layout.word_18 = 128;
}
void initialize_race_hud(RaceHudView view)
{
    current(view);
    view.selected_hud = 0;
    view.margins = {};
    clear_all_notices(view.controls);
    initialize_all_hud_layouts(view);
    initialize_selected_hud_ranking(view);
}
void enable_selected_race_hud(RaceHudView view)
{
    selected(view);
    view.countdown[view.selected_hud].active = 1;
}
void arm_race_hud_countdown(RaceHudView view, std::span<const VehicleState, 6> vehicles, std::uint32_t ticks)
{
    const auto original = static_cast<const volatile unsigned &>(view.controls.current); // 447BD1, pointer identity
                                                                                         // only
    static_cast<volatile unsigned &>(view.controls.current) = 0; // 447BDB, before first vehicle read
    for (unsigned visit = 0; visit < 4; ++visit)
    {
        const auto controller = static_cast<const volatile unsigned &>(view.controls.current);
        if (controller >= view.controls.controllers.size())
            throw FormatError("447BEF countdown controller outside source owner");
        const auto vehicle = view.controls.controllers[controller].vehicle;
        if (vehicle >= vehicles.size())
            throw FormatError("447BF5 countdown vehicle outside source owner");
        if (vehicles[vehicle].energy_exempt == 0)
            set_hud_countdown(view.countdown, controller, view.selected_hud, ticks); // 447C01
        static_cast<volatile unsigned &>(view.controls.current) =
            static_cast<const volatile unsigned &>(view.controls.current) + 1u; // 447C06..13 includes slot4
    }
    static_cast<volatile unsigned &>(view.controls.current) = original; // 447C1C, after all four children
}
} // namespace sgn::retail
