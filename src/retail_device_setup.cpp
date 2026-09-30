#include "sgn/retail_device_setup.hpp"
#include <array>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
DeviceSetupDevice &required(DeviceSetupDevice *device)
{
    if (!device)
        throw std::invalid_argument("retail device setup requires an external device receiver");
    return *device;
}
} // namespace
void configure_device_filter_42f9c0(DeviceSetupWorld world)
{
    if ((world.capabilities.field_f0 & 0xfffffffau) == 0)
        return;                                                   // 42F9C5..CF
    required(world.global_device_5da1c4).set_render_state(17, 2); // 42F9D1..DD
    required(world.global_device_5da1c4).set_render_state(18, 2); // 42F9E0..EC
    world.globals.flag_592078 = 1;                                // 42F9EF
}
void configure_device_color_key_42f930(DeviceSetupWorld world)
{
    auto *const surface = world.surface_61c358; // 42F936; preserve this receiver across calls
    if (!surface)
        return;
    if (world.capabilities.field_e4 & 0xffffff7eu)
    { // 42F945..4F
        world.globals.flag_592068 = 1;
        world.globals.flag_59206c = 0;
        return;
    }
    const std::array<std::uint32_t, 2> key{0x10001u, 0x10001u};   // 42F969..71
    surface->set_color_key(8, key);                               // 42F97F, ignored HRESULT
    required(world.global_device_5da1c4).set_render_state(41, 1); // 42F98E
    required(world.global_device_5da1c4).set_render_state(15, 0); // 42F99D
    world.globals.flag_592068 = 0;
    world.globals.flag_59206c = 1; // 42F9A0..AA
}
void configure_device_texture_42fa10(DeviceSetupWorld world)
{
    const auto high_byte = (world.capabilities.field_e8 >> 8) & 0xffu; // 42FA15..1B
    if (high_byte & 0x50u)
        return;
    if (high_byte & 0xa0u)
        required(world.global_device_5da1c4).set_render_state(33, 1); // 42FA31
    world.globals.flag_592070 = 1;                                    // 42FA34, including the no-call branch
}
void device_setup_noop_42fa00() noexcept {} // 42FA00 is literally RET
void configure_device_multitexture_42fa40(DeviceSetupWorld world) noexcept
{
    world.globals.flag_6244f0 = 0; // 42FA4A, even when592074 is retained
    if (world.capabilities.texture_units_160 < 2 || world.capabilities.blend_stages_162 < 2)
        return;
    world.globals.flag_6244f0 = 1;
    world.globals.flag_592074 = 1; // 42FA6B..70
}
std::uint32_t initialize_retail_device(DeviceSetupWorld world)
{
    auto &device = required(world.capabilities.initialization_device_164); // 42FA80..86, cachedESI
    if (device.begin_scene() != 0)
        return 0; // 42FA8F..99
    device.set_light_state(1, 0);
    device.set_light_state(2, 0);
    device.set_light_state(3, 2);
    device.set_light_state(4, 0); // 42FA9A..FAC2
    // Literal source42FAC2..42FBF1 order. Do not sort, batch away repeated
    // writes, infer unmentioned states, or stop after failed setters.
    constexpr std::array<std::array<std::uint32_t, 2>, 30> states{
        {{1, 0},  {2, 0},  {4, 1},  {7, 1},  {23, 4}, {14, 1}, {8, 3},    {9, 2},  {11, 0}, {15, 1},
         {16, 0}, {17, 2}, {18, 2}, {19, 2}, {20, 1}, {22, 1}, {24, 240}, {25, 7}, {26, 1}, {27, 0},
         {28, 0}, {29, 0}, {31, 1}, {32, 0}, {33, 0}, {34, 0}, {35, 0},   {38, 1}, {40, 0}, {41, 0}}};
    for (const auto &state : states)
        device.set_render_state(state[0], state[1]);
    // 42FBF1..42FC69 reload5DA1C4 before each call; this may be a different
    //  object from the initialization receiver cached above.
    required(world.global_device_5da1c4).set_texture_stage_state(0, 1, 4);
    required(world.global_device_5da1c4).set_texture_stage_state(0, 2, 2);
    required(world.global_device_5da1c4).set_texture_stage_state(0, 3, 0);
    required(world.global_device_5da1c4).set_texture_stage_state(0, 4, 4);
    required(world.global_device_5da1c4).set_texture_stage_state(0, 5, 2);
    required(world.global_device_5da1c4).set_texture_stage_state(0, 6, 0);
    configure_device_filter_42f9c0(world);       // 42FC69
    configure_device_color_key_42f930(world);    // 42FC6E
    configure_device_texture_42fa10(world);      // 42FC73
    device_setup_noop_42fa00();                  // 42FC78
    configure_device_multitexture_42fa40(world); // 42FC7D
    return device.end_scene() == 0 ? 1u : 0u;    // 42FC85..8E, exact zero comparison
}
} // namespace sgn::retail
