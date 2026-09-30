#include "sgn/source_force_feedback.hpp"
#include "sgn/source_input_dispatch.hpp"    // receiving applies the storage/dispatch patch
#include "sgn/menu_directinput_startup.hpp" // receiving applies the shared-owner extension
#include "sgn/race_session.hpp"
#include "sgn/vehicle_stunt_awards.hpp"
#include <dinput.h>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstring>
#include <limits>

namespace sgn::retail
{
SourceForceFeedbackWorld::SourceForceFeedbackWorld(RaceSession &session, SourceInputStorage &input,
                                                   Win32MenuDirectInputStartup &startup)
    : SourceForceFeedbackWorld(*session.shared_platform(), input, startup)
{
}
SourceForceFeedbackWorld::SourceForceFeedbackWorld(RaceSessionPlatform &owner, SourceInputStorage &input,
                                                   Win32MenuDirectInputStartup &startup)
    : platform(owner), input(input), startup(startup)
{
}
namespace
{
constexpr std::uint32_t device_base = 0x572ce8u, device_stride = 0x5a4u;
constexpr std::uint32_t physical_data_base = 0x62d738u, physical_stride = 0x1dcu;
using Words = std::array<std::uint32_t, 8>;
void require_thread(SourceForceFeedbackWorld &w)
{
    w.platform.require_owner_thread();
    if (&w.input != &w.platform.state().source_input)
        throw FormatError("force feedback input belongs to a different Session");
}
SourceForceFeedbackStorage &source(SourceForceFeedbackWorld &w)
{
    require_thread(w);
    return w.input.force_feedback;
}
MenuDirectInputStartupState &devices(SourceForceFeedbackWorld &w)
{
    require_thread(w);
    return w.startup.borrow_force_feedback_state(w.input.acquisition);
}
MenuDirectInputNativeRecord &record(SourceForceFeedbackWorld &w, unsigned index)
{
    auto &bank = devices(w).records;
    if (index >= bank.size())
        throw FormatError("force feedback device address leaves the ten-record source bank");
    return bank[index];
}
MenuDirectInputEffectRecord &effect(SourceForceFeedbackWorld &w, unsigned index, unsigned slot)
{
    auto &r = record(w, index);
    if (slot >= r.effects.size())
        throw FormatError("force feedback effect address leaves the twelve-record source bank");
    return r.effects[slot];
}
unsigned physical_device(unsigned physical)
{
    return physical - 1u;
} // 572744 + physical*5A4, NOT registry lookup
unsigned cursor_device(SourceForceFeedbackWorld &w)
{
    const auto offset = devices(w).cursor_576550 - device_base;
    if (offset % device_stride != 0 || offset / device_stride >= 10u)
        throw FormatError("force feedback cursor leaves the ten-record source bank");
    return offset / device_stride;
}
Words &selected_words(SourceForceFeedbackWorld &w)
{
    auto &s = source(w);
    const auto offset = s.selected_data_4d5b24 - physical_data_base;
    if (offset % physical_stride != 0 || offset / physical_stride >= s.physical_words.size())
        throw FormatError("force feedback selected physical data identity is invalid");
    return s.physical_words[offset / physical_stride];
}
unsigned selected_vehicle(SourceForceFeedbackWorld &w)
{
    require_thread(w);
    const auto index = w.platform.state().current_slot;
    if (index >= 6u)
        throw FormatError("force feedback selected vehicle leaves the six live owners");
    return index;
}
float value(const Words &a, unsigned i)
{
    return std::bit_cast<float>(a[i]);
}
void store(Words &a, unsigned i, double v)
{
    a[i] = std::bit_cast<std::uint32_t>(vehicle_f32(v));
}
void numeric_environment()
{
    if (std::fegetround() != FE_TONEAREST)
        throw FormatError("force feedback requires source round-nearest arithmetic");
}
// Separate binary64 operations preserve the caller's source PC53 order. No
// reassociation/FMA/fast-math; receiving must use /fp:strict or its equivalent.
double add(double a, double b)
{
    volatile double r = a + b;
    return r;
}
double mul(double a, double b)
{
    volatile double r = a * b;
    return r;
}
double divide(double a, double b)
{
    volatile double r = a / b;
    return r;
}
// 462860 masked FISTP64 returns the low DWORD. Integer indefinite's low word is
// zero for invalid/overflow; this is the actual conversion, not zero force.
std::uint32_t ftol(double n)
{
    if (!std::isfinite(n) || n < -9223372036854775808.0 || n >= 9223372036854775808.0)
        return 0;
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(n)));
}
bool source_less(double a, double b)
{
    return std::isnan(a) || std::isnan(b) || a < b;
} // x87 C0, including unordered

