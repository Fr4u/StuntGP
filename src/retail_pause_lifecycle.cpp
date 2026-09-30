#include "sgn/retail_pause_lifecycle.hpp"
#include <bit>

namespace sgn::retail
{
void enter_pause_4422c0(PauseLifecycleWorld world)
{
    if (static_cast<const volatile std::uint32_t &>(world.inhibit) == 1)
        return;                                                   // 4422C0..CD
    static_cast<volatile std::uint32_t &>(world.frame_pause) = 1; // 4422D0
    static_cast<volatile std::uint32_t &>(world.age) = 0;         // 4422D6
    for (unsigned i = 0; i < 4; ++i)
    {
        const auto active =
            static_cast<const volatile std::int32_t &>(world.hud.controls.controllers[i].active); // 4422E5
        static_cast<volatile unsigned &>(world.hud.controls.current) = i;                         // 4422E7
        if (active == 1 && world.vehicles[world.hud.controls.controllers[i].vehicle].energy_exempt != 1)
            set_overlay_transition(world.overlays[i], 2, 60, 0, 0, 0, 60, std::bit_cast<float>(0x3f75c28fu)); // 442316
    }
    hide_pause_hud_447c30(world.hud, world.vehicles, 0); // 44232B
    reset_vehicle_audio(world.vehicle_audio);            // 442330, includes real460DD0
    audio_stop_all(world.vehicle_audio.driver);          // 442335, separate second stop
    pause_music_45fa00(world.music);                     // 44233A
}
void resume_pause_442350(PauseResumeWorld world)
{
    for (unsigned i = 0; i < 4; ++i)
    {
        const auto active =
            static_cast<const volatile std::int32_t &>(world.hud.controls.controllers[i].active); // 442359
        static_cast<volatile unsigned &>(world.hud.controls.current) = i;                         // 44235B
        const auto vehicle = [&] {
            const auto slot = world.hud.controls.controllers[i].vehicle;
            if (slot >= world.vehicles.size())
                throw FormatError("44236C pause resume vehicle outside source owner");
            return slot;
        };
        if (active == 1 && world.vehicles[vehicle()].energy_exempt != 1)
            set_overlay_transition(world.overlays[i], 2, 60, 0, 60, 60, 0, std::bit_cast<float>(0x3f75c28fu)); // 44238A
    }
    arm_race_hud_countdown(world.hud, world.vehicles, 0);                  // 44239F
    set_level_audio_output(world.level_audio, 0);                          // 4423A6
    resume_music_45fa30(world.music);                                      // 4423AB
    static_cast<volatile std::uint32_t &>(world.frame_pause) = 0;          // 4423B0
    static_cast<volatile std::uint32_t &>(world.status) = 0;               // 4423B6
    static_cast<volatile std::uint32_t &>(world.second_pause) = 0;         // 4423BC
    static_cast<volatile std::uint32_t &>(world.physical_input.token) = 0; // 4423C2, null
}
void resume_pause_442350(PauseLifecycleWorld world)
{
    resume_pause_442350(PauseResumeWorld{world.frame_pause, world.status, world.second_pause, world.physical_input,
                                         world.hud, world.overlays, world.vehicles, world.level_audio, world.music});
}

} // namespace sgn::retail
