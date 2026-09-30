#include "sgn/source_application_surface_creation.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
namespace
{
template <std::size_t N>
std::uint32_t load(const std::array<std::optional<std::uint8_t>, N> &descriptor, std::size_t offset)
{
    std::uint32_t word = 0;
    for (unsigned i = 0; i < 4; ++i)
    {
        if (!descriptor[offset + i])
            throw FormatError("422AC0 reads unknown descriptor capabilities");
        word |= std::uint32_t(*descriptor[offset + i]) << (8 * i);
    }
    return word;
}
void store(SourceSurfaceDescriptor &descriptor, std::size_t offset, std::uint32_t word)
{
    for (unsigned i = 0; i < 4; ++i)
        descriptor[offset + i] = static_cast<std::uint8_t>(word >> (8 * i));
}
SourceSurfaceDescriptor primary_descriptor(std::uint32_t flags, bool attached)
{
    SourceSurfaceDescriptor descriptor;
    for (auto &byte : descriptor)
        byte = std::uint8_t{0}; // REP STOSD: exactly31 DWORDs
    store(descriptor, 0, 124);
    store(descriptor, 4, attached ? 0x21u : 1u);
    if (attached)
        store(descriptor, 20, 1);
    store(descriptor, 104, flags);
    return descriptor;
}
} // namespace

void source_decode_color_mask_422bb0(std::uint32_t mask, SourceColorMaskWords words)
{
    std::uint32_t count = 0;
    std::uint32_t first = 0xffffffffu;
    for (std::uint32_t bit = 0; bit < 16; ++bit)
    {
        if ((mask & 1u) != 0)
        {
            if (first == 0xffffffffu)
                first = bit;
            ++count;
        }
        mask >>= 1;
    }
    // Count precedes first-bit store, including aliased destination words.
    words.bit_count = count;
    words.first_bit = first;
}

void SourceApplicationSurfaceCreation::configure_16bit_422bf0()
{
    auto *surface = global_back_surface_61c3a0();
    std::array<std::optional<std::uint8_t>, 32> format;
    format[0] = std::uint8_t{32};
    format[1] = std::uint8_t{0};
    format[2] = std::uint8_t{0};
    format[3] = std::uint8_t{0};
    if (!surface)
        throw FormatError("422C06 requires the source61C3A0 surface");
    (void)pixel_format_slot54(*surface, format);
    if (!format[4])
        throw FormatError("422C0C reads unwritten pixel format flags");
    if ((*format[4] & 0x40u) == 0)
        return;
    if (load(format, 12) != 16)
        return;
    for (unsigned channel = 0; channel < 3; ++channel)
    {
        const auto mask = load(format, 16 + 4 * channel);
        source_decode_color_mask_422bb0(mask, color_mask_words(channel));
    }
}

std::uint32_t SourceApplicationSurfaceCreation::primary_422a80(SourceSurfaceDescriptor &descriptor,
                                                               SourceApplicationSurfaceSlot &destination,
                                                               std::uint32_t failure_policy)
{
    const auto result = create_surface_slot18(descriptor, destination);
    if (result == 0)
    {
        register_surface_422a00(destination);
    }
    else if (failure_policy == 1)
    {
        exit_44e4a0(); // same CRT exit(1) reached directly at422AB1
    }
    return result;
}

void SourceApplicationSurfaceCreation::register_surface_422a00(SourceApplicationSurfaceSlot &destination)
{
    register_source_surface_422a00(surface_registry(), destination.receiver);
}

std::uint32_t SourceApplicationSurfaceCreation::secondary_422ac0(SourceApplicationSurfaceSlot &destination,
                                                                 std::uint32_t width, std::uint32_t height,
                                                                 std::uint32_t caps)
{
    auto descriptor = primary_descriptor(flags_5728dc_ | caps | 0x40u, false);
    store(descriptor, 4, 7);
    store(descriptor, 8, height);
    store(descriptor, 12, width);
    if (caps == 0x4000u)
    {
        const auto result = primary_422a80(descriptor, destination, 2);
        if (result == 0)
            return result;
        // The original reloads local caps after the first API call. Preserve
        // any descriptor mutation rather than recomputing from input flags.
    }
    store(descriptor, 104, (load(descriptor, 104) & 0xffffbfffu) | 0x800u);
    return primary_422a80(descriptor, destination, 2);
}

std::uint32_t SourceApplicationSurfaceCreation::create_422df0(std::uint32_t, std::uint32_t)
{
    auto descriptor = primary_descriptor(flags_5728dc_ | 0x4218u, true);
    (void)primary_422a80(descriptor, primary_, 2); // deliberately ignored HRESULT
    auto *primary = primary_.receiver;             // read AFTER creation
    if (!primary)
        throw FormatError("422E66 requires the created primary surface");
    std::array<std::optional<std::uint32_t>, 4> caps{4u, 0u, 0u, 0u};
    const auto result = attached_slot30(*primary, caps, destination_);
    if (result != 0)
    {
        describe_error_422690(result);
        exit_44e4a0();
    }
    selected_slot_61c358_ = &destination_;
    created_5728d8_ = 2;
    return result;
}

std::uint32_t SourceApplicationSurfaceCreation::fallback_422c60(std::uint32_t, std::uint32_t, std::uint32_t depth)
{
    // The source discards requested width/height here, but keeps depth.
    if (set_display_mode_slot54(640, 480, depth, 0, 0) != 0)
        return 0xffffffffu;
    auto descriptor = primary_descriptor(flags_5728dc_ | 0x4200u, false);
    if (primary_422a80(descriptor, primary_, 2) != 0)
        return 0xffffffffu;
    auto result = secondary_422ac0(destination_, 640, 480, 0x4000u);
    if (result != 0)
    {
        // This extra800 attempt is outside422AC0's own fallback. Do not
        // collapse repeated requests; the source issues both when reached.
        result = secondary_422ac0(destination_, 640, 480, 0x800u);
        if (result != 0)
        {
            describe_error_422690(result);
            exit_44e4a0();
        }
    }
    selected_slot_61c358_ = &destination_;
    created_5728d8_ = 4;
    return result;
}

std::uint32_t SourceApplicationSurfaceCreation::create_422d50(std::uint32_t width, std::uint32_t height)
{
    auto descriptor = primary_descriptor(flags_5728dc_ | 0x4200u, false);
    if (primary_422a80(descriptor, primary_, 1) != 0)
        return 0xffffffffu;
    const auto result = secondary_422ac0(destination_, width, height, 0x800u);
    if (result != 0)
    {
        describe_error_422690(result);
        exit_44e4a0();
    }
    selected_slot_61c358_ = &destination_;
    created_5728d8_ = 1;
    return result;
}

std::uint32_t SourceApplicationSurfaceCreation::create_422ea0(std::uint32_t, std::uint32_t)
{
    auto descriptor = primary_descriptor(flags_5728dc_ | 0xa18u, true);
    (void)primary_422a80(descriptor, primary_, 2);
    auto *primary = primary_.receiver;
    if (!primary)
        throw FormatError("422F06 requires the created primary surface");
    std::array<std::optional<std::uint32_t>, 4> caps;
    caps[0] = 4; // the remaining three stack words are NOT initialized here
    const auto result = attached_slot30(*primary, caps, destination_);
    selected_slot_61c358_ = &destination_;
    created_5728d8_ = 3;
    return result; // EAX survives the epilogue, despite decompiler's void type
}
} // namespace sgn::retail