// Source DIEFFECT is the DX5 13-DWORD layout (0x34). Native pointer-bearing
// fields are rebuilt at the actual API boundary. Local arrays/structures live
// until COM returns; the receiver must not retain pointers into this object.
struct NativeParameters final
{
    DIEFFECT descriptor{};
    std::array<DWORD, 2> axes{};
    std::array<LONG, 2> direction{};
    DICONDITION condition{};
    DIPERIODIC periodic{};
    NativeParameters(const MenuDirectInputEffectRecord &e, const SourceForceFeedbackStorage &s,
                     std::span<const std::uint32_t> specific)
    {
        static_assert(sizeof(DWORD) == 4 && sizeof(LONG) == 4 && sizeof(DICONDITION) == 24 && sizeof(DIPERIODIC) == 16);
        const auto *d = e.payload.data() + 16; // source record+48
        if (d[0] != 0x34u || d[7] > 2u || d[8] != 0x477640u || d[9] != 0x576b78u || d[10] != 0u)
            throw FormatError("force feedback DIEFFECT layout or pointer domain is unresolved");
        for (unsigned i = 0; i < 2; ++i)
        {
            axes[i] = s.axes_477640[i];
            direction[i] = std::bit_cast<LONG>(s.direction_576b78[i]);
        }
        descriptor.dwSize = static_cast<DWORD>(sizeof(DIEFFECT_DX5));
        descriptor.dwFlags = d[1];
        descriptor.dwDuration = d[2];
        descriptor.dwSamplePeriod = d[3];
        descriptor.dwGain = d[4];
        descriptor.dwTriggerButton = d[5];
        descriptor.dwTriggerRepeatInterval = d[6];
        descriptor.cAxes = d[7];
        descriptor.rgdwAxes = axes.data();
        descriptor.rglDirection = direction.data();
        descriptor.lpEnvelope = nullptr;
        descriptor.cbTypeSpecificParams = d[11];
        if (specific.size() == 6 && d[11] == 24u)
        {
            condition = {std::bit_cast<LONG>(specific[0]),
                         std::bit_cast<LONG>(specific[1]),
                         std::bit_cast<LONG>(specific[2]),
                         specific[3],
                         specific[4],
                         std::bit_cast<LONG>(specific[5])};
            descriptor.lpvTypeSpecificParams = &condition;
        }
        else if (specific.size() == 4 && d[11] == 16u)
        {
            periodic = {specific[0], std::bit_cast<LONG>(specific[1]), specific[2], specific[3]};
            descriptor.lpvTypeSpecificParams = &periodic;
        }
        else
            throw FormatError("force feedback type-specific source payload is unresolved");
    }
    NativeParameters(const NativeParameters &) = delete;
    NativeParameters &operator=(const NativeParameters &) = delete;
};
void transient_pointer(SourceForceFeedbackWorld &w, unsigned index, unsigned slot)
{
    // Source stores an invocation stack address into descriptor+30. No guest
    // stack exists in the x64 port. Keep that DWORD opaque, bind the native
    // pointer only in NativeParameters and record the excluded address lane.
    source(w).parameter_pointer_kind.at(index).at(slot) = ForceFeedbackParameterPointer::transient_periodic;
}
void condition_pointer(SourceForceFeedbackWorld &w, unsigned index, unsigned slot, MenuDirectInputEffectRecord &e)
{
    e.payload[28] = device_base + index * device_stride + slot * 0x78u + 8u;
    source(w).parameter_pointer_kind.at(index).at(slot) = ForceFeedbackParameterPointer::record_condition;
}
void set_parameters(SourceForceFeedbackWorld &w, unsigned index, unsigned slot, std::span<const std::uint32_t> words,
                    std::uint32_t flags)
{
    auto &e = effect(w, index, slot);
    NativeParameters native(e, source(w), words);
    (void)w.startup.set_force_feedback_parameters(w.input.acquisition, *e.effect, native.descriptor, flags);
}
} // namespace

