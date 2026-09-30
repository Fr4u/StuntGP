#include "sgn/menu_directinput_acquisition.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cstring>

namespace sgn::retail
{
namespace
{
static_assert(std::endian::native == std::endian::little);
template <class T> T read(const T &value) noexcept
{
    return static_cast<const volatile T &>(value);
}
template <class T> void store(T &target, T value) noexcept
{
    static_cast<volatile T &>(target) = value;
}
using sgn::bit_reinterpret::as_signed;
std::uint32_t packet_word(std::span<const std::byte, 80> packet, unsigned index) noexcept
{
    std::uint32_t result;
    std::memcpy(&result, packet.data() + index * 4, 4);
    return result;
}
std::uint32_t wrapped_abs(std::uint32_t word) noexcept
{
    const auto sign = (word & 0x80000000u) != 0 ? 0xffffffffu : 0u;
    return (word ^ sign) - sign; // sourceCDQ/XOR/SUB, including80000000
}
void convert_and_clamp(std::uint32_t &axis, const std::uint32_t &range) noexcept
{
    const auto result = direct_input_axis_word_4245c6(read(axis));
    store(axis, result); // 4245D9 /424601, before reading the range
    const auto magnitude = wrapped_abs(result);
    const auto limit = read(range);
    if (as_signed(magnitude) > as_signed(limit)) // source signedJLE
        store(axis, as_signed(result) > 0 ? limit : std::uint32_t{0} - limit);
}
void deadzone(std::uint32_t &axis) noexcept
{
    // 42461E..424631 /424640..424653: FILD of WRAPPED signed absolute word.
    //  Pinned46E68C is1250.0 exactly. INT_MIN wrapped abs is negative and
    //  therefore follows the source clear branch; std::abs(int32) would be UB.
    if (as_signed(wrapped_abs(read(axis))) < 1250)
        store(axis, std::uint32_t{0});
}
} // namespace

std::uint32_t direct_input_axis_word_4245c6(std::uint32_t raw_axis) noexcept
{
    constexpr std::int64_t numerator = 2139095;
    constexpr std::int64_t denominator = 2097152;
    const auto value = static_cast<std::int64_t>(as_signed(raw_axis));
    const auto integer64 = (value * numerator) / denominator; // exact truncate toward0
    return static_cast<std::uint32_t>(integer64);             // defined modulo2^32, sourceEAX
}

std::uint32_t acquire_direct_input_424520(MenuKeyboardRawState &raw, const MenuDirectInputRegistry &registry,
                                          std::uint32_t slot)
{
    const auto count = read(registry.device_count); // 42452A, latched once
    const auto index = slot - std::uint32_t{1};     // 424532, wrap exactly
    if (count == 0 || index >= count)
        return 0; // 424530..424539, no device/output access
    if (index >= registry.devices.size() || slot >= raw.upstream.size())
        throw FormatError("DirectInput424520 source caller/table extent");
    auto *const device = registry.devices[index]; // 424544, retain across all three OS calls
    if (device == nullptr)
        throw FormatError("DirectInput424520 missing actual device receiver");
    (void)device->poll_42454e();                          // 42454E, failure does NOT skip GetDeviceState
    alignas(4) std::array<std::byte, 80> packet;          // source local output, no fabricated zero packet
    const auto status = device->get_state_42455a(packet); // 42455A, size50hex
    if (status != 0)
    { // 42455D..424564, not SUCCEEDED(status)
        (void)device->acquire_424564();
        return 0; // no retry and no source-state publication
    }
    auto &out = raw.upstream[slot].words; // sourceESI=576624+slot*88 =record+C
    store(out[3], std::uint32_t{2});      // 424586, upstream+C
    store(out[5], std::uint32_t{5000});   // 42458C, upstream+14
    for (unsigned i = 0; i < 10; ++i)
        store(out[6 + i], packet_word(packet, i)); // 424593..4245C3
    // OS offsets28/2C (POV2/3) are NOT copied by this source. Preserve upstream
    // words16/17. Both conversions precede both deadzone tests, as in the PE.
    convert_and_clamp(out[6], out[5]); // 4245C6..4245EB
    convert_and_clamp(out[7], out[5]); // 4245EE..424613
    deadzone(out[6]);                  // 424616..424631
    deadzone(out[7]);                  // 424638..424653
    for (unsigned i = 0; i < 8; ++i)
        store(out[26 + i], read(out[18 + i])); // 424667, current→previous32bytes
    for (unsigned i = 0; i < 8; ++i)
        store(out[18 + i], packet_word(packet, 12 + i)); // 424678, rawbuttons32bytes
    return 1;
}

void acquire_devices_4243f0(MenuKeyboardRawState &raw, RaceInitializationState &keyboard, MenuKeyboardAcquisition &keys,
                            const MenuDirectInputRegistry &registry)
{
    for (std::uint32_t slot = 0; slot < 10; ++slot)
    {
        const auto &words = raw.upstream[slot].words;
        if (read(words[0]) != 1u)
            continue;
        const auto kind = read(words[1]);
        if (kind - std::uint32_t{1} > 5u)
            continue;
        if (kind == 1u)
        {
            if (slot == 0)
                keys.acquire_45ec00(menu_keyboard_bytes(keyboard));
        }
        else
        {
            (void)acquire_direct_input_424520(raw, registry, slot); // 424426 ignores sourceEAX
        }
    }
}

void acquire_publish_devices_prefix_45f210(MenuKeyboardRawState &raw, SessionMenuInputState &menu,
                                           RaceInitializationState &keyboard, std::uint32_t &second_pause,
                                           MenuKeyboardAcquisition &keys, const MenuDirectInputRegistry &registry)
{
    store(second_pause, std::uint32_t{0}); // 45F221
    for (unsigned i = 0; i < 64; ++i)
        store(keyboard.fields_62dcf0[i], read(keyboard.fields_62d480[i])); // 45F22B
    acquire_devices_4243f0(raw, keyboard, keys, registry);                 // 45F22D
    const auto from = menu_upstream_publication_views(raw);
    const auto to = menu_physical_publication_views(raw, menu);
    publish_physical_input_4244c0(from, to); // 45F232; ordinary OS failure still reaches this call
}
} // namespace sgn::retail
