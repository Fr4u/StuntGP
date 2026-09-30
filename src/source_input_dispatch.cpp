#include "sgn/source_input_dispatch.hpp"
#include "sgn/source_force_feedback.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "source_input_data.hpp"
#include <bit>
#include <cfenv>
#include <cstring>
#include <limits>
#include <type_traits>

namespace sgn::retail
{
namespace
{
static_assert(std::endian::native == std::endian::little);
static_assert(std::is_same_v<std::uint8_t, unsigned char>);
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::digits == 53);
template <class T> T read(const T &v) noexcept
{
    return static_cast<const volatile T &>(v);
}
template <class T> void put(T &v, T x) noexcept
{
    static_cast<volatile T &>(v) = x;
}
using sgn::bit_reinterpret::as_signed;
constexpr std::array<unsigned, 6> profile_banks{0, 1, 3, 4, 2, 4}; // 40DD10/45F1F4 verified tables
std::array<std::reference_wrapper<std::uint8_t>, 10> current_bytes(VehicleInput &i) noexcept
{
    return {i.throttle, i.brake,     i.left,           i.right, i.shift,
            i.stunt,    i.rear_lock, i.steering_boost, i.turbo, i.auxiliary};
}
void physical_previous_and_clear(MenuInputBankView b) noexcept
{
    // 40DB26..37:4+2+1 copy, then40DB3C..42:4+2+1 clear. Same physical owners
    // as40D860, not a second normal-driving edge bank.
    for (const auto group : std::array<std::array<unsigned, 2>, 3>{{{0, 4}, {4, 2}, {6, 1}}})
    {
        std::array<std::uint8_t, 4> bytes{};
        for (unsigned j = 0; j < group[1]; ++j)
            bytes[j] = read(b.current[group[0] + j].get());
        for (unsigned j = 0; j < group[1]; ++j)
            put(b.previous[group[0] + j].get(), bytes[j]);
    }
    for (auto byte : b.current)
        put(byte.get(), std::uint8_t{0});
}
void vehicle_previous_and_clear(const SourceVehicleInputView &v) noexcept
{
    const auto now = current_bytes(v.current);
    for (const auto group : std::array<std::array<unsigned, 2>, 3>{{{0, 4}, {4, 4}, {8, 2}}})
    {
        std::array<std::uint8_t, 4> bytes{};
        for (unsigned j = 0; j < group[1]; ++j)
            bytes[j] = read(now[group[0] + j].get());
        for (unsigned j = 0; j < group[1]; ++j)
            put(v.previous[group[0] + j].get(), bytes[j]);
    }
    for (auto byte : now)
        put(byte.get(), std::uint8_t{0});
}
void copy_profile(std::span<std::uint8_t, 13> to, std::span<const std::uint8_t, 13> from) noexcept
{
    // Three original unaligned DWORD copies then the thirteenth byte. Distinct
    // source banks and vehicle destinations are a binding precondition.
    for (unsigned base = 0; base < 12; base += 4)
    {
        std::array<std::uint8_t, 4> bytes{};
        for (unsigned j = 0; j < 4; ++j)
            bytes[j] = read(from[base + j]);
        for (unsigned j = 0; j < 4; ++j)
            put(to[base + j], bytes[j]);
    }
    put(to[12], read(from[12]));
}
const SourceVehicleInputView &selected(SourceInputWorld &w, unsigned physical)
{
    if (physical >= 4)
        throw FormatError("source input physical index outside four records");
    const auto index = read(w.physical[physical].binding.vehicle);
    if (index >= 6)
        throw FormatError("source input vehicle address outside six live owners");
    return w.vehicles[index];
}
std::uint32_t raw_word(const SourcePhysicalInputView &p, unsigned byte_offset) noexcept
{
    return read(p.raw.words_from_2c[(byte_offset - 0x2c) / 4]);
}
std::uint8_t button(const SourcePhysicalInputView &p, std::uint8_t index, bool previous = false) noexcept
{
    const unsigned offset = (previous ? 0x88u : 0x68u) + index;
    if (offset < 0x168u)
    {
        const auto *bytes = reinterpret_cast<const unsigned char *>(p.raw.words_from_2c.data());
        return read(bytes[offset - 0x2cu]);
    }
    return read(p.tail[offset - 0x168u]);
}
void device_pause(SourceInputWorld &w, unsigned physical, const SourceVehicleInputView &v)
{
    const auto index = read(v.profile[12]);
    const auto &p = w.physical[physical];
    if (button(p, index) != 0 && button(p, index, true) == 0)
        toggle_pause_40c750(w.toggle, {physical + 1u});
}
std::int32_t clamp_axis(std::uint32_t bits) noexcept
{
    const auto value = as_signed(bits);
    return value < -255 ? -255 : value > 255 ? 255 : value;
}
void activity_keyboard(SourceInputWorld &w, std::span<const std::uint8_t, 13> profile)
{ // 45F020
    constexpr std::array<unsigned, 13> order{0, 1, 3, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    for (const auto index : order)
        if ((read(w.keyboard[read(profile[index])]) & 0x80u) != 0)
        {
            put(w.activity, 1u);
            break;
        }
    if ((read(w.keyboard[read(profile[12])]) & 0x80u) != 0 || (read(w.keyboard[read(profile[10])]) & 0x80u) != 0)
        put(w.confirm, 1u);
}
void activity_device(SourceInputWorld &w, const SourcePhysicalInputView &p, std::span<const std::uint8_t, 13> profile)
{ // 45EF30
    // CDQ/SUB/SAR implements signed truncation toward zero. The later FABS is
    // NOT the wrapping integer abs used by424520; INT_MIN has magnitude2^31.
    const auto half = as_signed(raw_word(p, 0x34)) / 2;
    const auto magnitude = [](std::int32_t n) {
        const auto wide = static_cast<std::int64_t>(n);
        return wide < 0 ? -wide : wide;
    };
    bool active = magnitude(as_signed(raw_word(p, 0x3c))) > half;
    if (!active)
        active = magnitude(as_signed(raw_word(p, 0x38))) > half;
    if (!active)
        for (unsigned i = 4; i < 13; ++i)
            if (button(p, read(profile[i])) != 0)
            {
                active = true;
                break;
            }
    if (active)
        put(w.activity, 1u);
    if (button(p, read(profile[12])) != 0 || button(p, read(profile[10])) != 0)
        put(w.confirm, 1u);
}
} // namespace

std::uint32_t source_analog_axis_40c780(std::uint32_t axis, std::uint32_t range)
{
    // 263.0, not255.0: literal46E458 is43838000. Two separately rounded source
    // CW53 operations. Maximum finite product is263*2^31, safely inside int64.
    if (range == 0)
        return 0; // masked divide/invalid -> FISTP64 indefinite8000...0000, EAX0
    if (std::fegetround() != FE_TONEAREST)
        throw FormatError("source input requires round-nearest binary64 host arithmetic");
    volatile double reciprocal =
        static_cast<double>(std::bit_cast<float>(source_input_data::analog_scale)) / as_signed(range);
    volatile double scaled = static_cast<double>(as_signed(axis)) * reciprocal;
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(scaled));
}
void source_keyboard_input_40c8f0(SourceInputWorld &w, unsigned physical)
{
    const auto &v = selected(w, physical);
    auto now = current_bytes(v.current);
    const auto pressed = [&](unsigned index) {
        const auto key = read(v.profile[index]);
        return key != 0 && (read(w.keyboard[key]) & 0x80u) != 0;
    };
    if (pressed(0))
        put(now[0].get(), std::uint8_t{255});
    // 40C930 checks key!=0 before its throttle-priority test.
    const auto brake = read(v.profile[1]);
    if (brake != 0 && read(now[0].get()) == 0 && (read(w.keyboard[brake]) & 0x80u) != 0)
        put(now[1].get(), std::uint8_t{255});
    if (pressed(3))
        put(now[3].get(), std::uint8_t{255});
    const auto left = read(v.profile[2]);
    if (left != 0 && read(now[3].get()) == 0 && (read(w.keyboard[left]) & 0x80u) != 0)
        put(now[2].get(), std::uint8_t{255});
    for (unsigned i = 4; i < 10; ++i)
        if (pressed(i))
            put(now[i].get(), std::uint8_t{255});
    bool pause = (read(w.keyboard[0x1b]) & 0x80u) != 0 && (read(w.previous_keyboard[0x1b]) & 0x80u) == 0;
    if (!pause)
    {
        const auto key = read(v.profile[12]);
        pause = (read(w.keyboard[key]) & 0x80u) != 0 && (read(w.previous_keyboard[key]) & 0x80u) == 0;
    }
    if (pause)
        toggle_pause_40c750(w.toggle, {physical + 1u});
}
void source_analog_input_40c780(SourceInputWorld &w, unsigned physical)
{
    if (physical >= 4)
        throw FormatError("source input physical index outside four records");
    const auto &p = w.physical[physical];
    const auto range = raw_word(p, 0x34); // 40C785, before binding read
    const auto &v = selected(w, physical);
    auto now = current_bytes(v.current);
    const auto first = source_analog_axis_40c780(raw_word(p, 0x38), range);
    const auto second = source_analog_axis_40c780(raw_word(p, 0x3c), range);
    const auto horizontal = clamp_axis(first), vertical = clamp_axis(second);
    put(now[0].get(), static_cast<std::uint8_t>(vertical < 0 ? -vertical : 0));
    put(now[1].get(), static_cast<std::uint8_t>(vertical > 0 ? vertical : 0));
    // Literal40C81D..40C83F: negative horizontal writes22AD, positive22AC.
    put(now[3].get(), static_cast<std::uint8_t>(horizontal < 0 ? -horizontal : 0));
    put(now[2].get(), static_cast<std::uint8_t>(horizontal > 0 ? horizontal : 0));
    for (unsigned i = 4; i < 10; ++i)
        if (button(p, read(v.profile[i])) != 0)
            put(now[i].get(), std::uint8_t{255});
    device_pause(w, physical, v);
}
void source_button_input_40ca60(SourceInputWorld &w, unsigned physical)
{
    const auto &v = selected(w, physical);
    const auto &p = w.physical[physical];
    auto now = current_bytes(v.current);
    const auto bits = raw_word(p, 0x34);
    const auto half = as_signed((bits >> 1) | (bits & 0x80000000u));
    // No throttle/brake exclusivity in this child. Profile2/3 are enable flags,
    // not button selectors; the axis supplies the two steering tests.
    if (button(p, read(v.profile[0])) != 0)
        put(now[0].get(), std::uint8_t{255});
    if (button(p, read(v.profile[1])) != 0)
        put(now[1].get(), std::uint8_t{255});
    if (read(v.profile[2]) != 0 && as_signed(raw_word(p, 0x38)) < -half)
        put(now[2].get(), std::uint8_t{255});
    if (read(v.profile[3]) != 0 && as_signed(raw_word(p, 0x38)) > half)
        put(now[3].get(), std::uint8_t{255});
    for (unsigned i = 4; i < 10; ++i)
        if (button(p, read(v.profile[i])) != 0)
            put(now[i].get(), std::uint8_t{255});
    device_pause(w, physical, v);
}
SourceInputResult dispatch_source_input_40dad0(SourceInputWorld &w, SourceForceFeedbackWorld *feedback)
{
    if (read(w.frame_pause) == 1u)
    {
        produce_menu_input_40d860(w.menu);
        return {};
    }
    for (unsigned physical = 0; physical < 4; ++physical)
    {
        const auto &p = w.physical[physical];
        if (read(p.active) != 1u || read(p.blocked) != 0u || read(p.binding.vehicle) == 0xffffu)
            continue;
        physical_previous_and_clear(p.edges);
        const auto index = read(p.binding.vehicle);
        if (index >= 6)
            throw FormatError("40DAD0 vehicle address outside six live owners");
        const auto &v = w.vehicles[index];
        const auto enabled = read(v.enabled); // 40DB58 before40DB68
        put(w.current_vehicle, index);
        if (enabled == 0)
            continue;
        vehicle_previous_and_clear(v);
        const auto kind = read(p.kind);
        const auto branch = kind - 1u;
        put(w.source_device, 0x0062d5acu + physical * 0x1dcu); // 40DBAE, even invalid kind
        if (branch <= 5u)
        {
            constexpr std::array<std::int32_t, 6> control{2, 0, 3, 4, 1, 4};
            const auto profile = read(p.profile_index); // source reads+14 BEFORE kind/sensitivity stores
            put(v.control_kind, control[branch]);
            put(v.steering_sensitivity, std::bit_cast<float>(branch == 2u ? 0x3f4ccccdu : 0x3f800000u));
            copy_profile(v.profile, source_input_profile(w.profiles, profile_banks[branch], profile));
            if (branch == 0u)
                source_keyboard_input_40c8f0(w, physical);
            else if (branch == 3u || branch == 5u)
                source_button_input_40ca60(w, physical);
            else
                source_analog_input_40c780(w, physical);
        }
        if (read(p.feedback_mode) == 2u)
        {
            if (!feedback)
                return {SourceInputStatus::force_feedback_410a10_required, physical};
            require_force_feedback_dispatch_binding(w, *feedback); // host ownership only, at reached boundary
            force_feedback_410a10(*feedback, physical);            // 40DCE1, real child; continue next physical slot
        }
    }
    return {};
}
void publish_input_feedback_kind_45ef00(std::span<const SourcePhysicalInputView, 4> physical)
{
    for (const auto &p : physical)
    {
        if (read(p.active) != 1u)
            continue;
        const auto requested = read(p.feedback_request), previous = read(p.previous_feedback_request);
        if (requested == previous)
            continue;
        put(p.feedback_mode, requested);
        put(p.previous_feedback_request, read(p.feedback_request));
    }
}
void update_source_input_activity_45f110(SourceInputWorld &w)
{
    const auto confirm = read(w.confirm), activity = read(w.activity);
    put(w.previous_confirm, confirm);
    put(w.confirm, 0u);
    put(w.previous_activity, activity);
    put(w.activity, 0u);
    for (unsigned physical = 0; physical < 4; ++physical)
    {
        if (read(w.confirm) == 1u)
            break; // source stops when confirm, NOT merely activity, becomes1
        const auto &p = w.physical[physical];
        if (read(p.active) != 1u)
            continue;
        put(p.activity_kind, 0u);
        const auto branch = read(p.kind) - 1u;
        if (branch > 5u)
            continue;
        constexpr std::array<std::uint32_t, 6> activity_kind{5, 2, 1, 3, 4, 3};
        put(p.activity_kind, activity_kind[branch]);
        const auto profile = source_input_profile(w.profiles, profile_banks[branch], read(p.profile_index));
        if (branch == 0)
            activity_keyboard(w, profile);
        else
            activity_device(w, p, profile);
    }
}
} // namespace sgn::retail