void require_force_feedback_dispatch_binding(const SourceInputWorld &input, SourceForceFeedbackWorld &w)
{
    require_thread(w);
    auto &s = w.platform.state();
    if (&input.current_vehicle != &s.current_slot || &input.frame_pause != &s.frame_pause ||
        &input.source_device != &w.input.acquisition.source_device_pointer_576b6c ||
        &input.profiles != &w.input.profiles)
        throw FormatError("force feedback dispatcher references a different source world");
    for (unsigned i = 0; i < 4u; ++i)
        if (&input.physical[i].binding != &s.frontend_launch.input_bindings[i] ||
            &input.physical[i].feedback_mode != &w.input.physical[i].feedback_mode)
            throw FormatError("force feedback physical binding is not the Session input owner");
    for (unsigned i = 0; i < 6u; ++i)
        if (&input.vehicles[i].current != &s.vehicles.physics[i].input)
            throw FormatError("force feedback dispatcher vehicle input is not the Session owner");
}
std::uint32_t force_feedback_45ed60(SourceForceFeedbackWorld &w, unsigned physical)
{
    require_thread(w);
    if (physical >= w.input.physical.size())
        throw FormatError("force feedback physical record is outside the source bank");
    return w.input.physical[physical].feedback_mode;
}
std::uint32_t force_feedback_443770(SourceForceFeedbackWorld &w)
{
    require_thread(w);
    return w.platform.state().clock.phase;
}
void force_feedback_410a10(SourceForceFeedbackWorld &w, unsigned physical)
{
    const auto mode = force_feedback_45ed60(w, physical);
    if (mode == 0u || mode == 1u)
        return;
    auto &s = w.platform.state();
    s.current_slot = s.frontend_launch.input_bindings[physical].vehicle; // 410A61, no controller substitution
    w.input.acquisition.source_device_pointer_576b6c = 0x62d5acu + physical * physical_stride; // 410A6F
    source(w).selected_data_4d5b24 = physical_data_base + physical * physical_stride;          // 410A75
    force_feedback_4240e0(w, physical);
    if (s.scene_mode != 2u && s.frame_pause != 1u && force_feedback_443770(w) == 2u)
    {
        force_feedback_410780(w, physical);
        force_feedback_410800(w, physical);
        force_feedback_4108c0(w, physical);
        force_feedback_410950(w, physical);
    }
    else
        force_feedback_4109e0(w, physical);
}
void force_feedback_4240e0(SourceForceFeedbackWorld &w, unsigned physical)
{
    if (source(w).initialized_576b70 == 1u)
        return;
    source(w).initialized_576b70 = 1u; // 4240EA before all source/COM children
    force_feedback_424310(w, physical);
    for (unsigned slot = 0; slot < 12u; ++slot)
    { // exact first native record, regardless of physical argument
        auto *receiver = effect(w, 0, slot).effect;
        if (receiver)
            (void)w.startup.start_force_feedback(w.input.acquisition, *receiver, 1u, 0u);
    }
}
void force_feedback_424310(SourceForceFeedbackWorld &w, unsigned physical)
{
    const auto index = physical_device(physical);
    (void)force_feedback_423e40(w, index);
    force_feedback_423ff0(w, index);
    force_feedback_424030(w, index);
    force_feedback_424070(w, index);
}
std::uint32_t force_feedback_423e40(SourceForceFeedbackWorld &w, unsigned index)
{
    const std::array<std::uint32_t, 6> condition{0u, 10000u, 10000u, 10000u, 10000u, source(w).deadband_477650};
    (void)force_feedback_423d30(w, index, 0u, condition);
    return 1u; // 423E70, even CreateEffect failure
}
bool force_feedback_423d30(SourceForceFeedbackWorld &w, unsigned index, unsigned slot,
                           std::span<const std::uint32_t, 6> condition)
{
    auto &e = effect(w, index, slot);
    auto *d = e.payload.data() + 16;
    auto &s = source(w);
    for (unsigned i = 0; i < 6u; ++i)
        e.payload[i] = condition[i]; // 423D5C REP MOVSD6
    const auto gain = s.gain_47764c;
    d[2] = 0xffffffffu;
    d[5] = 0xffffffffu;
    d[7] = s.axes_count_477648;
    condition_pointer(w, index, slot, e);
    d[0] = 0x34u;
    d[4] = gain;
    d[1] = 0x12u;
    d[3] = 0u;
    d[6] = 0u;
    d[8] = 0x477640u;
    d[9] = 0x576b78u;
    d[10] = 0u;
    d[11] = 24u;
    const auto &guids = devices(w).effect_guids_576558;
    if (slot >= guids.size())
        throw FormatError("force feedback condition GUID leaves its source table");
    NativeParameters native(e, s, std::span<const std::uint32_t>{e.payload.data(), 6});
    return w.startup.create_force_feedback_effect(w.input.acquisition, index, slot, guids[slot], native.descriptor) ==
           0u;
}
void force_feedback_423dd0(SourceForceFeedbackWorld &w, unsigned index, unsigned slot,
                           std::span<const std::uint32_t, 6> condition)
{
    auto &e = effect(w, index, slot);
    if (!e.effect)
        return;
    for (unsigned i = 0; i < 6u; ++i)
        e.payload[i] = condition[i];
    e.payload[16] = 0x34u;
    e.payload[27] = 24u;
    condition_pointer(w, index, slot, e);
    set_parameters(w, index, slot, std::span<const std::uint32_t>{e.payload.data(), 6}, 0x20000100u);
}
void force_feedback_423f40(SourceForceFeedbackWorld &w, unsigned index, unsigned slot,
                           const std::array<std::uint32_t, 4> &guid, std::uint32_t duration)
{
    auto &e = effect(w, index, slot);
    auto *d = e.payload.data() + 16;
    auto &s = source(w);
    const std::array<std::uint32_t, 4> periodic{10000u, 0u, 0u, 100000u};
    d[2] = duration;
    d[4] = s.gain_47764c;
    d[7] = s.axes_count_477648;
    d[0] = 0x34u;
    transient_pointer(w, index, slot);
    d[1] = 0x12u;
    d[3] = 0u;
    d[5] = 0xffffffffu;
    d[6] = 0u;
    d[8] = 0x477640u;
    d[9] = 0x576b78u;
    d[10] = 0u;
    d[11] = 16u;
    NativeParameters native(e, s, periodic);
    (void)w.startup.create_force_feedback_effect(w.input.acquisition, index, slot, guid, native.descriptor);
    // 423FCF rereads global576550. It need not name the record just created.
    auto *receiver = effect(w, cursor_device(w), slot).effect;
    if (receiver)
        (void)w.startup.start_force_feedback(w.input.acquisition, *receiver, 1u, 0u);
}
void force_feedback_423ff0(SourceForceFeedbackWorld &w, unsigned index)
{
    constexpr std::array<std::uint32_t, 4> guid{0x13541c23u, 0x11d08e33u, 0xa000d09au, 0x356ea0c9u}; // 46FB20
    force_feedback_423f40(w, index, 4u, guid, 0xffffffffu);
}
void force_feedback_424030(SourceForceFeedbackWorld &w, unsigned index)
{
    constexpr std::array<std::uint32_t, 4> guid{0x13541c22u, 0x11d08e33u, 0xa000d09au, 0x356ea0c9u}; // 46FB10
    force_feedback_423f40(w, index, 5u, guid, 100000u);
}
void force_feedback_424070(SourceForceFeedbackWorld &w, unsigned index)
{
    constexpr std::array<std::uint32_t, 4> guid{0x13541c24u, 0x11d08e33u, 0xa000d09au, 0x356ea0c9u}; // 46FB30
    force_feedback_423f40(w, index, 6u, guid, 0xffffffffu);
}

