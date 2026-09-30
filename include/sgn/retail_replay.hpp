#pragma once
#include "sgn/retail_race.hpp"
#include "sgn/vehicle_replay_handle.hpp"
#include <span>
#include <vector>

namespace sgn::retail
{
// The original six recordings are contiguous. A final record in slots 0..4
// writes the sentinel into the next recording's metadata at +24.
// Header +10 retains its source32 address bits. cursor() decodes an arena
// offset for native access; bytes() never swizzles serialized headers.
class ReplayBank
{
  public:
    static constexpr std::uint32_t source_base = 0x4d5cc0;
    static constexpr std::uint32_t slots = 6, stride = 0x19d00, header_bytes = 0x80;
    static constexpr std::uint32_t frame_bytes = 0x2c, frame_count = 2400, size = slots * stride;
    ReplayBank();
    std::span<std::uint8_t> bytes() noexcept
    {
        return storage_;
    }
    std::span<const std::uint8_t> bytes() const noexcept
    {
        return storage_;
    }
    static std::uint32_t base(ReplayHandle);
    std::uint16_t status(ReplayHandle) const;
    std::uint16_t count(ReplayHandle) const;
    // Modular source pointer subtraction, not an extra validity preflight.
    // The reached recorder/player access applies its existing bounds.
    std::uint32_t cursor(ReplayHandle) const;

  private:
    std::vector<std::uint8_t> storage_;
};
// Source globals 570AC0 / 570AC4. Each offset is relative to ReplayBank.
// Playback's preceding-frame global 4D5CB8 is kept separately.
struct ReplayWorkspace
{
    std::optional<ReplayHandle> recording;
    std::uint32_t frame_offset{}, previous_frame_offset{};
};
// Read-only snapshot of fields whose physical owner is contact or effects.
// Integration must obtain these values from those owners at the source call.
struct ReplayObservation
{
    std::array<float, 4> wheel_angles{}, contact_loads{}; // 2074/2060 + i*98
    std::int32_t surface_type{};                          // first wheel 2058
    float effect_current{}, effect_target{};              // 2360/2364
    std::uint32_t effect_active{}, effect_pending{};      // 2368/236C
};
void reset_replays(ReplayBank &, std::span<VehicleRaceState, 6>);                    // 004199A0
void begin_recording(ReplayBank &, ReplayHandle, VehicleRaceState &);                // 0041AB40
void begin_playback(ReplayBank &, ReplayHandle, VehicleRaceState &, VehicleState &); // 0041AB00, all four reset callees
void stop_replay(ReplayBank &, ReplayHandle);                                        // 0041AAC0
// Full 0041AC30 on the bounded source domain. A final write in slot 5 has an
// external alias at570AE4 (strip.normals[0].w low byte). Its write borrow
// is not supplied by this API, so the final slot5 write is still rejected.
void record_vehicle(ReplayBank &, const VehicleRaceState &, const VehicleState &, const ReplayObservation &,
                    float gravity, ReplayWorkspace &);
} // namespace sgn::retail
