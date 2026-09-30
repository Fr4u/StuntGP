#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace sgn
{
struct SampleSummary
{
    std::uint64_t total_samples{};
    std::size_t window_samples{};
    double p50{}, p95{}, p99{}, maximum{};
};
// New platform diagnostics. Constant memory and no allocation when recording.
// Percentiles cover the most recent Capacity samples; totals are lifetime counts.
template <std::size_t Capacity = 16384> class SampleWindow
{
    static_assert(Capacity > 0);
    std::array<double, Capacity> values_{};
    std::size_t next_{}, size_{};
    std::uint64_t total_{};

  public:
    void record(double value)
    {
        if (!std::isfinite(value) || value < 0)
            throw std::invalid_argument("invalid timing sample");
        values_[next_] = value;
        next_ = (next_ + 1) % Capacity;
        size_ = std::min(size_ + 1, Capacity);
        ++total_;
    }
    [[nodiscard]] SampleSummary summarize() const
    {
        SampleSummary result{total_, size_};
        if (!size_)
            return result;
        auto ordered = values_;
        std::sort(ordered.begin(), ordered.begin() + size_);
        auto percentile = [&](double p) {
            return ordered[static_cast<std::size_t>(p * static_cast<double>(size_ - 1))];
        };
        result.p50 = percentile(0.5);
        result.p95 = percentile(0.95);
        result.p99 = percentile(0.99);
        result.maximum = ordered[size_ - 1];
        return result;
    }
};
} // namespace sgn
