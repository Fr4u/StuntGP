#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <type_traits>

namespace sgn
{
// New platform clock. Retail 0x434840 establishes 60 updates/s. Preserve that
// rate, not its 32-bit QPC wrap bug. Nanoseconds * 60 is an exact rational phase.
class TickClock
{
    std::int64_t phase_{};
    std::uint64_t ticks_{};
    std::uint64_t discarded_ns_{};

  public:
    template <class Tick> unsigned advance(std::chrono::nanoseconds elapsed, Tick tick)
    {
        if (elapsed.count() < 0)
            throw std::invalid_argument("negative clock delta");
        constexpr std::int64_t max_delta = 250'000'000;
        const auto accepted = std::min(elapsed.count(), max_delta);
        discarded_ns_ += static_cast<std::uint64_t>(elapsed.count() - accepted);
        phase_ += accepted * 60;
        unsigned count = 0;
        while (phase_ >= 1'000'000'000)
        {
            // A host session can end inside a catch-up batch. A rejected step
            // must not advance the clock or leak stale time into its successor.
            if constexpr (std::is_same_v<std::invoke_result_t<Tick &, std::uint64_t>, bool>)
            {
                if (!tick(ticks_))
                {
                    phase_ = 0;
                    break;
                }
            }
            else
                tick(ticks_);
            ++ticks_;
            ++count;
            phase_ -= 1'000'000'000;
        }
        return count;
    }
    void suspend() noexcept
    {
        phase_ = 0;
    }
    [[nodiscard]] double alpha() const noexcept
    {
        return static_cast<double>(phase_) / 1'000'000'000.0;
    }
    [[nodiscard]] std::uint64_t ticks() const noexcept
    {
        return ticks_;
    }
    [[nodiscard]] std::uint64_t discarded_ns() const noexcept
    {
        return discarded_ns_;
    }
};
} // namespace sgn
