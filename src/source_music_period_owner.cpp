#include "source_music_period_owner.hpp"
#include <algorithm>
#include <exception>
#include <limits>
#include <stdexcept>
#include <utility>

namespace sgn::retail
{
SourceMusicPeriodSnapshot SourceMusicPeriodReport::snapshot() const
{
    if (std::this_thread::get_id() != thread_)
        throw std::logic_error("music period report belongs to its creator thread");
    return values_;
}

SourceMusicPeriodOwner::SourceMusicPeriodOwner(std::shared_ptr<SourceMusicPeriodCalls> calls)
    : calls_(std::move(calls)), report_(std::make_shared<SourceMusicPeriodReport>())
{
    if (!calls_)
        throw std::invalid_argument("music period owner requires an owned API backend");
}

void SourceMusicPeriodOwner::check() const
{
    // Check immutable thread identity before mutable state or native calls.
    if (std::this_thread::get_id() != thread_)
        throw std::logic_error("music period owner used from another thread");
    if (busy_)
        throw std::logic_error("music period owner does not permit API reentry");
}

void SourceMusicPeriodOwner::increment(std::uint64_t &value) noexcept
{
    if (value != std::numeric_limits<std::uint64_t>::max())
        ++value;
    else
        report_->values_.counters_saturated = true;
}

std::uint32_t SourceMusicPeriodOwner::begin(std::uint32_t period)
{
    check();
    // The only per-request allocation happens BEFORE Begin. Failure leaves
    // the ledger and counters unchanged, and reaches no native call.
    pending_.push_back({period, 0});
    auto &slot = pending_.back();
    auto &out = report_->values_;
    increment(out.begin_calls);
    out.pending_calls = static_cast<std::uint64_t>(pending_.size());
    busy_ = out.native_call_in_progress = true;
    const auto result = calls_->begin(period);
    // No allocation, exception, provider dispatch or fallible acceptance
    // follows the returned result. Retain each reached Begin for pairing,
    // including nonzero returns; this is explicit NEW host accounting.
    slot.begin_result = result;
    increment(result == 0 ? out.begin_successes : out.begin_failures);
    busy_ = out.native_call_in_progress = false;
    return result;
}

void SourceMusicPeriodOwner::record_end(std::uint32_t result) noexcept
{
    auto &out = report_->values_;
    increment(out.end_calls);
    increment(result == 0 ? out.end_successes : out.end_failures);
}

std::uint32_t SourceMusicPeriodOwner::end(std::uint32_t period)
{
    check();
    auto &out = report_->values_;
    const auto found = std::find_if(pending_.begin(), pending_.end(),
                                    [period](const Obligation &item) { return item.period == period; });
    // This NEW owner API cannot release another owner's request. A future
    // source45FD00 adapter must establish a matching recorded Begin first;
    // this guard is not presented as recovered retail error behavior.
    if (found == pending_.end())
    {
        increment(out.unmatched_ends);
        throw std::logic_error("explicit music period End has no matching owned Begin");
    }
    busy_ = out.native_call_in_progress = true;
    const auto result = calls_->end(period);
    record_end(result);
    if (result == 0)
        pending_.erase(found);
    out.pending_calls = static_cast<std::uint64_t>(pending_.size());
    busy_ = out.native_call_in_progress = false;
    // A failed explicit End retains the obligation for final host cleanup.
    return result;
}

SourceMusicPeriodOwner::~SourceMusicPeriodOwner() noexcept
{
    if (std::this_thread::get_id() != thread_)
        std::terminate();
    if (busy_)
        std::terminate();
    auto &out = report_->values_;
    // NEW host fallback, independent of source45FCE0's allocated gate.
    // Exactly one final attempt per remaining obligation; no infinite retry.
    while (!pending_.empty())
    {
        const auto period = pending_.front().period;
        busy_ = out.native_call_in_progress = true;
        const auto result = calls_->end(period);
        record_end(result);
        increment(out.teardown_end_calls);
        if (result != 0)
            increment(out.unresolved_at_teardown);
        pending_.pop_front();
        out.pending_calls = static_cast<std::uint64_t>(pending_.size());
        busy_ = out.native_call_in_progress = false;
    }
    // Nonzero unresolved_at_teardown is retained by external report observers;
    // finalized does not mean all native requests were successfully released.
    out.finalized = true;
}
} // namespace sgn::retail
