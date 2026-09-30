#include "sgn/retail_replay.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
template <class T> T load(std::span<const std::uint8_t> b, std::uint32_t at)
{
    if (at > b.size() || sizeof(T) > b.size() - at)
        throw std::out_of_range("replay read outside arena");
    T value;
    std::memcpy(&value, b.data() + at, sizeof(T));
    return value;
}
template <class T> void store(std::span<std::uint8_t> b, std::uint32_t at, T value)
{
    if (at > b.size() || sizeof(T) > b.size() - at)
        throw std::out_of_range("replay write outside arena");
    std::memcpy(b.data() + at, &value, sizeof(T));
}
std::uint32_t ftol(double x)
{
    if (!std::isfinite(x) || x < -9223372036854775808.0 || x >= 9223372036854775808.0)
        throw std::domain_error("replay value outside source integer conversion range");
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(x));
}
} // namespace
ReplayBank::ReplayBank() : storage_(size) {}
std::uint32_t ReplayBank::base(ReplayHandle h)
{
    if (h.slot >= slots)
        throw std::out_of_range("invalid replay handle");
    return h.slot * stride;
}
std::uint16_t ReplayBank::status(ReplayHandle h) const
{
    return load<std::uint16_t>(bytes(), base(h));
}
std::uint16_t ReplayBank::count(ReplayHandle h) const
{
    return load<std::uint16_t>(bytes(), base(h) + 2);
}
std::uint32_t ReplayBank::cursor(ReplayHandle h) const
{
    return load<std::uint32_t>(bytes(), base(h) + 0x10) - source_base;
}
void reset_replays(ReplayBank &bank, std::span<VehicleRaceState, 6> vehicles)
{
    // 004199A0..004199CD clears only status words and the six +7268 links.
    for (std::uint32_t j = 0; j < ReplayBank::slots; ++j)
        store<std::uint16_t>(bank.bytes(), j * ReplayBank::stride, 0);
    for (auto &vehicle : vehicles)
        vehicle.recording.reset();
}
void begin_recording(ReplayBank &bank, ReplayHandle handle, VehicleRaceState &race)
{
    const auto at = ReplayBank::base(handle);
    auto b = bank.bytes();
    race.recording = handle;
    store(b, at + 0x10, ReplayBank::source_base + at + ReplayBank::header_bytes);
    store<std::uint16_t>(b, at, 2);
    store<std::uint16_t>(b, at + 4, 20);
    store<float>(b, at + 8, 0);
    store(b, at + 0xc, std::uint32_t{0x3eaaaaab});
    store<std::uint16_t>(b, at + 6, 0);
    store<std::uint16_t>(b, at + 2, 0);
}
void begin_playback(ReplayBank &bank, ReplayHandle handle, VehicleRaceState &race, VehicleState &vehicle)
{
    const auto at = ReplayBank::base(handle);
    auto b = bank.bytes();
    store<std::uint16_t>(b, at, 1);
    race.recording = handle;
    store(b, at + 0x10, ReplayBank::source_base + at + ReplayBank::header_bytes);
    store<float>(b, at + 8, 0);
    reset_camera_dependent_effects(vehicle);
}
void stop_replay(ReplayBank &bank, ReplayHandle handle)
{
    const auto at = ReplayBank::base(handle);
    const auto status = bank.status(handle);
    if (status == 0)
        return;
    if (status == 2)
    {
        const auto difference = std::bit_cast<std::int32_t>(bank.cursor(handle) - at - ReplayBank::header_bytes);
        store(bank.bytes(), at + 2, static_cast<std::uint16_t>(difference / 44));
    }
    store<std::uint16_t>(bank.bytes(), at, 0);
}
void record_vehicle(ReplayBank &bank, const VehicleRaceState &race, const VehicleState &s,
                    const ReplayObservation &observed, float gravity, ReplayWorkspace &workspace)
{
    if (!race.recording)
        return;
    const auto handle = *race.recording;
    const auto base = ReplayBank::base(handle);
    if (bank.status(handle) != 2)
        return;
    auto bytes = bank.bytes();
    const float phase = load<float>(bytes, base + 8), step = load<float>(bytes, base + 0xc);
    const auto cursor = bank.cursor(handle);
    if (!std::isfinite(phase) || !std::isfinite(step))
        throw std::domain_error("nonfinite replay phase");
    const bool write = phase <= std::bit_cast<float>(std::uint32_t{0x3727c5ac});
    if (write && (cursor < base + ReplayBank::header_bytes || cursor >= base + ReplayBank::stride ||
                  (cursor - base - ReplayBank::header_bytes) % ReplayBank::frame_bytes != 0))
        throw std::out_of_range("recording cursor outside source frame range");
    if (write && handle.slot == 5 && cursor + ReplayBank::frame_bytes == ReplayBank::size)
        throw std::domain_error("sixth replay final sentinel requires actual strip.normals[0].w byte at570AE4");
    // Build the frame first so an invalid host value cannot leave a partial
    // recording. This validation is new host policy; finite retail data match.
    std::array<std::uint8_t, ReplayBank::frame_bytes> frame{};
    if (write)
    {
        auto b = std::span<std::uint8_t>(frame);
        store(b, 0, ftol(s.position.x));
        store(b, 4, ftol(s.position.y));
        store(b, 8, ftol(s.position.z));
        const std::array<float, 6> basis = {s.basis_z.x, s.basis_z.y, s.basis_z.z, s.up.x, s.up.y, s.up.z};
        for (unsigned j = 0; j < 6; ++j)
            b[0x26 + j] = static_cast<std::uint8_t>(ftol(double(basis[j]) * 127.0f));
        b[0x22] = static_cast<std::uint8_t>(ftol(s.steering));
        store(b, 0x20, static_cast<std::uint16_t>(ftol(s.rpm)));
        b[0x23] = static_cast<std::uint8_t>(s.gear);
        for (unsigned j = 0; j < 4; ++j)
        {
            store(b, 0xc + j * 2, static_cast<std::uint16_t>(ftol(observed.wheel_angles[j])));
            store(b, 0x14 + j * 2, static_cast<std::uint16_t>(ftol(double(s.wheels[j].load) * 2)));
            b[0x1c + j] = static_cast<std::uint8_t>(ftol(double(s.suspension[j]) * 2));
        }
        if (observed.effect_target == 0 && observed.effect_pending == 0)
            b[0x25] = 0;
        else
        {
            const bool target = observed.effect_active == 0 &&
                                (observed.effect_pending == 0 || observed.effect_target > observed.effect_current);
            const auto value =
                std::min(ftol(double(target ? observed.effect_target : observed.effect_current) + 1.0f), 127u);
            b[0x25] = static_cast<std::uint8_t>(value | (target ? 0x80u : 0u));
        }
        if (s.turbo_active == 1)
            b[0x24] |= 2;
        if (s.control_flags[2] == 1)
            b[0x24] |= 4;
        if (s.control_flags[3] == 1)
            b[0x24] |= 8;
        if (s.input.auxiliary != 0)
            b[0x24] |= 16;
        const double threshold = (double(gravity) * s.effective_mass) * 3.0f;
        unsigned contacts = 0;
        for (const float load : observed.contact_loads)
            if (threshold < load)
                ++contacts;
        if (contacts >= 2)
            b[0x24] |= 32;
        if (observed.surface_type == 2)
            b[0x24] |= 0x40;
        else if (observed.surface_type == 3)
            b[0x24] |= 0x80;
        else if (observed.surface_type == 4)
            b[0x24] |= 0xc0;
    }
    workspace.recording = handle;
    workspace.frame_offset = cursor;
    store(bytes, base + 8, vehicle_f32(std::fmod(double(step) + phase, 1.0)));
    if (!write)
        return;
    std::copy(frame.begin(), frame.end(), bytes.begin() + cursor);
    const auto next = cursor + ReplayBank::frame_bytes;
    store(bytes, base + 0x10, ReplayBank::source_base + next);
    bytes[next + 0x24] = 1;
    if (next >= base + ReplayBank::stride)
        stop_replay(bank, handle);
}
} // namespace sgn::retail
