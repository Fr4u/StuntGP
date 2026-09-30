#pragma once
#include <cstdint>
#include <functional>
#include <memory>

namespace sgn::retail
{
class RaceSessionPlatform;
struct SourceMusicPeriodicIO;
struct SourceMusicWaveIO;
// Native WinMM periodic timer, with owner-thread delivery. Source delay and
// resolution are retained. Host adds synchronous cancellation, not frame ticks.
class SourceMusicTimerDispatch final
{
    struct Impl;
    std::shared_ptr<Impl> impl_;

  public:
    explicit SourceMusicTimerDispatch(std::shared_ptr<RaceSessionPlatform>);
    ~SourceMusicTimerDispatch();
    SourceMusicTimerDispatch(const SourceMusicTimerDispatch &) = delete;
    SourceMusicTimerDispatch &operator=(const SourceMusicTimerDispatch &) = delete;
    std::uint32_t start(std::uint32_t delay, std::uint32_t resolution,
                        std::function<void(std::shared_ptr<RaceSessionPlatform>)>, std::uint32_t source_flags);
    std::uint32_t kill(std::uint32_t native_id);
    std::uint32_t start_source(std::uint32_t delay, std::uint32_t resolution, std::weak_ptr<RaceSessionPlatform>,
                               std::weak_ptr<SourceMusicPeriodicIO>, std::weak_ptr<SourceMusicWaveIO>,
                               std::uint32_t callback_site, std::uint32_t flags);
    void check_health();
};
} // namespace sgn::retail
