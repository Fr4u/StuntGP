#include "sgn/session_outer_source_bindings.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
SessionOuterDeviceWords session_outer_device_words(RaceSessionState &s) noexcept
{
    return {s.outer.render_counter, s.outer.device.scene_active, s.outer.device.finalized};
}
DemoTailWords session_outer_demo_words(RaceSessionState &s) noexcept
{
    return {s.outer.demo.age, s.outer.demo.offset, s.outer.demo.initialized};
}
DeviceSetupGlobals session_outer_device_setup_globals(RaceSessionState &s) noexcept
{
    auto &d = s.outer.device;
    return {d.flag_592068, d.flag_59206c, d.flag_592070, d.flag_592074, d.flag_592078, d.flag_6244f0};
}
SessionPolygonCapabilities session_outer_polygon_capabilities(RaceSessionState &s,
                                                              SelectedDeviceDepthBorrow selected) noexcept
{
    return {selected.word_64, s.outer.device.flag_592074};
}
void reset_device_source_globals_42f300(SessionOuterDeviceSourceState &d, PolygonSubmissionState &submission) noexcept
{
    submission.specular_enabled = 0; // 42F302,592050
    d.fog_a = 0;                     // 42F307,592054
    d.fog_b = 0;                     // 42F30C,592058
    submission.depth_enabled = 0;    // 42F311,59205C
    submission.depth_write = 0;      // 42F316,592060
    submission.depth_other = 0;      // 42F31B,592064
    d.flag_592068 = 0;               // 42F320
    d.flag_59206c = 0;               // 42F325
    d.flag_592070 = 0;               // 42F32A
    d.flag_592074 = 0;               // 42F32F
    d.flag_592078 = 0;               // 42F334
}
} // namespace sgn::retail
