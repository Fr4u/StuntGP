#include "sgn/source_application_surface_win32.hpp"
#include "sgn/race_session.hpp"
#include <Windows.h>
#include <ddraw.h>
#include <cstring>
#include <utility>

namespace sgn::retail
{
namespace
{
template <std::size_t N>
void store_word(std::array<std::optional<std::uint8_t>, N> &out, std::size_t offset, std::uint32_t word)
{
    for (unsigned i = 0; i < 4; ++i)
        out[offset + i] = static_cast<std::uint8_t>(word >> (8 * i));
}
} // namespace
SourceApplicationSurfaceWin32::SourceApplicationSurfaceWin32(IDirectDrawSurface4 &surface,
                                                             std::weak_ptr<RaceSessionPlatform> platform)
    : surface_(&surface), platform_(std::move(platform))
{
    (void)require_owner();
}
std::shared_ptr<RaceSessionPlatform> SourceApplicationSurfaceWin32::require_owner() const
{
    auto platform = platform_.lock();
    if (!platform)
        throw FormatError("Source surface platform lifetime ended");
    platform->require_owner_thread();
    if (!surface_)
        throw FormatError("Source surface interface was already released");
    return platform;
}
std::uint32_t SourceApplicationSurfaceWin32::is_lost_slot60()
{
    const auto owner = require_owner();
    return static_cast<std::uint32_t>(surface_->IsLost());
}
std::uint32_t SourceApplicationSurfaceWin32::restore_slot6c()
{
    const auto owner = require_owner();
    return static_cast<std::uint32_t>(surface_->Restore());
}
std::uint32_t SourceApplicationSurfaceWin32::release_slot08()
{
    const auto owner = require_owner();
    const auto remaining = surface_->Release();
    if (remaining == 0)
        surface_ = nullptr;
    return remaining;
}
std::uint32_t SourceApplicationSurfaceWin32::capabilities_slot38(std::array<std::optional<std::uint32_t>, 4> &out)
{
    const auto owner = require_owner();
    DDSCAPS2 caps{}; // host scratch; not source-initialized output
    const auto result = surface_->GetCaps(&caps);
    for (auto &word : out)
        word.reset();
    if (result == DD_OK)
    {
        out[0] = caps.dwCaps;
        out[1] = caps.dwCaps2;
        out[2] = caps.dwCaps3;
        out[3] = caps.dwCaps4;
    }
    return static_cast<std::uint32_t>(result);
}
std::uint32_t SourceApplicationSurfaceWin32::describe_slot58(SourceSurfaceDescriptor &out)
{
    const auto owner = require_owner();
    DDSURFACEDESC2 descriptor{};
    descriptor.dwSize = sizeof(descriptor); // native SDK size, including x64 pointers
    const auto result = surface_->GetSurfaceDesc(&descriptor);
    for (auto &byte : out)
        byte.reset();
    if (result == DD_OK)
    {
        store_word(out, 0, 124); // translate known source ABI size
        store_word(out, 4, descriptor.dwFlags);
        if ((descriptor.dwFlags & DDSD_PITCH) != 0)
            store_word(out, 16, static_cast<std::uint32_t>(descriptor.lPitch));
    }
    return static_cast<std::uint32_t>(result);
}
std::uint32_t SourceApplicationSurfaceWin32::pixel_format_slot54(std::array<std::optional<std::uint8_t>, 32> &out)
{
    const auto owner = require_owner();
    DDPIXELFORMAT format{};
    static_assert(sizeof(format) == 32);
    format.dwSize = sizeof(format);
    const auto result = surface_->GetPixelFormat(&format);
    for (auto &byte : out)
        byte.reset();
    if (result == DD_OK)
    {
        std::array<std::uint8_t, 32> bytes;
        std::memcpy(bytes.data(), &format, bytes.size());
        for (std::size_t i = 0; i < bytes.size(); ++i)
            out[i] = bytes[i];
    }
    return static_cast<std::uint32_t>(result);
}
} // namespace sgn::retail
