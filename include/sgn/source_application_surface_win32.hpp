#pragma once
#include "sgn/source_application_display_mode.hpp"
#include "sgn/race_session_platform.hpp"
#include <memory>

struct IDirectDrawSurface4;
namespace sgn::retail
{
// Native fidelity adapter. The caller supplies a confirmed Surface4 interface
// and retains this wrapper independently of COM Release and source aliases.
// No QueryInterface, AddRef or implicit destructor Release is performed.
class SourceApplicationSurfaceWin32 final : public SourceApplicationSurface
{
    IDirectDrawSurface4 *surface_;
    std::weak_ptr<RaceSessionPlatform> platform_;
    std::shared_ptr<RaceSessionPlatform> require_owner() const;

  public:
    SourceApplicationSurfaceWin32(IDirectDrawSurface4 &, std::weak_ptr<RaceSessionPlatform>);
    SourceApplicationSurfaceWin32(const SourceApplicationSurfaceWin32 &) = delete;
    SourceApplicationSurfaceWin32 &operator=(const SourceApplicationSurfaceWin32 &) = delete;
    std::uint32_t is_lost_slot60() override;
    std::uint32_t restore_slot6c() override;
    std::uint32_t release_slot08() override;
    std::uint32_t capabilities_slot38(std::array<std::optional<std::uint32_t>, 4> &) override;
    // These project only fields consumed by recovered application functions.
    // Other source descriptor bytes remain unknown; no memcpy of native x64
    // DDSURFACEDESC2 into the original124-byte layout is permitted.
    std::uint32_t describe_slot58(SourceSurfaceDescriptor &);
    std::uint32_t pixel_format_slot54(std::array<std::optional<std::uint8_t>, 32> &);
};
} // namespace sgn::retail
