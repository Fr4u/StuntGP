#include "sgn/retail_pause_toggle.hpp"

namespace sgn::retail
{
void toggle_pause_40c750(PauseToggleWorld world, PausePhysicalInputIdentity incoming)
{
    if (static_cast<const volatile std::uint32_t &>(world.scene) != 1)
        return; // 40C750..5D
    if (static_cast<const volatile std::uint32_t &>(world.lifecycle.frame_pause) == 1)
    {                                         // 40C75F..65
        resume_pause_442350(world.lifecycle); // 40C767 real tail target
        return;
    }
    static_cast<volatile std::uint32_t &>(world.lifecycle.physical_input.token) = incoming.token; // 40C76C
    enter_pause_4422c0(world.lifecycle); // 40C772 real tail target, publication already occurred
}
} // namespace sgn::retail
