#include "sgn/host_parallel.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

// NEW HOST (R30). See host_parallel.hpp.
namespace sgn::host_parallel
{
namespace
{
thread_local bool pool_thread = false;
thread_local bool background_thread = false;
thread_local bool running_bands = false; // this thread is inside its own band run (a nested run is inline)

struct Job
{
    const void *context{};
    BandBody body{};
    std::uint32_t count{};
    unsigned bands{};
    std::atomic<unsigned> next{0};
    std::mutex error_mutex;
    std::exception_ptr error;
    unsigned error_band{};
    std::uint32_t edge(unsigned band) const noexcept
    {
        return static_cast<std::uint32_t>(std::uint64_t(count) * band / bands);
    }
    // Claims and runs bands until none is left.
    void work() noexcept
    {
        for (;;)
        {
            const auto band = next.fetch_add(1, std::memory_order_relaxed);
            if (band >= bands)
                return;
            try
            {
                body(context, edge(band), edge(band + 1));
            }
            catch (...)
            {
                const std::lock_guard lock(error_mutex);
                if (!error || band < error_band)
                {
                    error = std::current_exception();
                    error_band = band;
                }
            }
        }
    }
};

// Workers wait for a new generation, take part in its job, and report when they leave it. The pool is
// intentionally never destroyed: idle workers blocked on the condition variable are ended by process exit,
// so no static destructor ever joins a thread.
class Pool
{
    std::mutex submit_; // one band run at a time; a concurrent caller runs inline
    std::mutex mutex_;
    std::condition_variable wake_, left_;
    Job *job_{};
    std::atomic<std::uint64_t> generation_{0}; // written under mutex_, also read by spinning workers
    unsigned active_{};
    unsigned workers_{};

    void worker() noexcept
    {
        pool_thread = true;
        std::uint64_t seen = 0;
        for (;;)
        {
            // A conversion makes many short band runs in a row; a worker that just left one stays awake
            // for about 0.2 ms, so the next run does not pay the condition-variable wake-up (tens of
            // microseconds, often longer than the run itself).
            const auto spin_end = std::chrono::steady_clock::now() + std::chrono::microseconds(200);
            while (generation_.load(std::memory_order_acquire) == seen && std::chrono::steady_clock::now() < spin_end)
                std::this_thread::yield();
            Job *job{};
            {
                std::unique_lock lock(mutex_);
                wake_.wait(lock, [&] { return generation_.load(std::memory_order_relaxed) != seen; });
                seen = generation_.load(std::memory_order_relaxed);
                job = job_;
                if (!job)
                    continue;
                ++active_;
            }
            job->work();
            {
                const std::lock_guard lock(mutex_);
                --active_;
            }
            left_.notify_all();
        }
    }

  public:
    Pool()
    {
        const unsigned hardware = std::max(1u, std::thread::hardware_concurrency());
        const unsigned wanted = std::min(7u, hardware - 1u);
        for (unsigned i = 0; i < wanted; ++i)
        {
            try
            {
                std::thread(&Pool::worker, this).detach();
                ++workers_;
            }
            catch (...)
            {
                break; // fewer workers; band runs stay correct
            }
        }
    }
    unsigned workers() const noexcept
    {
        return workers_;
    }
    // False when another thread is running bands (the caller then runs inline).
    bool run(Job &job)
    {
        std::unique_lock submit(submit_, std::try_to_lock);
        if (!submit.owns_lock())
            return false;
        {
            const std::lock_guard lock(mutex_);
            job_ = &job;
            generation_.fetch_add(1, std::memory_order_release);
        }
        wake_.notify_all();
        job.work();
        {
            std::unique_lock lock(mutex_);
            job_ = nullptr; // a late worker sees no job
            left_.wait(lock, [&] { return active_ == 0; });
        }
        return true;
    }
};
Pool &pool()
{
    static Pool *const instance = new Pool; // never destroyed (see Pool)
    return *instance;
}

class Background
{
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::function<void()>> tasks_;
    bool started_{};

    void worker() noexcept
    {
        background_thread = true;
#ifdef _WIN32
        // NEW HOST (R35): background processing mode (lowest CPU, I/O and memory priority), so the prewarm and
        // cache-write work never competes with the game thread's own loading and file reads.
        if (!SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN))
            (void)SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
        for (;;)
        {
            std::function<void()> task;
            {
                std::unique_lock lock(mutex_);
                wake_.wait(lock, [&] { return !tasks_.empty(); });
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }
            try
            {
                task();
            }
            catch (...)
            {
            }
        }
    }

  public:
    bool post(std::function<void()> &&task) noexcept
    {
        try
        {
            {
                const std::lock_guard lock(mutex_);
                if (!started_)
                {
                    // NEW HOST (R35): up to 2 workers (a sixth of the logical CPUs), so a menu's image
                    // prewarm finishes before its first page changes; still below normal priority and never
                    // on the band pool, so a foreground band run is not slowed.
                    const unsigned count = std::clamp(std::thread::hardware_concurrency() / 6u, 1u, 2u);
                    for (unsigned i = 0; i < count; ++i)
                        std::thread(&Background::worker, this).detach();
                    started_ = true;
                }
                tasks_.push_back(std::move(task));
            }
            wake_.notify_one();
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
};
Background &background()
{
    static Background *const instance = new Background; // never destroyed, like the pool
    return *instance;
}
std::atomic<ProfileSink> sink{nullptr};
} // namespace

void run_bands(std::uint32_t count, std::uint32_t grain, unsigned limit, const void *context, BandBody body)
{
    if (count == 0)
        return;
    const bool inline_only = pool_thread || background_thread || running_bands;
    unsigned bands = 1;
    if (!inline_only)
    {
        const auto by_grain = std::max<std::uint32_t>(1u, count / std::max<std::uint32_t>(1u, grain));
        bands = static_cast<unsigned>(std::min<std::uint64_t>({std::max(1u, limit), by_grain, concurrency()}));
    }
    if (bands > 1)
    {
        Job job;
        job.context = context;
        job.body = body;
        job.count = count;
        job.bands = bands;
        struct Running
        {
            Running() noexcept
            {
                running_bands = true;
            }
            ~Running()
            {
                running_bands = false;
            }
        };
        bool ran = false;
        {
            const Running running;
            ran = pool().run(job);
        }
        if (ran)
        {
            if (job.error)
                std::rethrow_exception(job.error);
            return;
        }
    }
    body(context, 0, count);
}
unsigned concurrency() noexcept
{
    try
    {
        return pool().workers() + 1u;
    }
    catch (...)
    {
        return 1u;
    }
}
bool post_background(std::function<void()> task) noexcept
{
    try
    {
        return background().post(std::move(task));
    }
    catch (...)
    {
        return false;
    }
}
bool on_background_thread() noexcept
{
    return background_thread;
}
bool on_pool_thread() noexcept
{
    return pool_thread;
}
void set_profile_sink(ProfileSink value) noexcept
{
    sink.store(value);
}
ProfileSink profile_sink() noexcept
{
    return pool_thread || background_thread ? nullptr : sink.load(std::memory_order_relaxed);
}
} // namespace sgn::host_parallel
