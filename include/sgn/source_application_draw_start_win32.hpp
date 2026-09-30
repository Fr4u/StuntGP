#pragma once
#include <cstdint>
struct IDirectDraw;
struct IDirectDraw4;
struct _GUID;

namespace sgn::retail
{
struct SourceDrawStartupFailure
{
    virtual ~SourceDrawStartupFailure() = default;
    virtual const char *describe_error_422690(std::uint32_t) = 0;
    [[noreturn]] virtual void exit_44e4a0(const char *description) = 0;
};
// Destination references are the retained native pointer slots themselves.
// No temporary COM owner, automatic Release, or additional QueryInterface.
std::uint32_t create_source_draw_4227e0(IDirectDraw *&destination, _GUID *adapter, std::uintptr_t window,
                                        SourceDrawStartupFailure &);
std::uint32_t query_source_draw4_422820(IDirectDraw *const &source, IDirectDraw4 *&destination, std::uintptr_t window,
                                        SourceDrawStartupFailure &);
std::uint32_t query_source_video_memory_422950(IDirectDraw4 *const &device_61c380, std::uint32_t &video_total,
                                               std::uint32_t &texture_total, std::uint32_t &video_free);
} // namespace sgn::retail
