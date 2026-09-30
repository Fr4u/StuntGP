#pragma once
#include "sgn/window_input_owner.hpp"
#include "sgn/retail_audio_driver.hpp"
#include <iosfwd>

namespace sgn::retail { class RaceSession; }
namespace sgn::retail::receiving {
struct AudioCheckpointObservation {
    std::uint32_t last_call{};
    AudioResult result{},cooperative_result{};
    bool input_gate_open{},primary_format_completed{};
};
// Authored diagnostic composition. Call once on a fresh cold Session after
// explicit source CFG, with its already attached live top-level HWND. Invokes
// the existing prefix and A1-A5 once; does not publish full462090 readiness.
// Caller retains Session/platform/window on this thread until native teardown.
AudioCheckpointObservation receive_source_audio_checkpoint(
    RaceSession&,InputWindowHandles,std::ostream&);
}
