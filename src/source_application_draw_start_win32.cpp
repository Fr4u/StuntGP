#include "sgn/source_application_draw_start_win32.hpp"
#include <Windows.h>
#include <ddraw.h>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
void require_success(HRESULT result, SourceDrawStartupFailure &failure)
{
    if (result != 0)
    {
        const char *description = failure.describe_error_422690(static_cast<std::uint32_t>(result));
        failure.exit_44e4a0(description);
    }
}
// Bytes at retail46ECB0, interpreted as GUID fields, matched to SDK Draw4.
constexpr GUID source_draw4_iid{0x9c59509a, 0x39bd, 0x11d1, {0x8c, 0x4a, 0x00, 0xc0, 0x4f, 0xd9, 0x30, 0xc5}};
} // namespace
std::uint32_t create_source_draw_4227e0(IDirectDraw *&destination, GUID *adapter, std::uintptr_t window,
                                        SourceDrawStartupFailure &failure)
{
    const HRESULT creation = DirectDrawCreate(adapter, &destination, nullptr);
    require_success(creation, failure);
    // 422780 reloads the destination after creation, then slot50(HWND,13).
    auto *receiver = destination;
    if (!receiver)
        throw std::logic_error("422780 reached a null DirectDraw receiver");
    const HRESULT cooperative = receiver->SetCooperativeLevel(reinterpret_cast<HWND>(window), 0x13u);
    require_success(cooperative, failure);
    return static_cast<std::uint32_t>(creation);
}
std::uint32_t query_source_draw4_422820(IDirectDraw *const &source, IDirectDraw4 *&destination, std::uintptr_t window,
                                        SourceDrawStartupFailure &failure)
{
    auto *receiver = source;
    if (!receiver)
        throw std::logic_error("422820 reached a null DirectDraw receiver");
    const HRESULT query = receiver->QueryInterface(source_draw4_iid, reinterpret_cast<void **>(&destination));
    require_success(query, failure);
    auto *queried_receiver = destination; // 4227B0 fresh destination read
    if (!queried_receiver)
        throw std::logic_error("4227B0 reached a null DirectDraw4 receiver");
    const HRESULT cooperative = queried_receiver->SetCooperativeLevel(reinterpret_cast<HWND>(window), 0x13u);
    require_success(cooperative, failure);
    return static_cast<std::uint32_t>(query);
}
std::uint32_t query_source_video_memory_422950(IDirectDraw4 *const &device, std::uint32_t &video_total,
                                               std::uint32_t &texture_total, std::uint32_t &video_free)
{
    DDSCAPS2 caps{};
    caps.dwCaps = 0x4000u;
    DWORD total = 0, free = 0; // host scratch; no publication on API failure
    auto *first = device;
    if (!first)
        throw std::logic_error("42296E reached null DirectDraw4");
    HRESULT result = first->GetAvailableVidMem(&caps, &total, &free);
    if (result != 0)
        return static_cast<std::uint32_t>(result);
    video_total = total; // 422992, first reached caller output
    video_free = free;   // 422996, preserved if the next query fails
    caps = {};
    caps.dwCaps = 0x1000u;
    auto *second = device; // 4229AB rereads the global after first call
    if (!second)
        throw std::logic_error("4229AB reached null DirectDraw4");
    result = second->GetAvailableVidMem(&caps, &total, &free);
    if (result != 0)
        return static_cast<std::uint32_t>(result);
    texture_total = total; // 4229CC; never publishes the second free count
    return 0;
}
} // namespace sgn::retail
