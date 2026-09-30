#include "sgn/retail_replay_playback.hpp"
#include "sgn/retail_replay_dynamics.hpp"
#include <cmath>
#include <cstring>

namespace sgn::retail
{
namespace
{
float read_float(std::span<const std::uint8_t> bytes, std::uint32_t at)
{
    float value;
    std::memcpy(&value, bytes.data() + at, 4);
    return value;
}
template <class T> void write(std::span<std::uint8_t> bytes, std::uint32_t at, T value)
{
    std::memcpy(bytes.data() + at, &value, sizeof(T));
}
} // namespace
VehicleAirTickObservation replay_air_dynamics(ReplayPlaybackVehicle vehicle, const ReplayPlaybackContext &context)
{
    replay_tyre_response(vehicle.parameters, vehicle.physics, context.dynamics.steering);
    replay_contact_state(vehicle.physics);
    replay_braking_signals(vehicle.physics);
    return vehicle_air_tick(vehicle.parameters, vehicle.physics, vehicle.air, context.air);
}
ReplayPlaybackObservation play_replay_frame(ReplayBank &bank, const VehicleRaceState &recording_owner,
                                            ReplayWorkspace &workspace, ReplayPlaybackVehicle vehicle,
                                            const ReplayPlaybackContext &context)
{
    if (!recording_owner.recording)
        throw std::domain_error("direct replay playback requires a recording identity");
    const auto handle = *recording_owner.recording;
    const auto base = ReplayBank::base(handle);
    auto bytes = bank.bytes();
    const auto cursor = bank.cursor(handle);
    const float old_phase = read_float(bytes, base + 8), step = read_float(bytes, base + 0xc);
    if (!std::isfinite(old_phase) || !std::isfinite(step))
        throw std::domain_error("nonfinite replay playback phase");
    if (cursor < base + ReplayBank::header_bytes || cursor >= base + ReplayBank::stride ||
        (cursor - base - ReplayBank::header_bytes) % ReplayBank::frame_bytes != 0)
        throw std::out_of_range("playback cursor outside source frame range");
    const auto next = cursor + ReplayBank::frame_bytes;
    if (next > bytes.size() || ReplayBank::frame_bytes > bytes.size() - next)
        throw std::out_of_range("playback next frame reaches unresolved external arena state");
    const float phase = vehicle_f32(std::fmod(double(step) + old_phase, 1.0));
    const bool advance = 1.0 - double(step) <= phase;
    const auto advanced_next = next + ReplayBank::frame_bytes;
    // 0041AC1C reads the sentinel only after an actual advance. Do not mask an
    // invalid sixth-slot read with invented padding or mutate before rejection.
    if (advance && (advanced_next > bytes.size() || 0x24u >= bytes.size() - advanced_next))
        throw std::out_of_range("playback sentinel reaches unresolved external arena state");
    workspace.recording = handle;
    workspace.previous_frame_offset = cursor;
    workspace.frame_offset = next;
    write(bytes, base + 8, phase);
    const ReplayFramePair frames{std::span<const std::uint8_t, 44>(bytes.data() + cursor, 44),
                                 std::span<const std::uint8_t, 44>(bytes.data() + next, 44), phase};
    auto &s = vehicle.physics;
    decode_replay_position(frames, s);
    decode_replay_basis(frames, s);
    decode_replay_wheels(frames, s, vehicle.parameters, vehicle.wheels);
    decode_replay_steering(frames, s);
    decode_replay_suspension(frames, s);
    decode_replay_loads(frames, s, vehicle.contact, context.gravity_magnitude);
    decode_replay_turbo(frames, s);
    decode_replay_gear(frames, s);
    decode_replay_auxiliary(frames, s);
    decode_replay_brake(frames, s);
    decode_replay_rpm(frames, s);
    decode_replay_throttle(frames, s);
    decode_replay_shock(frames, vehicle.contact);
    decode_replay_surface(frames, vehicle.contact);
    ReplayPlaybackObservation observed;
    observed.decoded = true;
    observed.air = replay_air_dynamics(vehicle, context);
    if (advance)
    {
        write(bytes, base + 0x10, ReplayBank::source_base + next);
        workspace.frame_offset = advanced_next;
        observed.advanced = true;
        if ((bytes[advanced_next + 0x24] & 1) != 0)
        {
            stop_replay(bank, handle);
            observed.stopped = true;
        }
    }
    return observed;
}
ReplayPlaybackObservation update_replay_vehicle(ReplayBank &bank, const VehicleRaceState &current,
                                                ReplayWorkspace &workspace, ReplayPlaybackVehicle vehicle,
                                                const ReplayPlaybackContext &context)
{
    if (!current.recording || bank.status(*current.recording) != 1)
        return {};
    return play_replay_frame(bank, current, workspace, vehicle, context);
}
} // namespace sgn::retail
