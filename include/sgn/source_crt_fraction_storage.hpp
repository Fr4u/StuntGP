#pragma once
#include <cstdint>
namespace sgn::retail
{
// Sole added CRT scalar owners, retained with NativeSourceMenuCrt for its whole
// lifetime. Startup already owns63652C: it is MOVED here, never duplicated or
// reset for a frame.496560 is the literal PE word read by465032, not a guessed
// Boolean/math-mode interpretation. Source errno remains App.crt_error_storage.
struct SourceCrtFractionStorage
{
    std::uint32_t literal_496560{0x2694u};
    std::uint32_t bypass_636528{};
    std::uint32_t workaround_63652c{};
    std::uint64_t result_6365c0{};
    std::uint8_t register_arguments_6365c8{};
};
} // namespace sgn::retail
