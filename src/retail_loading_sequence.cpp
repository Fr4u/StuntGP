#include "sgn/retail_loading_sequence.hpp"
#include <limits>

namespace sgn::retail
{
namespace
{
void set_fades(LoadingPresentationUpdateWorld world, std::uint32_t hold, std::uint32_t duration, std::uint32_t flags,
               std::uint32_t value, std::uint32_t target)
{
    // 442A40 loop in its445940 rate1 caller domain. The current pointer is
    // published even for an inactive controller, and remains on controller3.
    for (unsigned i = 0; i < 4; ++i)
    {
        world.controls.current = i;
        if (world.controls.controllers[i].active == 1)
            set_overlay_transition(world.overlays[i], 2, hold, duration, flags, value, target, 1.0f);
    }
}
} // namespace
void run_loading_sequence_445940(LoadingSequenceWorld world, std::uint32_t frames, std::uint32_t initial_hold,
                                 std::uint32_t fade_out_hold, FrontendPerformanceCounter &counter,
                                 PolygonSubmissionSink &sink, LoadingFrameDevice &device)
{
    if (&world.update.controls != &world.frame.quads.controls ||
        world.update.overlays.data() != world.frame.quads.overlays.data() ||
        &world.loading_fade != &world.frame.quads.loading_fade)
        throw FormatError("loading sequence requires the same controller, overlay and fade owners");
    if (world.update.clock.increment == 0 ||
        world.update.clock.increment > std::numeric_limits<std::uint32_t>::max() / 60u)
        throw FormatError("loading sequence requires an initialized source clock");
    world.loading_fade = 1;
    set_light_gamma(world.gamma, 1.0f, world.gamma_device);
    for (auto &controller : world.update.controls.controllers)
        controller.active = 1;
    world.update.controls.current = 0;
    set_fades(world.update, initial_hold, initial_hold, initial_hold, 100, 0);
    for (auto remaining = frames + 2u; remaining != 0; --remaining)
    {
        update_loading_presentation_4458f0(world.update, counter);
        draw_loading_frame_4458b0(world.frame, sink, device);
        if (remaining == 120 && fade_out_hold != 0)
            set_fades(world.update, fade_out_hold, 0, 180, 0, 100);
    }
    world.loading_fade = 0;
}
} // namespace sgn::retail