void force_feedback_410780(SourceForceFeedbackWorld &w, unsigned physical)
{
    numeric_environment();
    const auto &p = w.platform.state().vehicles.physics[selected_vehicle(w)];
    const auto z = mul(p.basis_x.z, p.acceleration.z), y = mul(p.basis_x.y, p.acceleration.y);
    const auto x = mul(p.basis_x.x, p.acceleration.x);
    const auto acceleration = mul(std::abs(add(add(z, y), x)), 6000.0); // 46E4DC
    const auto &wheel = p.wheels[2];
    auto load = divide(wheel.load, wheel.reference_load);
    load = mul(load, wheel.surface_grip);
    load = mul(load, wheel.surface_grip);
    load = mul(load, 4000.0); // 46E4D8
    auto result = add(acceleration, load);
    if (result > 10000.0)
        result = 10000.0;
    store(selected_words(w), 0, result);
    force_feedback_4241a0(w, physical);
}
void force_feedback_410800(SourceForceFeedbackWorld &w, unsigned physical)
{
    numeric_environment();
    auto &s = w.platform.state();
    const auto index = selected_vehicle(w);
    const auto &p = s.vehicles.physics[index];
    auto &a = selected_words(w);
    const auto period = divide(400000000.0, s.vehicles.parameters[index].rpm_limit); // 46E4F0
    store(a, 7, period); // 41081D FST binary32, next comparison still uses binary64 ST0
    if (source_less(period, 30000.0))
        a[7] = 0x46ea6000u;
    if (value(a, 7) > 75000.0)
        a[7] = 0x47927c00u;
    if (source_less(p.rpm, 4096.0))
    {
        auto magnitude = add(4096.0, -double(p.rpm));
        const auto divisor =
            std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(s.vehicles.presentation[index].idle_hold) + 1u);
        magnitude = mul(magnitude, 0.000244140625);
        magnitude = mul(magnitude, 1000.0);
        magnitude = divide(magnitude, divisor);
        if (magnitude > 10000.0)
            magnitude = 10000.0;
        store(a, 6, magnitude);
    }
    else
        a[6] = 0u;
    force_feedback_424210(w, physical);
}
void force_feedback_4108c0(SourceForceFeedbackWorld &w, unsigned physical)
{
    numeric_environment();
    auto &c = w.platform.state().vehicles.contact[selected_vehicle(w)];
    auto &a = selected_words(w);
    a[1] = std::bit_cast<std::uint32_t>(c.direction_magnitude);
    const bool below = source_less(value(a, 1), double(std::bit_cast<float>(std::uint32_t{0x3dcccccdu})));
    c.direction_magnitude = 0.0f; // 4108E0, before every branch/COM call
    if (below)
        return;
    store(a, 1, mul(value(a, 1), 5000.0));
    a[2] = 0x47c35000u;
    if (!source_less(value(a, 1), 500.0))
    {
        if (value(a, 1) > 10000.0)
        {
            store(a, 2, mul(mul(value(a, 1), 100000.0), double(std::bit_cast<float>(std::uint32_t{0x38d1b717u}))));
            a[1] = 0x461c4000u;
        }
        force_feedback_424290(w, physical);
    }
    selected_words(w)[1] = 0u; // 41093E rereads selected pointer after the actual child
}
void force_feedback_410950(SourceForceFeedbackWorld &w, unsigned physical)
{
    numeric_environment();
    const auto index = selected_vehicle(w);
    auto &s = w.platform.state();
    const auto &p = s.vehicles.physics[index];
    auto sum = 0.0;
    for (unsigned i = 0; i < 4u; ++i)
    {
        const auto kind = s.vehicles.contact[index].wheels[i].type;
        if (kind != 2 && kind != 1)
            continue;
        const auto weighted = mul(p.wheels[i].load, kind == 2 ? 2000.0 : 500.0);
        sum = add(sum, divide(weighted, p.wheels[i].reference_load));
    }
    sum = mul(sum, 0.25);
    sum = mul(sum, std::abs(double(p.speed)));
    sum = mul(sum, double(std::bit_cast<float>(std::uint32_t{0x3c23d70au}))); // 46E4F8
    if (sum > 10000.0)
        sum = 10000.0;
    auto &a = selected_words(w);
    a[4] = 0x47c35000u;
    store(a, 3, sum);
    force_feedback_424120(w, physical);
}
void force_feedback_4109e0(SourceForceFeedbackWorld &w, unsigned physical)
{
    selected_words(w)[6] = 0u;
    force_feedback_424210(w, physical);
    selected_words(w)[3] = 0u;
    force_feedback_424120(w, physical);
}
void force_feedback_4241a0(SourceForceFeedbackWorld &w, unsigned physical)
{
    const auto index = physical_device(physical);
    if (!effect(w, index, 0u).effect)
        return;
    auto &a = selected_words(w);
    const auto positive = ftol(value(a, 0));
    const auto negative = ftol(value(a, 0));
    const std::array<std::uint32_t, 6> condition{0u, positive, negative, 10000u, 10000u, source(w).deadband_477650};
    force_feedback_423dd0(w, index, 0u, condition);
}
void force_feedback_424210(SourceForceFeedbackWorld &w, unsigned physical)
{
    const auto index = physical_device(physical);
    if (!effect(w, index, 4u).effect)
        return;
    auto &a = selected_words(w);
    const auto magnitude = ftol(value(a, 6));
    const auto period = ftol(value(a, 7));
    const std::array<std::uint32_t, 4> periodic{magnitude, 0u, 0u, period};
    transient_pointer(w, index, 4u);
    set_parameters(w, index, 4u, periodic, 0x100u);
}
void force_feedback_424290(SourceForceFeedbackWorld &w, unsigned physical)
{
    const auto index = physical_device(physical);
    if (!effect(w, index, 5u).effect)
        return;
    const auto magnitude = ftol(value(selected_words(w), 1));
    const auto phase =
        0u - static_cast<std::uint32_t>(vehicle_random_integer(w.platform.state().random, 1000)); // 4242CF and NEG EAX
    const auto period = ftol(value(selected_words(w), 2)); // reread4D5B24 after RNG
    const std::array<std::uint32_t, 4> periodic{magnitude, 0u, phase, period};
    transient_pointer(w, index, 5u);
    set_parameters(w, index, 5u, periodic, 0x20000100u);
}
void force_feedback_424120(SourceForceFeedbackWorld &w, unsigned physical)
{
    const auto index = physical_device(physical);
    if (!effect(w, index, 6u).effect)
        return;
    auto &a = selected_words(w);
    const auto magnitude = ftol(value(a, 3));
    const auto period = ftol(value(a, 4));
    const std::array<std::uint32_t, 4> periodic{magnitude, 0u, 0u, period};
    transient_pointer(w, index, 6u);
    set_parameters(w, index, 6u, periodic, 0x100u);
}
} // namespace sgn::retail
