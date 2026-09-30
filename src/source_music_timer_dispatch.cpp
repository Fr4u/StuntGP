#include "sgn/source_music_timer_dispatch.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/source_music_periodic.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <atomic>
#include <exception>
#include <limits>
#include <map>
#include <stdexcept>
#include <system_error>

namespace sgn::retail
{
struct SourceMusicTimerDispatch::Impl : std::enable_shared_from_this<Impl>
{
    static constexpr UINT tick_message = WM_APP + 0x153;
    struct Timer
    {
        HWND window{};
        std::uintptr_t token{};
        UINT native_id{};
        std::atomic<bool> post_failed{false};
        std::function<void(std::shared_ptr<RaceSessionPlatform>)> callback;
    };
    std::weak_ptr<RaceSessionPlatform> owner;
    DWORD thread = GetCurrentThreadId();
    HWND window{};
    std::uintptr_t next = 1;
    std::map<std::uintptr_t, std::unique_ptr<Timer>> timers;
    std::exception_ptr failure;
    bool closed = false;
    explicit Impl(const std::shared_ptr<RaceSessionPlatform> &p) : owner(p) {}
    void require_thread() const
    {
        if (GetCurrentThreadId() != thread)
            throw std::logic_error("music timer foreign thread");
    }
    static void CALLBACK timer_proc(UINT, UINT, DWORD_PTR user, DWORD_PTR, DWORD_PTR) noexcept
    {
        auto *timer = reinterpret_cast<Timer *>(user);
        // No game state, mutex, COM or owner destruction on the WinMM thread.
        if (!PostMessageW(timer->window, tick_message, timer->token, 0))
            timer->post_failed.store(true, std::memory_order_relaxed);
    }
    static LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) noexcept
    {
        auto *raw = reinterpret_cast<Impl *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE)
        {
            raw = static_cast<Impl *>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(raw));
        }
        if (raw && message == tick_message)
        {
            // Keep the dispatch state alive even if callback releases its facade.
            auto self = raw->shared_from_this();
            if (self->closed)
                return 0;
            const auto found = self->timers.find(static_cast<std::uintptr_t>(wp));
            if (found == self->timers.end())
                return 0; // cancelled/stale message
            const auto id = found->second->native_id;
            try
            {
                auto owner = self->owner.lock();
                if (!owner)
                {
                    (void)self->kill(id);
                    return 0;
                }
                owner->require_owner_thread();
                const auto callback = found->second->callback;
                callback(std::move(owner));
            }
            catch (...)
            {
                if (!self->failure)
                    self->failure = std::current_exception();
                (void)self->kill(id);
            }
            return 0;
        }
        return DefWindowProcW(hwnd, message, wp, lp);
    }
    void initialize()
    {
        WNDCLASSW cls{};
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.lpfnWndProc = procedure;
        cls.lpszClassName = L"StuntGPNewSourceMusicTimer";
        if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "music timer class");
        window = CreateWindowExW(0, cls.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, cls.hInstance, this);
        if (!window)
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "music timer window");
    }
    std::uint32_t kill(UINT id)
    {
        require_thread();
        for (auto it = timers.begin(); it != timers.end(); ++it)
            if (it->second->native_id == id)
            {
                const auto result = timeKillEvent(id);
                if (result == TIMERR_NOERROR)
                    timers.erase(it);
                // Failed cancellation retains context: WinMM could still use it.
                return result;
            }
        return TIMERR_NOCANDO;
    }
    void close() noexcept
    {
        if (GetCurrentThreadId() != thread)
            std::terminate();
        closed = true;
        for (auto &[token, timer] : timers)
        {
            (void)token;
            if (timeKillEvent(timer->native_id) != TIMERR_NOERROR)
                std::terminate(); // cannot free a possibly live native callback context
        }
        timers.clear();
        if (window)
        {
            DestroyWindow(window);
            window = nullptr;
        }
    }
};
SourceMusicTimerDispatch::SourceMusicTimerDispatch(std::shared_ptr<RaceSessionPlatform> owner)
{
    if (!owner)
        throw std::invalid_argument("music timer needs owner");
    owner->require_owner_thread();
    impl_ = std::make_shared<Impl>(owner);
    impl_->initialize();
}
SourceMusicTimerDispatch::~SourceMusicTimerDispatch()
{
    impl_->close();
}
std::uint32_t SourceMusicTimerDispatch::start(std::uint32_t delay, std::uint32_t resolution,
                                              std::function<void(std::shared_ptr<RaceSessionPlatform>)> callback,
                                              std::uint32_t flags)
{
    impl_->require_thread();
    if (impl_->closed || !callback || flags != 1)
        throw std::invalid_argument("music timer source arguments");
    const auto owner = impl_->owner.lock();
    if (!owner)
        throw std::logic_error("music timer owner expired");
    if (impl_->next == std::numeric_limits<std::uintptr_t>::max())
        throw std::overflow_error("music timer tokens exhausted");
    const auto token = impl_->next++;
    auto timer = std::make_unique<Impl::Timer>();
    timer->window = impl_->window;
    timer->token = token;
    timer->callback = std::move(callback);
    auto [slot, inserted] = impl_->timers.emplace(token, std::move(timer));
    if (!inserted)
        throw std::logic_error("duplicate timer token");
    // NEW lifetime flag. It preserves periodic mode but qualifies context
    // destruction after timeKillEvent, unlike the original source's flags=1.
    const auto id = timeSetEvent(delay, resolution, Impl::timer_proc, reinterpret_cast<DWORD_PTR>(slot->second.get()),
                                 TIME_PERIODIC | TIME_KILL_SYNCHRONOUS);
    if (!id)
    {
        impl_->timers.erase(slot);
        return 0;
    }
    slot->second->native_id = id;
    return id;
}
std::uint32_t SourceMusicTimerDispatch::kill(std::uint32_t id)
{
    return impl_->kill(id);
}
std::uint32_t SourceMusicTimerDispatch::start_source(std::uint32_t delay, std::uint32_t resolution,
                                                     std::weak_ptr<RaceSessionPlatform> expected,
                                                     std::weak_ptr<SourceMusicPeriodicIO> device,
                                                     std::weak_ptr<SourceMusicWaveIO> wave, std::uint32_t site,
                                                     std::uint32_t flags)
{
    impl_->require_thread();
    const auto actual = impl_->owner.lock();
    const auto supplied = expected.lock();
    if (!actual || actual != supplied || site != 0x00460510u || device.expired() || wave.expired())
        throw std::invalid_argument("music callback owner or source identity differs");
    return start(
        delay, resolution,
        [device, wave](std::shared_ptr<RaceSessionPlatform> owner) {
            const auto retained_device = device.lock();
            const auto retained_wave = wave.lock();
            if (!retained_device || !retained_wave)
                throw std::logic_error("music callback I/O lifetime expired");
            source_music_periodic_460510(std::move(owner), *retained_device, *retained_wave);
        },
        flags);
}
void SourceMusicTimerDispatch::check_health()
{
    impl_->require_thread();
    if (impl_->failure)
        std::rethrow_exception(impl_->failure);
    for (const auto &[token, timer] : impl_->timers)
    {
        (void)token;
        if (timer->post_failed.load(std::memory_order_relaxed))
            throw std::runtime_error("music timer owner message delivery failed");
    }
}
} // namespace sgn::retail
