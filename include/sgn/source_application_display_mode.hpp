#pragma once
#include "sgn/retail_viewport.hpp"
#include "sgn/source_application_surfaces.hpp"
#include <array>
#include <optional>

namespace sgn::retail
{
// These views borrow existing process words; width/height/depth stay in the
// sole ViewportState. The two remaining words need native-window owner binding.
struct SourceDisplayModeWorld
{
    ViewportState &viewport;
    std::uint32_t &pitch_576b88;
    std::uint32_t &surface_created_5728d8;
};
using SourceSurfaceDescriptor = std::array<std::optional<std::uint8_t>, 124>;

// Invocation captures the ECX device once and the two destination slot
// addresses once. Its implementations must keep those identities on all calls.
// The final descriptor query instead reads the global61C368 when reached.
struct SourceDisplayModeInvocation
{
    virtual ~SourceDisplayModeInvocation() = default;
    virtual std::uint32_t set_display_mode_slot54(std::uint32_t width, std::uint32_t height, std::uint32_t depth,
                                                  std::uint32_t refresh, std::uint32_t flags) = 0;
    virtual std::uint32_t fallback_422c60(std::uint32_t width, std::uint32_t height, std::uint32_t depth) = 0;
    virtual std::uint32_t create_422df0(std::uint32_t width, std::uint32_t height) = 0;
    virtual std::uint32_t create_422d50(std::uint32_t width, std::uint32_t height) = 0;
    virtual std::uint32_t create_422ea0(std::uint32_t width, std::uint32_t height) = 0;
    virtual void configure_16bit_422bf0() = 0;
    virtual SourceApplicationSurface *global_surface_61c368() = 0;
    virtual std::uint32_t describe_surface_slot58(SourceApplicationSurface &, SourceSurfaceDescriptor &) = 0;
};
std::uint32_t source_set_display_mode_422f30(SourceDisplayModeWorld, SourceDisplayModeInvocation &, std::uint32_t width,
                                             std::uint32_t height, std::uint32_t depth);
} // namespace sgn::retail
