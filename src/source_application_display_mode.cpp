#include "sgn/source_application_display_mode.hpp"
#include "sgn/race_session.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
std::uint32_t descriptor_word(const SourceSurfaceDescriptor &descriptor, std::size_t offset)
{
    std::uint32_t value = 0;
    for (std::size_t i = 0; i != 4; ++i)
    {
        if (!descriptor[offset + i])
            throw FormatError("422F30 reads unwritten surface descriptor bytes");
        value |= std::uint32_t(*descriptor[offset + i]) << (8 * i);
    }
    return value;
}
} // namespace

std::uint32_t source_set_display_mode_422f30(SourceDisplayModeWorld world, SourceDisplayModeInvocation &call,
                                             std::uint32_t width, std::uint32_t height, std::uint32_t depth)
{
    auto &viewport = world.viewport;
    if (std::bit_cast<std::uint32_t>(viewport.width) == width &&
        std::bit_cast<std::uint32_t>(viewport.height) == height &&
        std::bit_cast<std::uint32_t>(viewport.depth_bits) == depth)
        return 0;

    world.surface_created_5728d8 = 0;
    auto result = call.set_display_mode_slot54(width, height, depth, 0, 0);
    if (result != 0)
    {
        if (world.surface_created_5728d8 == 0)
            result = call.fallback_422c60(width, height, depth);
        if (result != 0)
        {
            result = call.set_display_mode_slot54(width, height, depth, 0, 0);
            if (result != 0)
                return result;
        }
    }
    // Keep EAX from the actual preceding child; do not infer failure or
    // success from the creation flag. Re-read the flag after every attempt.
    if (world.surface_created_5728d8 == 0)
    {
        result = call.create_422df0(width, height);
        if (world.surface_created_5728d8 == 0)
        {
            result = call.create_422d50(width, height);
            if (world.surface_created_5728d8 == 0)
                result = call.create_422ea0(width, height);
        }
    }
    if (result != 0)
        return result;
    if (depth == 16)
        call.configure_16bit_422bf0();

    auto *surface = call.global_surface_61c368();
    SourceSurfaceDescriptor descriptor; // only dwSize is initialized by source
    descriptor[0] = std::uint8_t{124};
    descriptor[1] = std::uint8_t{0};
    descriptor[2] = std::uint8_t{0};
    descriptor[3] = std::uint8_t{0};
    // Source dereferences the captured surface before publishing dimensions.
    if (!surface)
        throw FormatError("42302F requires the source61C368 surface");
    viewport.width = std::bit_cast<std::int32_t>(width);
    viewport.height = std::bit_cast<std::int32_t>(height);
    viewport.depth_bits = std::bit_cast<std::int32_t>(depth);
    world.pitch_576b88 = width;
    (void)call.describe_surface_slot58(*surface, descriptor);
    // TEST reads a byte of flags, not the whole DWORD. Preserve partial writes.
    if (!descriptor[4])
        throw FormatError("42304D reads unwritten surface flags");
    if ((*descriptor[4] & 8u) != 0)
        world.pitch_576b88 = descriptor_word(descriptor, 16);
    return 0;
}
} // namespace sgn::retail
