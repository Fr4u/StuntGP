#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace source_outer_receiving {
struct CaptureBegin {std::uint64_t tick{},epoch{};};
inline std::array<CaptureBegin,8> capture_begins{};
inline std::size_t capture_begin_count{};
inline bool capture_probe_overflow{};
// Renderer argument observation only. No allocation, callbacks, game access,
// exceptions, result replacement, clock changes or branch steering.
inline void observe_capture_begin(std::uint64_t tick,std::uint64_t epoch) noexcept {
    if(capture_begin_count<capture_begins.size())capture_begins[capture_begin_count++]={tick,epoch};
    else capture_probe_overflow=true;
}
inline std::array<unsigned char,256> keyboard_packet{};
inline unsigned keyboard_calls{};
}
