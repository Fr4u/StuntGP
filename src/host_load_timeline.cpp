#include "sgn/host_load_timeline.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

// NEW HOST (R30). See host_load_timeline.hpp.
namespace sgn::host_load_timeline
{
namespace
{
struct Stage
{
    const char *name;
    std::uint64_t calls{};
    std::int64_t total{}, worst{}, last{};
    double first_start_ms{};
};
struct Milestone
{
    const char *name;
    double ms{};
};
struct State
{
    bool enabled{};
    std::string path;
    std::mutex mutex;
    std::vector<Stage> stages;
    std::vector<Milestone> milestones;
    std::chrono::steady_clock::time_point origin{std::chrono::steady_clock::now()};
    double origin_ms{}; // process age at `origin`
    State()
    {
        const char *value = nullptr;
#ifdef _WIN32
        char *owned = nullptr;
        std::size_t size = 0;
        if (_dupenv_s(&owned, &size, "SGN_PROFILE") == 0 && owned)
        {
            path = owned;
            std::free(owned);
        }
        FILETIME creation{}, exit{}, kernel{}, user{}, now{};
        if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user))
        {
            GetSystemTimePreciseAsFileTime(&now);
            const auto ticks = [](const FILETIME &t) {
                return (std::uint64_t(t.dwHighDateTime) << 32) | t.dwLowDateTime;
            };
            if (ticks(now) > ticks(creation))
                origin_ms = static_cast<double>(ticks(now) - ticks(creation)) / 10000.0; // 100 ns units
        }
#else
        value = std::getenv("SGN_PROFILE");
        if (value)
            path = value;
#endif
        (void)value;
        enabled = !path.empty();
        if (enabled)
            path += ".load.txt";
    }
};
State &state()
{
    static State s;
    return s;
}
double since_origin_ms(const State &s, std::chrono::steady_clock::time_point t) noexcept
{
    return s.origin_ms + std::chrono::duration<double, std::milli>(t - s.origin).count();
}
void write(State &s) noexcept
{
    try
    {
        auto stages = s.stages;
        std::sort(stages.begin(), stages.end(),
                  [](const Stage &a, const Stage &b) { return a.first_start_ms < b.first_start_ms; });
        std::FILE *file = nullptr;
#ifdef _WIN32
        if (fopen_s(&file, s.path.c_str(), "w") != 0 || !file)
            return;
#else
        file = std::fopen(s.path.c_str(), "w");
        if (!file)
            return;
#endif
        std::fprintf(file, "process_ms %.1f\n", since_origin_ms(s, std::chrono::steady_clock::now()));
        for (const auto &m : s.milestones)
            std::fprintf(file, "milestone %-40s %12.1f ms since process creation\n", m.name, m.ms);
        std::fprintf(file, "%-48s %8s %12s %10s %10s %10s %12s\n", "stage", "calls", "total_ms", "avg_ms", "worst_ms",
                     "last_ms", "first_at_ms");
        for (const auto &e : stages)
            std::fprintf(file, "%-48s %8llu %12.2f %10.3f %10.3f %10.3f %12.1f\n", e.name,
                         static_cast<unsigned long long>(e.calls), e.total / 1e6,
                         e.calls ? e.total / 1e6 / e.calls : 0.0, e.worst / 1e6, e.last / 1e6, e.first_start_ms);
        std::fclose(file);
    }
    catch (...)
    {
    }
}
} // namespace
bool enabled() noexcept
{
    return state().enabled;
}
void record(const char *stage, std::chrono::steady_clock::time_point start,
            std::chrono::steady_clock::time_point end) noexcept
{
    auto &s = state();
    if (!s.enabled)
        return;
    const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    try
    {
        const std::lock_guard lock(s.mutex);
        auto found = std::find_if(s.stages.begin(), s.stages.end(), [stage](const Stage &e) {
            return e.name == stage || std::strcmp(e.name, stage) == 0;
        });
        if (found == s.stages.end())
        {
            s.stages.push_back({stage});
            found = s.stages.end() - 1;
            found->first_start_ms = since_origin_ms(s, start);
        }
        ++found->calls;
        found->total += nanoseconds;
        found->worst = std::max(found->worst, nanoseconds);
        found->last = nanoseconds;
    }
    catch (...)
    {
    }
}
void milestone(const char *name) noexcept
{
    auto &s = state();
    if (!s.enabled)
        return;
    try
    {
        const std::lock_guard lock(s.mutex);
        if (std::any_of(s.milestones.begin(), s.milestones.end(),
                        [name](const Milestone &m) { return std::strcmp(m.name, name) == 0; }))
            return;
        s.milestones.push_back({name, since_origin_ms(s, std::chrono::steady_clock::now())});
        write(s);
    }
    catch (...)
    {
    }
}
void flush() noexcept
{
    auto &s = state();
    if (!s.enabled)
        return;
    try
    {
        const std::lock_guard lock(s.mutex);
        write(s);
    }
    catch (...)
    {
    }
}
double process_milliseconds() noexcept
{
    auto &s = state();
    return since_origin_ms(s, std::chrono::steady_clock::now());
}
} // namespace sgn::host_load_timeline
