#include "sgn/source_host_counter.hpp"
#include <windows.h>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
std::int64_t query_counter()
{
    LARGE_INTEGER value{};
    if (!QueryPerformanceCounter(&value))
        throw std::runtime_error("QueryPerformanceCounter failed");
    return value.QuadPart;
}
} // namespace
// NEW repair (PORT-FIX proposal P1): see header. The base is taken once; the
// source still performs its own two distinct reads at 434869/434873 and
// 442100/44210A, and one read per 442490 call at 4424A8.
SourceHostRebasedCounter::SourceHostRebasedCounter()
{
    LARGE_INTEGER value{};
    if (!QueryPerformanceFrequency(&value) || value.QuadPart <= 0)
        throw std::runtime_error("QueryPerformanceFrequency failed");
    native_frequency_ = value.QuadPart;
    base_ = query_counter();
}
std::uint64_t SourceHostRebasedCounter::counter()
{
    const auto elapsed = query_counter() - base_;
    if (elapsed < 0)
        throw std::runtime_error("QueryPerformanceCounter moved backwards");
    const auto ticks = static_cast<std::uint64_t>(elapsed);
    const auto frequency = static_cast<std::uint64_t>(native_frequency_);
    // Split to avoid 64-bit overflow; floor keeps the value monotonic.
    return ticks / frequency * virtual_frequency + ticks % frequency * virtual_frequency / frequency;
}
double SourceHostRebasedCounter::precise_counter() const
{
    const auto elapsed = query_counter() - base_;
    if (elapsed < 0)
        throw std::runtime_error("QueryPerformanceCounter moved backwards");
    const auto ticks = static_cast<std::uint64_t>(elapsed);
    const auto frequency = static_cast<std::uint64_t>(native_frequency_);
    // Integer part exactly as counter(); only the remainder is kept fractional.
    return double(ticks / frequency * virtual_frequency) +
           double(ticks % frequency * virtual_frequency) / double(frequency);
}
std::uint64_t SourceHostRebasedCounter::frequency()
{
    return virtual_frequency;
}
} // namespace sgn::retail
