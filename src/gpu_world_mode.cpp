// NEW HOST rendering (gpu_world R19): runtime switch between the CPU reference
// world path (mode 0, default) and the hybrid GPU path (mode 1). Not game state.
#include "sgn/detail/gpu_world_ticket.hpp"
#include <atomic>
#include <cstdlib>
#include <cstring>

namespace sgn::gpu_world
{
namespace
{
Mode initial_mode() noexcept
{
    // _dupenv_s keeps /W4 /WX clean (getenv is deprecated under MSVC).
    char *value = nullptr;
    std::size_t length = 0;
    Mode mode = Mode::cpu_reference;
    if (_dupenv_s(&value, &length, "SGN_GPU_WORLD") == 0 && value)
    {
        if (std::strcmp(value, "1") == 0)
            mode = Mode::hybrid_exact;
        std::free(value);
    }
    return mode;
}
std::atomic<std::uint32_t> &mode_word() noexcept
{
    static std::atomic<std::uint32_t> word{static_cast<std::uint32_t>(initial_mode())};
    return word;
}
std::atomic<bool> &available_word() noexcept
{
    static std::atomic<bool> word{false};
    return word;
}
std::atomic<void (*)(Mode)> &listener() noexcept
{
    static std::atomic<void (*)(Mode)> value{nullptr};
    return value;
}
} // namespace
Mode requested_mode() noexcept
{
    return static_cast<Mode>(mode_word().load(std::memory_order_relaxed));
}
void set_requested_mode(Mode mode)
{
    mode_word().store(static_cast<std::uint32_t>(mode), std::memory_order_relaxed);
    if (const auto notify = listener().load(std::memory_order_relaxed))
        notify(mode);
}
bool backend_available() noexcept
{
    return available_word().load(std::memory_order_relaxed);
}
namespace detail
{
void set_backend_available(bool available) noexcept
{
    available_word().store(available, std::memory_order_relaxed);
}
void set_mode_listener(void (*notify)(Mode)) noexcept
{
    listener().store(notify, std::memory_order_relaxed);
}
} // namespace detail
} // namespace sgn::gpu_world
