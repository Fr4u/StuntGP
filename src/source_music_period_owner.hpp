#pragma once
#include "sgn/source_music_period_host.hpp"
#include <list>

namespace sgn::retail {
// Private implementation: owned only by RaceSessionPlatform::Storage. No
// source state, HWND, legacy stream or borrowed backend pointer is stored.
class SourceMusicPeriodOwner final {
    struct Obligation { std::uint32_t period{}, begin_result{}; };
    const std::thread::id thread_{std::this_thread::get_id()};
    const std::shared_ptr<SourceMusicPeriodCalls> calls_;
    const std::shared_ptr<SourceMusicPeriodReport> report_;
    std::list<Obligation> pending_;
    bool busy_{};
    void check() const;
    void increment(std::uint64_t&) noexcept;
    void record_end(std::uint32_t) noexcept;
public:
    explicit SourceMusicPeriodOwner(std::shared_ptr<SourceMusicPeriodCalls>);
    ~SourceMusicPeriodOwner() noexcept;
    SourceMusicPeriodOwner(const SourceMusicPeriodOwner&) = delete;
    SourceMusicPeriodOwner& operator=(const SourceMusicPeriodOwner&) = delete;
    std::shared_ptr<const SourceMusicPeriodReport> report() const noexcept { return report_; }
    std::uint32_t begin(std::uint32_t);
    std::uint32_t end(std::uint32_t);
};

// Creates only an owned API adapter; construction makes no native API calls.
std::shared_ptr<SourceMusicPeriodCalls> make_installed_music_period_calls();
}
