#include "sgn/source_host_pacing.hpp"
#include "sgn/host_frame_trace.hpp"
#include "sgn/host_graphics_settings.hpp"
#include "sgn/host_presentation.hpp"
#include "sgn/host_presentation_presenter.hpp"
#include "sgn/host_profiler.hpp"
#include "sgn/renderer.hpp"
#include "sgn/source_host_counter.hpp"
#include <windows.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <utility>
#include <stdexcept>
#include <system_error>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace sgn::retail
{

std::uint32_t source_host_counts_until_tick(const RetailTickClock &clock, std::uint32_t observed) noexcept
{
    // 4424AD..4424CF: EAX=observed-scheduled; CDQ/XOR/SUB magnitude (INT_MIN
    // stays 0x80000000); threshold=increment*30 with 32-bit wrap (LEA/LEA/SHL).
    const std::uint32_t delta = observed - clock.scheduled;
    const std::uint32_t sign = 0u - (delta >> 31);
    const std::uint32_t magnitude = (delta ^ sign) - sign;
    const std::uint32_t threshold = clock.increment * 30u;
    if (magnitude > threshold)
        return 0u; // 4424D4 JBE not taken: 4424D6/4424D7 store observed-1
    if (clock.scheduled < observed)
        return 0u; // 442834/441F1F CMP, JNC not taken
    // scheduled >= observed here and scheduled-observed <= threshold.
    return clock.scheduled - observed + 1u;
}

namespace
{
constexpr DWORD inactive_poll_ms = 100;   // NEW HOST bound for 44E9D0 while the pass is gated off
constexpr DWORD latency_timeout_ms = 250; // same bound as START's frame-event wait (main.cpp)

[[noreturn]] void throw_last_error(const char *what)
{
    throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), what);
}
enum class LatencyWait
{
    ready,
    message
};
// NEW HOST (R27) frame rate limiter constants (QPC based, see present_between_ticks).
constexpr std::uint32_t limit_min_fps = 30, limit_max_fps = 1000; // host_graphics contract domain
constexpr double limiter_on_time_seconds = 0.00025;               // a sample this close to its target keeps the phase
constexpr double present_cost_initial_seconds = 0.001, present_cost_min_seconds = 0.0003,
                 present_cost_max_seconds = 0.008, present_cost_margin_seconds = 0.0002;
// NEW HOST (R28b) uncapped mode (vsync off, frame_rate_limit 0): implicit cap in
// multiples of the display refresh (tearing/exclusive: 2x, a windowed swap chain
// without tearing: 1x, DWM shows at most one frame per refresh).
constexpr std::uint32_t implicit_cap_tearing_multiple = 2, implicit_cap_min_fps = 60;
constexpr double buffer_wait_max_seconds = 0.008;
// NEW HOST (R30): the monitor under the window is checked each second (cheap); its
// refresh (EnumDisplaySettingsW, a driver query) only when it changes or every 5 s.
constexpr double monitor_check_seconds = 1.0, refresh_query_seconds = 5.0;
// NEW HOST (R30): the final spin of a precise wait covers the timer wake-up
// lateness: 2x its average (fast rise, slow decay) + 50 us, within 150 us..1 ms.
constexpr double spin_min_seconds = 0.00015, spin_max_seconds = 0.001, spin_extra_seconds = 0.00005,
                 wake_error_initial_seconds = 0.0002;
// NEW HOST (R30) present-then-pass: bounded start delay of a due pass.
constexpr double presentation_margin_ticks = 0.5; // the presenter shows now - 1.5 ticks (host_presentation_presenter)
constexpr double pass_delay_max_seconds = 0.004, pass_delay_margin_seconds = 0.001;
constexpr double pass_cost_initial_seconds = 0.004;
constexpr double imminent_slot_seconds = 0.0003; // a due pass waits at most this long for a present slot
// NEW HOST (R33b) in-pass present guard: estimates start values (fast rise, slow decay),
// the fraction of a refresh that must separate two presents, and the source part of a
// pass (its duration without in-pass presents).
constexpr double yield_cost_initial_seconds = 0.0015, source_cost_initial_seconds = 0.004;
constexpr double yield_refresh_fraction = 0.75;
// NEW HOST (R30) implicit cap adaptation (tearing: 2x <-> 1x refresh).
constexpr double late_ratio_alpha = 1.0 / 16.0, late_ratio_reduce = 0.25;
constexpr double cap_hold_seconds = 2.0, cap_restore_cost_fraction = 0.35, cap_restore_gpu_fraction = 0.6;
// NEW HOST (R36) refresh-grid pose: the band (refreshes) around the sample time that the R35 rules keep
// the pose in; beyond it the pose is pulled back by a share of its own step (see plan_pose).
constexpr double pose_band_refreshes = 2.0;
std::int64_t qpc_now() noexcept
{
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value); // cannot fail on Windows XP and later
    return value.QuadPart;
}
std::int64_t qpc_frequency() noexcept
{
    LARGE_INTEGER value{};
    QueryPerformanceFrequency(&value);
    return value.QuadPart;
}
// NEW HOST (R28b): the monitor showing this thread's active window (primary monitor without one).
HMONITOR current_monitor() noexcept
{
    HWND window = GetActiveWindow();
    return window ? MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST)
                  : MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
}
// Current refresh rate (Hz) of `monitor`; 0 when Windows reports the default.
std::uint32_t display_refresh_hz(HMONITOR monitor) noexcept
{
    MONITORINFOEXW info{};
    info.cbSize = sizeof info;
    if (!monitor || !GetMonitorInfoW(monitor, &info))
        return 0;
    DEVMODEW mode{};
    mode.dmSize = sizeof mode;
    if (!EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode) || mode.dmDisplayFrequency <= 1)
        return 0;
    return mode.dmDisplayFrequency;
}
// Asymmetric average: rises fast (a spike is taken seriously at once), decays slowly.
void track_cost(double &average, double sample) noexcept
{
    average += (sample - average) * (sample > average ? 0.5 : 0.125);
}
} // namespace

struct SourceHostPacedMessages::Impl
{
    SourceApplicationMessages &native;
    SourceHostPacingWorld world;
    FrontendPerformanceCounter &counter;
    const std::unique_ptr<Renderer> &renderer;
    host_presentation::InterpolatingPresenter *presenter; // NEW HOST (R19), optional
    const DWORD owner_thread{GetCurrentThreadId()};
    HANDLE timer{};
    // Frame-latency credit: one successful wait not yet matched by a Present.
    bool credit{};
    HANDLE credit_object{};
    const Renderer *credit_renderer{};
    std::uint64_t credit_presents{};
    // Host cadence used only when the source clock does not gate the pass.
    bool host_deadline_valid{};
    std::uint64_t host_deadline{};
    bool idle_waited{};
    // NEW HOST (R27) frame rate limiter for the interpolated presents only.
    const std::int64_t qpc_hz{qpc_frequency()};
    std::uint64_t limit_revision{~std::uint64_t{0}};
    std::uint32_t limit_fps{}; // effective limit, 0 = off
    bool limiter_target_valid{};
    std::int64_t limiter_target{}; // QPC time of the next allowed present sample
    double present_cost{};         // EMA of pose sample -> Present return, QPC ticks
    // NEW HOST (R28b): uncapped-mode protection (see pacing_mode()).
    bool vsync{true};
    std::uint32_t refresh_hz{};
    HMONITOR refresh_monitor{};
    std::int64_t monitor_checked{}, refresh_queried{};
    bool refresh_valid{};
    double buffer_cost{}; // EMA of present() entry -> gate (back buffer wait), QPC ticks
    // NEW HOST (R30): exact tick times from the rebased counter's fractional value.
    const SourceHostRebasedCounter *precise{dynamic_cast<const SourceHostRebasedCounter *>(&counter)};
    double wake_error{}; // timer wake-up lateness, QPC ticks (fast rise, slow decay)
    // NEW HOST (R30): the released source pass (profiler span, pass cost).
    bool pass_open{}, pass_presenting{}, pass_profiled{};
    std::int64_t pass_start{};
    std::chrono::steady_clock::time_point pass_clock{};
    unsigned pass_depth{};
    double pass_cost{}; // QPC ticks, passes released while the presenter was active
    std::uint32_t last_scene{0xffffffffu};
    // NEW HOST (R30): vsync slot phase of the interpolated presents.
    bool slot_valid{};
    std::int64_t slot_pose{}; // QPC time whose state the last slot present showed
    // NEW HOST (R30): implicit cap adaptation (vsync off, no limit, tearing).
    bool cap_reduced{};
    std::int64_t cap_changed{};
    double late_ratio{};
    std::uint32_t reported_mode{0xffffffffu}, reported_fps{};
    // NEW HOST (R30): the due tick (clock.scheduled) whose pass already let one present go first.
    bool first_present_valid{};
    std::uint32_t first_present_scheduled{};
    // NEW HOST (R31) cooperative presentation inside the released source pass.
    bool in_yield{};
    // NEW HOST (R33b) in-pass present guard (see pass_yield).
    double yield_cost{};  // EMA of one in-pass present (whole yield call), QPC ticks
    double source_cost{}; // EMA of a presenting pass without its in-pass presents, QPC ticks
    std::int64_t pass_due{}, pass_yield_time{}, last_present_time{};
    bool pass_yield_stopped{};
    bool ticks_valid{};
    std::uint32_t ticks_scheduled{};
    std::exception_ptr yield_error; // a pacing failure inside a yield, rethrown at the next peek (pass boundary)

    Impl(SourceApplicationMessages &n, SourceHostPacingWorld w, FrontendPerformanceCounter &c,
         const std::unique_ptr<Renderer> &r, host_presentation::InterpolatingPresenter *p)
        : native(n), world(w), counter(c), renderer(r), presenter(p)
    {
        if (qpc_hz <= 0)
            throw std::logic_error("QueryPerformanceFrequency returned no frequency");
        present_cost = present_cost_initial_seconds * double(qpc_hz);
        wake_error = wake_error_initial_seconds * double(qpc_hz);
        pass_cost = pass_cost_initial_seconds * double(qpc_hz);
        yield_cost = yield_cost_initial_seconds * double(qpc_hz);
        source_cost = source_cost_initial_seconds * double(qpc_hz);
        timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        // Before Windows 10 1803 this flag is rejected; the waits then use
        // millisecond message waits (coarser wake, same tick results).
        sgn::host_profile::info("pacing.timer", timer ? "high-resolution waitable timer"
                                                      : "no high-resolution timer (millisecond waits)");
        sgn::host_profile::info("pacing.tick_times",
                                precise ? "exact (rebased counter fraction)" : "counter units (no rebased counter)");
        host_presentation::set_pass_yield(&pass_yield_entry, this); // NEW HOST (R31)
    }
    ~Impl()
    {
        host_presentation::set_pass_yield(nullptr, nullptr);
        if (timer)
            CloseHandle(timer);
    }
    Impl(const Impl &) = delete;
    Impl &operator=(const Impl &) = delete;

    void require_thread() const
    {
        if (GetCurrentThreadId() != owner_thread)
            throw std::logic_error("Paced application message queue used from another thread");
    }
    bool pass_enabled() const noexcept
    { // 44EB09..44EB19
        return world.window_word_61c374 != 0u && world.window_word_61c364 == 0u;
    }
    bool source_clock_gates() const noexcept
    { // 44EB1B..44EB34 and 4424A5/4424DE
        return (world.scene == 1u || world.scene == 2u) && world.fixed_mode_a != 1u && world.fixed_mode_b != 1u;
    }
    bool presenting() const noexcept
    {
        return presenter && presenter->active();
    }
    std::uint32_t source_frequency()
    {
        // Same low DWORD 434840 stores at 43485D; constant for the counter.
        const auto frequency = static_cast<std::uint32_t>(counter.frequency());
        if (frequency == 0u)
            throw std::logic_error("Source counter frequency is zero");
        return frequency;
    }
    void wait_messages(DWORD milliseconds)
    {
        if (MsgWaitForMultipleObjectsEx(0, nullptr, milliseconds, QS_ALLINPUT, MWMO_INPUTAVAILABLE) == WAIT_FAILED)
            throw_last_error("Paced message wait failed");
    }
    std::int64_t seconds_to_qpc(double seconds) const noexcept
    {
        return static_cast<std::int64_t>(seconds * double(qpc_hz));
    }
    // NEW HOST (R30): the next due tick. counts: the unchanged predicate on a
    // host read of the counter (0 = a tick is due now); due: its QPC time (in the
    // past when overdue). With the rebased counter the time is exact: the tick
    // is due when the floored counter reaches scheduled+1; floor(precise) is
    // counter() for the same QPC read (source_host_counter_win32.cpp).
    struct Tick
    {
        std::uint32_t counts{};
        std::int64_t due{};
    };
    Tick tick_state()
    {
        Tick t;
        const std::int64_t q = qpc_now();
        const double frequency = double(source_frequency());
        if (precise)
        {
            const double p = precise->precise_counter();
            const double whole = std::floor(p);
            const auto observed = static_cast<std::uint32_t>(static_cast<std::uint64_t>(whole));
            t.counts = source_host_counts_until_tick(world.clock, observed);
            double delta = double(static_cast<std::int32_t>(world.clock.scheduled + 1u - observed)) - (p - whole);
            if (!t.counts && delta > 0)
                delta = 0; // resync (4424D4): due now
            t.due = q + static_cast<std::int64_t>(std::ceil(delta * double(qpc_hz) / frequency));
            return t;
        }
        const auto observed = read_frontend_counter_434820(counter); // fresh host read, not stored
        t.counts = source_host_counts_until_tick(world.clock, observed);
        // R27: counts includes the +1 of the JNC test: counts-1 is the earliest distance.
        t.due = q + (t.counts ? static_cast<std::int64_t>(double(t.counts - 1u) * double(qpc_hz) / frequency) : 0);
        return t;
    }
    // QPC time at which the host cadence deadline (counter units) is reached.
    std::int64_t host_deadline_time(std::uint64_t now_counts)
    {
        const std::int64_t q = qpc_now();
        const double frequency = double(source_frequency());
        if (precise)
        {
            const double remaining = double(host_deadline) - precise->precise_counter();
            return q + static_cast<std::int64_t>(std::ceil((std::max)(remaining, 0.0) * double(qpc_hz) / frequency));
        }
        // counter() is floored: waiting the whole distance from the read is never early.
        return q +
               static_cast<std::int64_t>(std::ceil(double(host_deadline - now_counts) * double(qpc_hz) / frequency));
    }
    std::int64_t spin_window() const noexcept
    {
        const double window = std::clamp(2.0 * wake_error + spin_extra_seconds * double(qpc_hz),
                                         spin_min_seconds * double(qpc_hz), spin_max_seconds * double(qpc_hz));
        return static_cast<std::int64_t>(window);
    }
    // NEW HOST (R30): wait until `target` (QPC) or input. The high-resolution
    // timer is armed never early; with `spin` it wakes one spin window before the
    // target and the rest is spun. Returns false when input arrived (the caller
    // dispatches it and re-evaluates; targets are absolute).
    bool precise_wait(std::int64_t target, bool spin)
    {
        const std::int64_t window = spin ? spin_window() : 0;
        for (;;)
        {
            const std::int64_t now = qpc_now();
            const std::int64_t remaining = target - now;
            if (remaining <= 0)
                return true;
            const std::int64_t wake_at = target - window;
            const double exact = double(wake_at - now) * 10000000.0 / double(qpc_hz); // 100 ns units
            const auto units = static_cast<LONGLONG>(spin ? std::floor(exact) : std::ceil(exact));
            if (remaining <= window || units <= 0)
            {
                while (qpc_now() < target)
                    YieldProcessor();
                return true;
            }
            DWORD result;
            if (timer)
            {
                LARGE_INTEGER due{};
                due.QuadPart = -units;
                if (!SetWaitableTimerEx(timer, &due, 0, nullptr, nullptr, nullptr, 0))
                    throw_last_error("Paced timer arm failed");
                const DWORD bound = static_cast<DWORD>(units / 10000) + 2u;
                result = MsgWaitForMultipleObjectsEx(1, &timer, bound, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                if (result == WAIT_FAILED)
                    throw_last_error("Paced timer wait failed");
                if (result == WAIT_OBJECT_0 + 1)
                    return false;
                if (result == WAIT_OBJECT_0)
                {
                    const std::int64_t late = (std::max)(std::int64_t{0}, qpc_now() - wake_at);
                    track_cost(wake_error, double(late));
                    sgn::host_profile::sample("sched.timer_wake_late_us", late * 1000000 / qpc_hz);
                }
                continue;
            }
            // No high-resolution timer (before Windows 10 1803): whole milliseconds, then spin.
            const auto milliseconds = static_cast<DWORD>(units / 10000);
            if (milliseconds == 0)
            {
                while (qpc_now() < target)
                    YieldProcessor();
                return true;
            }
            result = MsgWaitForMultipleObjectsEx(0, nullptr, milliseconds, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            if (result == WAIT_FAILED)
                throw_last_error("Paced wait failed");
            if (result == WAIT_OBJECT_0)
                return false;
        }
    }
    // Sleeps until the tick is due or a message arrives; the caller re-evaluates.
    // NEW HOST (R30): exact due time; the final spin only where the tick decides a
    // displayed frame's time (presenter inactive, vsync off).
    void wait_for_tick(const Tick &tick)
    {
        SGN_PROFILE_SCOPE("host.pacing_wait_for_tick");
        const bool spin = pass_enabled() && !presenting() && !vsync_on();
        (void)precise_wait(tick.due, spin);
    }
    bool vsync_on()
    {
        refresh_limit();
        return vsync;
    }
    void settle_credit()
    {
        if (!credit)
            return;
        const Renderer *current = renderer.get();
        if (!current || current != credit_renderer || current->frame_event() != credit_object ||
            current->stats().presents != credit_presents)
            credit = false; // consumed by a Present (432A60) or the swap chain changed
    }
    LatencyWait acquire_latency()
    {
        SGN_PROFILE_SCOPE("host.frame_latency_wait");
        Renderer *current = renderer.get();
        if (!current)
            return LatencyWait::ready; // before 44E890 creates the swap chain
        HANDLE object = current->frame_event();
        if (!object || credit)
            return LatencyWait::ready;
        if (current->occluded())
            return LatencyWait::ready; // occluded Present retires nothing
        const auto result =
            MsgWaitForMultipleObjectsEx(1, &object, latency_timeout_ms, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (result == WAIT_OBJECT_0)
        {
            take_credit(object, *current);
            return LatencyWait::ready;
        }
        if (result == WAIT_OBJECT_0 + 1)
            return LatencyWait::message;
        if (result == WAIT_TIMEOUT)
            return LatencyWait::ready; // no retirement: run the pass as 44EA60 would
        throw_last_error("Frame latency wait failed");
    }
    void take_credit(HANDLE object, const Renderer &current)
    {
        credit = true;
        credit_object = object;
        credit_renderer = &current;
        credit_presents = current.stats().presents;
    }
    // NEW HOST (R27): host_graphics::Settings::frame_rate_limit, applied live
    // (revision()). It caps only the presenter's interpolated presents. With
    // interpolation off the source 60 Hz pass is the only frame source and is
    // never paced by it, so a limit at or above 60 is a no-op there and a limit
    // below 60 is ignored (simulation and source presents stay at the tick).
    void refresh_limit()
    {
        const auto revision = host_graphics::revision();
        if (revision == limit_revision)
            return;
        limit_revision = revision;
        const auto settings = host_graphics::current();
        vsync = settings.vsync;
        auto fps = settings.frame_rate_limit;
        if (fps != 0)
            fps = std::clamp(fps, limit_min_fps, limit_max_fps);
        if (fps != limit_fps)
        {
            limit_fps = fps;
            limiter_target_valid = false; // new period: restart the phase at the next present
        }
    }
    // NEW HOST (R30): the monitor is compared each second; its refresh is re-read
    // when it changes and every 5 s (EnumDisplaySettingsW is a driver query).
    void query_refresh()
    {
        const auto now = qpc_now();
        if (refresh_valid && now - monitor_checked < seconds_to_qpc(monitor_check_seconds))
            return;
        monitor_checked = now;
        const HMONITOR monitor = current_monitor();
        if (refresh_valid && monitor == refresh_monitor &&
            now - refresh_queried < seconds_to_qpc(refresh_query_seconds))
            return;
        SGN_PROFILE_SCOPE("host.pacing_refresh_query");
        refresh_monitor = monitor;
        refresh_hz = display_refresh_hz(monitor);
        refresh_queried = now;
        refresh_valid = true;
    }
    // NEW HOST (R28b/R30) pacing of the interpolated presents:
    //  - vsync on and no limit, or a limit >= the refresh (R30: it cannot bind):
    //    display slots (frame-latency object), no guard, pose on the slot phase;
    //  - frame_rate_limit set otherwise: that limit, tick guard on (R27);
    //  - vsync off, no limit: implicit cap (refresh x2 with tearing or exclusive
    //    fullscreen, R30: x1 while x2 misses its slots; x1 without tearing), tick
    //    guard on; if the refresh is unknown, the tick guard alone.
    struct PacingMode
    {
        std::uint32_t fps{};
        bool guard{};
        bool implicit{};
        bool slots{};
    };
    PacingMode pacing_mode(const Renderer &current)
    {
        query_refresh();
        if (limit_fps)
        {
            if (vsync && refresh_hz && limit_fps >= refresh_hz)
                return {0, false, false, true};
            return {limit_fps, true, false, false};
        }
        if (vsync)
            return {0, false, false, true};
        const auto &stats = current.stats();
        std::uint32_t multiple =
            stats.tearing_supported || stats.exclusive_fullscreen ? implicit_cap_tearing_multiple : 1u;
        if (multiple > 1u)
            multiple = adapt_implicit_cap(stats) ? multiple : 1u;
        std::uint32_t fps = 0;
        if (refresh_hz)
            fps = std::clamp(refresh_hz * multiple, implicit_cap_min_fps, limit_max_fps);
        return {fps, true, true, false};
    }
    // NEW HOST (R30): true while the 2x implicit cap is kept. Reduced to 1x when
    // more than a quarter of the paced presents miss their slot (the CPU or GPU
    // cannot sustain it: an irregular cadence); restored after cap_hold_seconds
    // when the present cost and the GPU time fit the 2x period with headroom.
    bool adapt_implicit_cap(const RenderStats &stats)
    {
        const std::int64_t now = qpc_now();
        if (!cap_reduced)
        {
            if (late_ratio > late_ratio_reduce && now - cap_changed >= seconds_to_qpc(cap_hold_seconds))
            {
                cap_reduced = true;
                cap_changed = now;
                char text[128];
                std::snprintf(text, sizeof text, "2x -> 1x refresh (%.0f%% of presents late)", late_ratio * 100.0);
                sgn::host_profile::transition("pacing.implicit_cap", text);
            }
            return !cap_reduced;
        }
        if (!refresh_hz || now - cap_changed < seconds_to_qpc(cap_hold_seconds))
            return false;
        const double period = double(qpc_hz) / (2.0 * double(refresh_hz));
        const double cost = double(present_estimate() + buffer_estimate());
        const double gpu = stats.gpu_ms_last * 1e-3 * double(qpc_hz);
        if (cost < cap_restore_cost_fraction * period && gpu < cap_restore_gpu_fraction * period)
        {
            cap_reduced = false;
            cap_changed = now;
            late_ratio = 0;
            sgn::host_profile::transition("pacing.implicit_cap", "1x -> 2x refresh (cost fits)");
            return true;
        }
        cap_changed = now; // next check after another hold period
        return false;
    }
    void report_mode(const PacingMode &mode)
    {
        const std::uint32_t key = (mode.slots ? 1u : 0u) | (mode.guard ? 2u : 0u) | (mode.implicit ? 4u : 0u);
        if (key == reported_mode && mode.fps == reported_fps)
            return;
        reported_mode = key;
        reported_fps = mode.fps;
        if (!sgn::host_profile::enabled())
            return;
        char text[160];
        if (mode.slots)
            std::snprintf(text, sizeof text, "vsync display slots (limit %u, refresh %u Hz), pose on the slot",
                          limit_fps, refresh_hz);
        else if (mode.implicit)
            std::snprintf(text, sizeof text, "uncapped: implicit %u fps (refresh %u Hz) + tick guard", mode.fps,
                          refresh_hz);
        else
            std::snprintf(text, sizeof text, "limit %u fps + tick guard (vsync %s, refresh %u Hz)", mode.fps,
                          vsync ? "on" : "off", refresh_hz);
        sgn::host_profile::transition("pacing.mode", text);
    }
    std::uint32_t paced_fps{}; // period of the current PacingMode (0: tick guard only)
    std::int64_t limiter_period() const noexcept
    {
        return paced_fps ? (qpc_hz + paced_fps / 2) / paced_fps : 0;
    }
    // Expected time from present() entry to the gate: the wait for this Present's back buffer.
    std::int64_t buffer_estimate() const noexcept
    {
        return static_cast<std::int64_t>(std::clamp(buffer_cost * 1.25, 0.0, buffer_wait_max_seconds * double(qpc_hz)));
    }
    // Expected time from the pose sample to the return of the Present call.
    std::int64_t present_estimate() const noexcept
    {
        const double estimate =
            std::clamp(present_cost * 1.25 + present_cost_margin_seconds * double(qpc_hz),
                       present_cost_min_seconds * double(qpc_hz), present_cost_max_seconds * double(qpc_hz));
        return static_cast<std::int64_t>(estimate);
    }
    // NEW HOST (R30) present-then-pass: how long a due pass may wait for one
    // present (guarded modes, presenter active). The presenter first needs the
    // frame of a tick half a tick after its due time; the pass (estimated from the
    // measured passes, fast rise) must end before that with a margin.
    std::int64_t pass_delay_budget()
    {
        const double tick = double(world.clock.increment) * double(qpc_hz) / double(source_frequency());
        const double slack =
            presentation_margin_ticks * tick - pass_cost * 1.25 - pass_delay_margin_seconds * double(qpc_hz);
        return static_cast<std::int64_t>(std::clamp(slack, 0.0, pass_delay_max_seconds * double(qpc_hz)));
    }
    // Gate run by the presenter between its buffer wait and its time sample.
    struct LimiterGate final : host_presentation::PresentGate
    {
        Impl &pacing;
        std::int64_t pass_limit{}; // QPC time by which the pass of the next tick must be able to start
        std::int64_t entry{};      // QPC time present() was called (R28b: back buffer wait cost)
        std::int64_t target{}, sample{};
        bool skipped_for_tick{}, input{};
        std::exception_ptr error; // a pacing failure is carried out of present(), never absorbed by it
        LimiterGate(Impl &p, std::int64_t limit) : pacing(p), pass_limit(limit), entry(qpc_now()) {}
        bool open() override
        {
            try
            {
                return pacing.limiter_open(*this);
            }
            catch (...)
            {
                error = std::current_exception();
                return false;
            }
        }
    };
    bool limiter_open(LimiterGate &gate)
    {
        const std::int64_t now = qpc_now();
        track_cost(buffer_cost, double(now - gate.entry));
        if (!limiter_target_valid)
        {
            limiter_target = now;
            limiter_target_valid = true;
        }
        gate.target = limiter_target;
        // Never delay a tick pass beyond its budget (R30; R27: not at all): a
        // present that cannot finish in time is shifted behind the pass (its
        // target stays; it is then late).
        if ((std::max)(limiter_target, now) + present_estimate() > gate.pass_limit)
        {
            gate.skipped_for_tick = true;
            sgn::host_profile::add("interp.frame_limiter_shifted_behind_tick", 0);
            return false;
        }
        {
            SGN_PROFILE_SCOPE("host.frame_limiter_wait");
            if (!precise_wait(limiter_target, true))
            {
                gate.input = true;
                return false;
            }
        }
        gate.sample = qpc_now();
        return true;
    }
    void limiter_presented(const LimiterGate &gate, bool implicit)
    {
        const std::int64_t end = qpc_now();
        track_cost(present_cost, double(end - gate.sample)); // R28b: fast rise, slow decay
        // On time: keep the phase (exact average rate). Late (behind a tick pass,
        // a slow build or input): restart one full period after this sample, so
        // two presents are never closer than the period (VRR upper bound).
        const std::int64_t period = limiter_period();
        const bool on_time = gate.sample - gate.target <= seconds_to_qpc(limiter_on_time_seconds);
        limiter_target = on_time ? gate.target + period : gate.sample + period;
        if (implicit)
            late_ratio += ((on_time ? 0.0 : 1.0) - late_ratio) * late_ratio_alpha; // R30 implicit cap adaptation
    }
    // NEW HOST (R35) pose time on the real display refresh grid (vsync display slots).
    // The DXGI frame statistics (read before every slot present: GetLastPresentCount,
    // GetFrameStatistics) give the refresh grid (SyncQPCTime/SyncRefreshCount, period
    // measured over >= 1 s of refreshes) and, for the latest displayed present, the
    // refresh it appeared on (PresentCount/PresentRefreshCount). Each slot present is
    // assigned the refresh it will be shown on: the one after its predecessor's, or,
    // when it is late for that one, the first grid refresh after the sample (a present
    // cannot appear earlier; a GPU delay beyond it shows in the statistics and is
    // corrected as below). Its pose time is its predecessor's pose time
    // plus exactly that many refresh periods, so consecutive displayed frames advance
    // the state by exactly their display step. When the statistics show that a present
    // appeared on another refresh than predicted, the presents already queued behind it
    // moved with it: the refresh numbering is shifted, the poses are not (their steps
    // stay one refresh, like the display's). The pose stays near the sample time: when
    // the average offset (pose to sample, 1/32 average; presents come in bursts, so the
    // instantaneous offset swings by a refresh or two) exceeds one refresh, it is slewed
    // out by at most 2% of a refresh per present (a one-refresh drift after a repeat
    // disappears in ~50 presents, invisibly). A gap without presents (a long pass) keeps
    // the chain: the next present takes the first refresh it can reach and advances the
    // pose by exactly the refreshes in between. More than 0.5 s without a slot present,
    // another present in between (limiter, canonical Flip, pause, scene change) or an
    // offset beyond 6 refreshes restarts the chain at the sample time. Without statistics: the R30 slot heuristic.
    struct PoseGrid
    {
        bool grid_valid{};
        double period{}; // QPC ticks per refresh
        std::int64_t sync_qpc{}, sync_refresh{};
        std::uint32_t raw_sync_refresh{};
        std::int64_t anchor_qpc{}, anchor_refresh{};
        bool chain_valid{};
        std::uint32_t chain_id{};
        std::int64_t chain_refresh{}, chain_pose{}, chain_time{};
        double offset_average{}; // QPC ticks, sample - pose
        struct Prediction
        {
            bool valid{};
            std::uint32_t id{};
            std::int64_t refresh{}, sample{};
        };
        std::array<Prediction, 32> predicted{};
        std::size_t next_prediction{};
        // NEW HOST (R36): sample-to-sample intervals (QPC ticks) of the latest presents of this chain.
        std::array<std::int64_t, 8> cadence{};
        std::size_t cadence_count{}, cadence_next{};
    } grid;
    struct PosePlan
    {
        bool grid{}, resync{};
        bool restart{};           // NEW HOST (R36): a new chain (the cadence history starts again)
        std::int64_t interval{};  // NEW HOST (R36): sample - previous chain sample (continued chain only)
        std::int64_t paced_by{};  // NEW HOST (R36): pose advance added by the present cadence (QPC ticks)
        std::int64_t caught_up{}; // NEW HOST (R36): band correction of this pose (QPC ticks, + = forward)
        std::uint32_t id{};
        std::int64_t refresh{}, pose{}, now{}, steps{};
        double offset_average{};
    };
    // NEW HOST (R36) present cadence in refreshes: the median sample-to-sample interval of the latest
    // (up to 8) presents of the chain, when at least 4 are known and it is clearly above one refresh
    // (> 1.25); 0 otherwise. A vsync-limited chain (one present per refresh, bursts after a pass) has
    // a median of one refresh or less and is never affected; a GPU-limited chain (e.g. ~120 fps on
    // a 240 Hz display for the first ~0.2 s of a race after the load) has a median of ~2 refreshes.
    double cadence_refreshes() const
    {
        const auto &g = grid;
        if (g.cadence_count < 4 || g.period <= 0.0)
            return 0.0;
        std::array<std::int64_t, 8> sorted{};
        std::copy_n(g.cadence.begin(), g.cadence_count, sorted.begin());
        const auto end = sorted.begin() + static_cast<std::ptrdiff_t>(g.cadence_count);
        std::sort(sorted.begin(), end);
        const std::size_t half = g.cadence_count / 2;
        const double median =
            g.cadence_count % 2 ? double(sorted[half]) : 0.5 * double(sorted[half - 1] + sorted[half]);
        const double refreshes = median / g.period;
        return refreshes > 1.25 ? refreshes : 0.0;
    }
    std::int64_t grid_time(std::int64_t refresh) const noexcept
    {
        return grid.sync_qpc + std::llround(double(refresh - grid.sync_refresh) * grid.period);
    }
    void update_grid(const Renderer::DisplayStatistics &stats)
    {
        auto &g = grid;
        if (!g.grid_valid || stats.sync_qpc < g.sync_qpc)
        {
            g.grid_valid = true;
            g.sync_refresh = stats.sync_refresh;
            g.raw_sync_refresh = stats.sync_refresh;
            g.sync_qpc = stats.sync_qpc;
            g.anchor_qpc = g.sync_qpc;
            g.anchor_refresh = g.sync_refresh;
            g.chain_valid = false;
        }
        else
        {
            g.sync_refresh += static_cast<std::int32_t>(stats.sync_refresh - g.raw_sync_refresh);
            g.raw_sync_refresh = stats.sync_refresh;
            g.sync_qpc = stats.sync_qpc;
            const std::int64_t refreshes = g.sync_refresh - g.anchor_refresh;
            if (refreshes < 0)
            {
                g.anchor_qpc = g.sync_qpc;
                g.anchor_refresh = g.sync_refresh;
            }
            else if (refreshes >= 240)
            {
                const double p = double(g.sync_qpc - g.anchor_qpc) / double(refreshes);
                if (p > double(qpc_hz) / 1000.0 && p < double(qpc_hz) / 10.0)
                    g.period = p;
                g.anchor_qpc = g.sync_qpc;
                g.anchor_refresh = g.sync_refresh;
            }
        }
        if (g.period <= 0.0 && refresh_hz)
            g.period = double(qpc_hz) / double(refresh_hz); // until one second of refreshes is measured
    }
    // The refresh the latest displayed present appeared on, against its prediction.
    void grid_feedback(const Renderer::DisplayStatistics &stats)
    {
        auto &g = grid;
        const std::int64_t actual =
            g.sync_refresh + static_cast<std::int32_t>(stats.present_refresh - g.raw_sync_refresh);
        for (auto &p : g.predicted)
        {
            if (!p.valid)
                continue;
            const auto age = static_cast<std::int32_t>(stats.present_count - p.id);
            if (age > 0)
            {
                p.valid = false; // displayed between two statistics reads: its refresh is not reported
                continue;
            }
            if (age < 0)
                continue;
            p.valid = false;
            if (sgn::host_profile::enabled())
                sgn::host_profile::sample("pacing.pose_sample_to_screen_us",
                                          (grid_time(actual) - p.sample) * 1000000 / qpc_hz);
            const std::int64_t error = actual - p.refresh;
            if (!error)
                continue;
            // Shown on another refresh than predicted: everything queued behind it moved too.
            for (auto &q : g.predicted)
                if (q.valid && static_cast<std::int32_t>(q.id - stats.present_count) > 0)
                    q.refresh += error;
            if (g.chain_valid && static_cast<std::int32_t>(g.chain_id - stats.present_count) >= 0)
                g.chain_refresh += error;
            sgn::host_profile::add(
                error > 0 ? "pacing.pose_shown_later_than_predicted" : "pacing.pose_shown_earlier_than_predicted", 0);
        }
    }
    PosePlan plan_pose(std::int64_t now)
    {
        PosePlan r;
        r.now = now;
        Renderer *current = renderer.get();
        if (!current)
            return r;
        const auto stats = current->display_statistics();
        if (!stats.present_ok || !stats.ok)
            return r;
        update_grid(stats);
        grid_feedback(stats);
        auto &g = grid;
        if (g.period <= 0.0)
            return r;
        r.grid = true;
        r.id = stats.last_present + 1;
        // Earliest refresh this present can appear on: the first one after the sample.
        const std::int64_t earliest =
            g.sync_refresh + static_cast<std::int64_t>(std::floor(double(now - g.sync_qpc) / g.period)) + 1;
        // A gap (a long pass, a hitch) keeps the chain: the present then takes the first refresh it
        // can reach and its pose advances by exactly the refreshes the display advanced.
        const bool restart = !g.chain_valid || r.id != g.chain_id + 1 || now - g.chain_time > seconds_to_qpc(0.5);
        r.restart = restart; // NEW HOST (R36)
        if (!restart)
        {
            r.refresh = (std::max)(g.chain_refresh + 1, earliest);
            r.steps = r.refresh - g.chain_refresh;
            r.pose = g.chain_pose + std::llround(double(r.steps) * g.period);
            r.interval = now - g.chain_time; // NEW HOST (R36)
            // NEW HOST (R36): when the presents come further apart than one refresh (GPU-limited, e.g.
            // for ~0.2 s after a load), the display shows each frame for about that many refreshes although
            // the numbering above (shifted by the statistics feedback onto the queued, later refreshes)
            // advances by one: the pose fell behind the sample by about a refresh per present until the
            // 6-refresh resync jumped it forward (half-speed motion, then a ~31 ms jump, repeated). The pose
            // now advances by the present cadence, but not closer than two refreshes to the sample, so each
            // displayed frame advances the state by about its display step and the presenter keeps its
            // margin to the newest source frame. Vsync-limited chains (median interval <= 1.25 refreshes)
            // keep the R35 steps exactly.
            const double cadence = cadence_refreshes();
            const std::int64_t paced_limit = now - std::llround(pose_band_refreshes * g.period);
            if (cadence > double(r.steps) && r.pose < paced_limit)
            {
                const std::int64_t paced = (std::min)(g.chain_pose + std::llround(cadence * g.period), paced_limit);
                if (paced > r.pose)
                {
                    r.paced_by = paced - r.pose;
                    r.pose = paced;
                    r.refresh =
                        g.chain_refresh + (std::max)(r.steps, std::llround(double(paced - g.chain_pose) / g.period));
                    r.steps = r.refresh - g.chain_refresh;
                }
            }
            const double offset = double(now - r.pose);
            if (std::abs(offset) <= 6.0 * g.period)
            {
                r.offset_average = g.offset_average + (offset - g.offset_average) / 32.0;
                const double band = pose_band_refreshes * g.period;
                if (std::abs(offset) > band)
                {
                    // NEW HOST (R36): more than two refreshes off the sample (after a stall, a queue that filled
                    // or drained, a long pass): pulled back toward the band by a share of this present's own
                    // step, 10% up to 3.5 refreshes off, 25% beyond (a lag of ~20 ms is gone in ~0.1-0.2 s
                    // instead of ending in the 6-refresh resync jump, or, ahead, in frozen alpha-1 frames).
                    const double step = (std::max)(double(r.pose - g.chain_pose), g.period);
                    const double share = std::abs(offset) <= 3.5 * g.period ? 0.10 : 0.25;
                    const double correction =
                        std::clamp(offset - std::copysign(band, offset), -share * step, share * step);
                    r.caught_up = std::llround(correction);
                    r.pose += r.caught_up;
                }
                else if (std::abs(r.offset_average) > g.period)
                    r.pose += std::llround(std::clamp(r.offset_average, -0.02 * g.period, 0.02 * g.period));
                return r;
            }
        }
        r.resync = true;
        r.refresh = restart ? earliest : r.refresh;
        r.steps = 0;
        r.pose = now;
        return r;
    }
    void commit_pose(const PosePlan &r)
    {
        auto &g = grid;
        if (!r.grid)
        {
            g.chain_valid = false;
            return;
        }
        g.chain_valid = true;
        g.chain_id = r.id;
        g.chain_refresh = r.refresh;
        g.chain_pose = r.pose;
        g.chain_time = r.now;
        g.offset_average = r.resync ? 0.0 : r.offset_average;
        g.predicted[g.next_prediction] = {true, r.id, r.refresh, r.now};
        g.next_prediction = (g.next_prediction + 1) % g.predicted.size();
        // NEW HOST (R36): the present cadence of this chain (a new chain starts it again).
        if (r.restart)
            g.cadence_count = g.cadence_next = 0;
        else if (r.interval > 0)
        {
            g.cadence[g.cadence_next] = r.interval;
            g.cadence_next = (g.cadence_next + 1) % g.cadence.size();
            g.cadence_count = (std::min)(g.cadence_count + 1, g.cadence.size());
        }
        if (sgn::host_profile::enabled())
        {
            if (r.paced_by > 0) // NEW HOST (R36)
            {
                sgn::host_profile::add("pacing.pose_paced_by_cadence", 0);
                sgn::host_profile::sample("pacing.pose_cadence_advance_us", r.paced_by * 1000000 / qpc_hz);
            }
            if (r.caught_up != 0) // NEW HOST (R36)
            {
                sgn::host_profile::add(r.caught_up > 0 ? "pacing.pose_band_catch_up" : "pacing.pose_band_hold_back", 0);
                sgn::host_profile::sample("pacing.pose_band_correction_us", r.caught_up * 1000000 / qpc_hz);
            }
            if (r.resync)
                sgn::host_profile::add("pacing.pose_resync", 0);
            else if (r.steps > 1)
                sgn::host_profile::add("pacing.pose_refresh_skip", 0);
            sgn::host_profile::sample("pacing.pose_behind_sample_us", (r.now - r.pose) * 1000000 / qpc_hz);
        }
    }
    // NEW HOST (R30): gate of a vsync-paced present. The state shown is the one of
    // its display slot: the time the frame-latency object was signalled when this
    // present waited for it, else the next slot of the refresh grid after the
    // previous present's (at least one refresh later; the latest grid point when
    // presents fell behind). Nothing waits here.
    struct SlotGate final : host_presentation::PresentGate
    {
        Impl &pacing;
        bool waited{};
        std::int64_t wake{}, pose{}, sample{};
        double delay{};
        std::int64_t entry{qpc_now()}; // NEW HOST (R33b): present() entry, for the back buffer wait estimate
        PosePlan plan;                 // NEW HOST (R35): committed by the caller when the present succeeded
        SlotGate(Impl &p, bool w, std::int64_t at) : pacing(p), waited(w), wake(at) {}
        bool open() override
        {
            const std::int64_t now = qpc_now();
            track_cost(pacing.buffer_cost, double(now - entry));
            sample = now;
            // NEW HOST (R35): the refresh-grid pose (DXGI statistics); the R30 heuristic without them.
            plan = pacing.plan_pose(now);
            pose = plan.grid ? plan.pose : pacing.slot_pose_for(waited, wake, now);
            delay = double(now - pose) / double(pacing.qpc_hz);
            return true;
        }
        double pose_delay_seconds() const noexcept override
        {
            return delay;
        }
    };
    std::int64_t slot_pose_for(bool waited, std::int64_t wake, std::int64_t now) const noexcept
    {
        if (waited)
            return wake;
        if (!slot_valid || !refresh_hz)
            return now;
        const std::int64_t refresh = (qpc_hz + refresh_hz / 2) / refresh_hz;
        const std::int64_t since = now - slot_pose;
        if (since < 0 || since > 8 * refresh)
            return now; // no recent slot (first present, a stall): the sample itself
        const std::int64_t slots = (std::max)(std::int64_t{1}, since / refresh);
        return slot_pose + slots * refresh;
    }
    // NEW HOST (R19 interp): between two due ticks, present interpolated frames on
    // each display slot. Returns false when nothing can be presented now; the
    // caller then waits for the tick (or releases the due pass) exactly as before.
    // A slot is the frame-latency credit (acquired here; the pass takes none
    // while the presenter is active, R30).
    // NEW HOST (R30): also called with a due tick (tick.counts == 0) in guarded
    // modes: one present goes first when its slot is due now and it ends within
    // the pass delay budget (present-then-pass); otherwise false (the pass runs).
    bool present_between_ticks(const Tick &tick)
    {
        Renderer *current = renderer.get();
        if (!current || current->occluded() || !presenter->wants_present())
            return false;
        refresh_limit();
        const auto mode = pacing_mode(*current);
        report_mode(mode);
        const bool due = tick.counts == 0;
        if (due && !mode.guard)
            return false; // display slots: a due pass runs at once
        if (mode.guard && mode.fps != paced_fps)
        {
            paced_fps = mode.fps;
            limiter_target_valid = false; // new period: restart the phase at the next present
        }
        const std::int64_t pass_limit = tick.due + (mode.guard ? pass_delay_budget() : 0);
        if (due)
        {
            // At most one present per due pass, and only on a paced grid.
            if (!paced_fps || (first_present_valid && first_present_scheduled == world.clock.scheduled))
                return false;
            const std::int64_t now = qpc_now();
            const std::int64_t slot = limiter_target_valid ? limiter_target : now;
            if (slot - now > seconds_to_qpc(imminent_slot_seconds) ||
                (std::max)(slot, now) + buffer_estimate() + present_estimate() > pass_limit)
                return false;
        }
        bool waited = false;
        std::int64_t wake = 0;
        if (!credit)
        {
            HANDLE object = current->frame_event();
            if (!object)
                return false;
            const DWORD ready = WaitForSingleObject(object, 0);
            if (ready == WAIT_FAILED)
                throw_last_error("Paced display slot poll failed");
            if (ready != WAIT_OBJECT_0)
            {
                if (due)
                    return false; // a due pass never waits for a display slot
                std::array<HANDLE, 2> handles{object, timer};
                DWORD count = 1;
                const std::int64_t remaining = (std::max)(std::int64_t{0}, tick.due - qpc_now());
                const auto units = static_cast<LONGLONG>(std::ceil(double(remaining) * 10000000.0 / double(qpc_hz)));
                if (timer)
                {
                    LARGE_INTEGER at{};
                    at.QuadPart = -(std::max)(LONGLONG{1}, units);
                    if (!SetWaitableTimerEx(timer, &at, 0, nullptr, nullptr, nullptr, 0))
                        throw_last_error("Paced timer arm failed");
                    count = 2;
                }
                const DWORD bound = static_cast<DWORD>(units / 10'000) + 2u;
                DWORD result;
                {
                    SGN_PROFILE_SCOPE("host.interp_wait_display_slot");
                    result =
                        MsgWaitForMultipleObjectsEx(count, handles.data(), bound, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                }
                if (result == WAIT_FAILED)
                    throw_last_error("Paced display slot wait failed");
                if (result != WAIT_OBJECT_0)
                    return true; // tick timer, message or bound: re-evaluate
                waited = true;
                wake = qpc_now();
            }
            take_credit(object, *current);
        }
        if (!mode.guard)
        {
            // NEW HOST (R33b): never delay the tick pass for a present. A present that
            // would still be waiting for its back buffer or running when the tick is due
            // is left to the pass: a checkpoint inside it presents as soon as the buffer
            // is free (the credit is kept).
            {
                const std::int64_t now = qpc_now();
                const std::int64_t cost =
                    present_estimate() + (current->transient_present_ready() ? 0 : buffer_estimate());
                if (now + cost > tick.due)
                {
                    sgn::host_profile::add("interp.slot_present_left_to_pass", 0);
                    wait_for_tick(tick);
                    return true;
                }
            }
            // Display slots (vsync). R30: the pose of the slot (SlotGate).
            SlotGate gate(*this, waited, wake);
            if (!presenter->present(&gate))
                return false;
            settle_credit();        // the transient Present consumed the slot
            commit_pose(gate.plan); // NEW HOST (R35)
            slot_pose = gate.pose;
            slot_valid = true;
            last_present_time = qpc_now();
            track_cost(present_cost, double(last_present_time - gate.sample)); // R33b: slot mode estimate too
            return true;
        }
        slot_valid = false;
        // NEW HOST (R28b/R30): a present that cannot finish (back buffer wait + build +
        // Present, estimated) before the pass must start is not started: the tick
        // pass runs first and the present follows it. Checked again in the gate
        // after the buffer wait.
        {
            const std::int64_t now = qpc_now();
            const std::int64_t start = limiter_target_valid ? (std::max)(limiter_target, now) : now;
            if (start + buffer_estimate() + present_estimate() > pass_limit)
            {
                if (due)
                    return false;
                sgn::host_profile::add(mode.implicit ? "interp.uncapped_present_deferred_behind_tick"
                                                     : "interp.frame_limiter_shifted_behind_tick",
                                       0);
                wait_for_tick(tick);
                return true;
            }
        }
        // NEW HOST (R27) limited path: the gate waits for the next present slot of
        // the limit (timer + spin) or declines when that slot would reach the pass.
        LimiterGate gate(*this, pass_limit);
        if (!presenter->present(&gate))
        {
            if (gate.error)
                std::rethrow_exception(gate.error); // same failure as the unlimited waits
            if (gate.input)
                return true; // dispatch input first; the absolute target is kept
            if (gate.skipped_for_tick)
            {
                if (due)
                    return false;            // the due pass runs now
                wait_for_tick(tick_state()); // fresh read: the pass is never delayed past its budget
                return true;
            }
            return false;
        }
        settle_credit(); // the transient Present consumed the slot
        limiter_presented(gate, mode.implicit);
        last_present_time = qpc_now();
        if (due)
        {
            first_present_valid = true;
            first_present_scheduled = world.clock.scheduled;
            sgn::host_profile::add("interp.present_before_due_pass", 0);
        }
        return true;
    }
    // NEW HOST (R30): profiler span and cost of the released source pass (from
    // the "queue empty" report until 44EA60 peeks again).
    void open_pass(bool with_presenter, std::int64_t due)
    {
        pass_open = true;
        pass_presenting = with_presenter;
        pass_start = qpc_now();
        pass_due = due; // NEW HOST (R33b)
        pass_yield_time = 0;
        pass_yield_stopped = false;
        if (due)
            sgn::host_profile::sample("pacing.pass_start_late_us",
                                      (std::max)(std::int64_t{0}, pass_start - due) * 1000000 / qpc_hz);
        pass_profiled = sgn::host_profile::enabled();
        if (pass_profiled)
        {
            pass_depth = sgn::host_profile::enter();
            pass_clock = std::chrono::steady_clock::now();
        }
        if (host_frame_trace::active()) // NEW HOST (R34) diagnostics
            host_frame_trace::pass_begin(due, world.clock.scheduled, world.clock.increment, counter.frequency(),
                                         with_presenter);
    }
    // NEW HOST (R31) cooperative presentation inside the source pass. The race
    // draw calls host_presentation::pass_yield_point() between its stages; while
    // a pass released with the presenter active runs, a yield presents ONE
    // interpolated frame when its display slot is due (vsync: the frame-latency
    // object is already signalled; limit/uncapped: the paced slot is due now or
    // within imminent_slot_seconds and a latency slot is free). Nothing waits for
    // a slot here (polls only, at most the imminent spin) and no message is
    // retrieved or dispatched. The present uses only the presenter's immutable
    // history (earlier sealed captures) and the renderer's own contexts; the open
    // source capture records CPU data only and is neither read nor written. No
    // source function runs and no source value is read except the pacing words
    // (scene, clock, window words) that peek() reads as well. Any failure is kept
    // and rethrown at the next peek, never inside the source pass.
    static void pass_yield_entry(void *owner) noexcept
    {
        static_cast<Impl *>(owner)->pass_yield();
    }
    void pass_yield() noexcept
    {
        if (!pass_open || !pass_presenting || in_yield || yield_error || !presenter)
            return;
        if (pass_yield_stopped)
            return;
        sgn::host_profile::add("pacing.yield_checks", 0);
        in_yield = true;
        const std::int64_t begin = qpc_now();
        try
        {
            yield_present();
        }
        catch (...)
        {
            yield_error = std::current_exception();
        }
        pass_yield_time += qpc_now() - begin; // R33b: the pass's own work excludes its presents
        in_yield = false;
    }
    // NEW HOST (R33b) source ticks run since the last pass (442490 advanced 612D00 by
    // `increment` per tick), for the profiler's sim.tick_rate. Host read only.
    void count_ticks() noexcept
    {
        if (!source_clock_gates() || !world.clock.increment)
        {
            ticks_valid = false;
            return;
        }
        const std::uint32_t scheduled = world.clock.scheduled;
        if (ticks_valid)
        {
            const std::uint32_t ticks = (scheduled - ticks_scheduled) / world.clock.increment;
            if (ticks <= 60u) // a resync jump is not a run of ticks
                sgn::host_profile::ticks(ticks);
        }
        ticks_scheduled = scheduled;
        ticks_valid = true;
    }
    // NEW HOST (R33b) the in-pass present guard. A checkpoint presents only when
    //  - at least 3/4 of a refresh passed since the last present (vsync slots) or the
    //    paced slot is due now (limit/uncapped): at most one present per refresh;
    //  - the pass can still end, with one more present, before its frame is needed:
    //    now + present estimate + the pass's remaining source work (its measured source
    //    cost minus the source time already spent) <= due + half a tick (the presenter's
    //    margin; earlier than the next tick's due time); otherwise the pass stops
    //    presenting (pacing.yield_budget_exhausted);
    //  - nothing would block: the back buffer and the next command context have
    //    retired (non-blocking fence reads) and the frame-latency object is already
    //    signalled (zero-timeout poll); otherwise pacing.yield_skipped_would_block.
    bool yield_allowed(Renderer &current, const PacingMode &mode, std::int64_t now)
    {
        if (!pass_due || !world.clock.increment)
            return false;
        const double tick = double(world.clock.increment) * double(qpc_hz) / double(source_frequency());
        // The frame of this pass is first needed by the presenter half a tick after its due
        // time (it shows now - 1.5 ticks); that is also well before the next tick is due.
        const double frame_needed = double(pass_due) + presentation_margin_ticks * tick;
        const double source_done = double(now - pass_start - pass_yield_time);
        const double remaining = (std::max)(0.0, source_cost * 1.25 - source_done);
        if (double(now) + yield_cost * 1.25 + remaining > frame_needed)
        {
            pass_yield_stopped = true;
            sgn::host_profile::add("pacing.yield_budget_exhausted", 0);
            return false;
        }
        if (!mode.guard && refresh_hz && last_present_time &&
            double(now - last_present_time) < yield_refresh_fraction * double(qpc_hz) / double(refresh_hz))
            return false; // this refresh already has its present
        if (!current.transient_present_ready())
        {
            sgn::host_profile::add("pacing.yield_skipped_would_block", 0);
            return false;
        }
        return true;
    }
    void yield_present()
    {
        if (GetCurrentThreadId() != owner_thread || world.scene != 1u || !pass_enabled() || !presenter->active())
            return;
        Renderer *current = renderer.get();
        if (!current || current->occluded() || !presenter->wants_present())
            return;
        refresh_limit();
        const auto mode = pacing_mode(*current);
        if (mode.guard)
        {
            if (mode.fps != paced_fps)
            {
                paced_fps = mode.fps;
                limiter_target_valid = false;
            }
            if (!paced_fps)
                return; // guard-only (unknown refresh): no grid to present on inside the pass
            const std::int64_t now = qpc_now();
            if (limiter_target_valid && limiter_target > now)
                return; // R33b: never wait for a paced slot inside a pass
        }
        if (!yield_allowed(*current, mode, qpc_now()))
            return;
        if (!credit)
        {
            HANDLE object = current->frame_event();
            if (!object)
                return;
            const DWORD ready = WaitForSingleObject(object, 0);
            if (ready == WAIT_FAILED)
                throw_last_error("Paced display slot poll failed");
            if (ready != WAIT_OBJECT_0)
            {
                sgn::host_profile::add("pacing.yield_skipped_would_block", 0);
                return; // no display slot yet: the next yield point checks again
            }
            take_credit(object, *current);
        }
        SGN_PROFILE_SCOPE("host.pass_yield_present");
        const std::int64_t present_begin = qpc_now();
        struct Cost
        {
            Impl &pacing;
            std::int64_t begin;
            ~Cost()
            {
                track_cost(pacing.yield_cost, double(qpc_now() - begin)); // R33b
            }
        } cost{*this, present_begin};
        if (!mode.guard)
        {
            // Display slots (vsync): the slot was signalled while the pass ran, so
            // the pose is the refresh-grid slot after the previous present's.
            SlotGate gate(*this, false, 0);
            if (!presenter->present(&gate))
                return;
            settle_credit();
            commit_pose(gate.plan); // NEW HOST (R35)
            slot_pose = gate.pose;
            slot_valid = true;
            last_present_time = qpc_now();
            sgn::host_profile::add("interp.present_inside_pass", 0);
            return;
        }
        // Limit / uncapped: the running pass is not guarded against (it runs already).
        LimiterGate gate(*this, (std::numeric_limits<std::int64_t>::max)());
        if (!presenter->present(&gate))
        {
            if (gate.error)
                std::rethrow_exception(gate.error);
            return; // input arrived during the short slot wait: the next yield point or peek
        }
        settle_credit();
        limiter_presented(gate, mode.implicit);
        last_present_time = qpc_now();
        sgn::host_profile::add("interp.present_inside_pass", 0);
    }
    void close_pass()
    {
        if (!pass_open)
            return;
        pass_open = false;
        host_frame_trace::pass_end(world.clock.scheduled); // NEW HOST (R34) diagnostics
        if (pass_presenting)
        {
            const auto length = qpc_now() - pass_start;
            track_cost(pass_cost, double(length));
            track_cost(source_cost, double((std::max)(std::int64_t{0}, length - pass_yield_time))); // R33b
        }
        count_ticks(); // NEW HOST (R33b) sim.tick_rate
        if (pass_profiled)
        {
            const auto end = std::chrono::steady_clock::now();
            sgn::host_profile::leave();
            sgn::host_profile::span("host.source_pass", pass_clock, end, pass_depth);
            sgn::host_profile::sample("sched.spin_window_us", spin_window() * 1000000 / qpc_hz);
        }
    }
    void observe_scene()
    {
        if (world.scene == last_scene)
            return;
        last_scene = world.scene;
        sgn::host_profile::mark_break(sgn::host_profile::Series::race_pass);
        if (sgn::host_profile::enabled())
        {
            char text[32];
            std::snprintf(text, sizeof text, "scene %u", unsigned(world.scene));
            sgn::host_profile::transition("pacing.scene", text);
        }
    }
    bool peek()
    {
        require_thread();
        close_pass();
        if (yield_error) // NEW HOST (R31): a failure of an in-pass present, at the pass boundary
            std::rethrow_exception(std::exchange(yield_error, nullptr));
        for (;;)
        {
            if (native.peek_removed())
                return true;
            settle_credit();
            if (presenter)
                presenter->observe_completed_frames(); // after each pass, before the next 432A60
            observe_scene();
            const bool enabled = pass_enabled();
            if (!enabled)
            {
                // 44EA60 would call only 44E9D0 here (the original spins). Block
                // on the queue first; then report "empty" once so 44E9D0 still
                // runs. If 44E9D0 re-enables the words (44E4B0), the pass that
                // follows is still behind the tick gate below.
                host_deadline_valid = false;
                if (!idle_waited)
                {
                    wait_messages(inactive_poll_ms);
                    idle_waited = true;
                    continue;
                }
            }
            bool host_cadence = false;
            std::int64_t due = 0;
            if (source_clock_gates())
            {
                host_deadline_valid = false;
                const auto tick = tick_state();
                const bool interpolating = enabled && presenting();
                if (tick.counts)
                {
                    if (interpolating && present_between_ticks(tick))
                        continue;
                    wait_for_tick(tick);
                    continue;
                }
                // NEW HOST (R30) present-then-pass (guarded modes, bounded).
                if (interpolating && present_between_ticks(tick))
                    continue;
                due = tick.due;
            }
            else
            {
                const std::uint64_t now = counter.counter();
                const std::uint64_t increment =
                    world.clock.increment ? world.clock.increment : source_frequency() / 60u;
                if (!host_deadline_valid || (now > host_deadline && now - host_deadline > increment * 30u))
                {
                    host_deadline = now; // same 30-increment resync rule as 442490
                    host_deadline_valid = true;
                }
                if (now < host_deadline)
                {
                    SGN_PROFILE_SCOPE("host.pacing_wait_for_tick");
                    const bool spin = enabled && !vsync_on();
                    (void)precise_wait(host_deadline_time(now), spin);
                    continue;
                }
                host_cadence = true;
            }
            // NEW HOST (R30): with the presenter active the race Present of this
            // pass is suppressed; it needs no display slot (the next interpolated
            // present waits for its own).
            const bool with_presenter = enabled && presenting();
            if (enabled && !with_presenter && acquire_latency() == LatencyWait::message)
                continue;
            if (host_cadence)
                host_deadline += world.clock.increment ? world.clock.increment : source_frequency() / 60u;
            idle_waited = false;
            open_pass(with_presenter, due);
            return false; // 44EAEA JZ taken: 44E9D0 and the unchanged pass follow
        }
    }
};

SourceHostLoadingPacedCounter::SourceHostLoadingPacedCounter(FrontendPerformanceCounter &inner,
                                                             const RetailTickClock &clock,
                                                             const std::uint32_t &fixed_mode_a,
                                                             const std::uint32_t &fixed_mode_b)
    : inner_(inner), clock_(clock), fixed_mode_a_(fixed_mode_a), fixed_mode_b_(fixed_mode_b)
{
    timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
}
SourceHostLoadingPacedCounter::~SourceHostLoadingPacedCounter()
{
    if (timer_)
        CloseHandle(static_cast<HANDLE>(timer_));
}
std::uint64_t SourceHostLoadingPacedCounter::frequency()
{
    return inner_.frequency();
}
std::uint64_t SourceHostLoadingPacedCounter::counter()
{
    host_frame_trace::loading_frame(); // NEW HOST (R34) diagnostics: a 445940 loading frame
    for (;;)
    {
        const auto value = inner_.counter();
        // Only 4424A8 reaches this read inside 445940; 4424DE never does.
        if (fixed_mode_a_ == 1u || fixed_mode_b_ == 1u)
            return value;
        const auto counts = source_host_counts_until_tick(clock_, static_cast<std::uint32_t>(value));
        if (counts == 0u)
            return value; // the caller's 442490 stores this same value
        const auto frequency = static_cast<std::uint32_t>(inner_.frequency());
        if (frequency == 0u)
            throw std::logic_error("Source counter frequency is zero");
        const std::uint64_t units = (std::uint64_t{counts} * 10'000'000u + frequency - 1u) / frequency;
        // 445940 pumps no messages (original); a plain timed wait keeps that.
        if (timer_)
        {
            LARGE_INTEGER due{};
            due.QuadPart = -static_cast<LONGLONG>(units);
            if (!SetWaitableTimerEx(static_cast<HANDLE>(timer_), &due, 0, nullptr, nullptr, nullptr, 0))
                throw_last_error("Loading pacing timer arm failed");
            if (WaitForSingleObject(static_cast<HANDLE>(timer_), static_cast<DWORD>(units / 10'000u) + 100u) ==
                WAIT_FAILED)
                throw_last_error("Loading pacing timer wait failed");
        }
        else
        {
            Sleep(static_cast<DWORD>((units + 9'999u) / 10'000u));
        }
    }
}

SourceHostPacedMessages::SourceHostPacedMessages(SourceApplicationMessages &native, SourceHostPacingWorld world,
                                                 FrontendPerformanceCounter &counter,
                                                 const std::unique_ptr<Renderer> &renderer_slot,
                                                 host_presentation::InterpolatingPresenter *presenter)
    : impl_(std::make_unique<Impl>(native, world, counter, renderer_slot, presenter))
{
}
SourceHostPacedMessages::~SourceHostPacedMessages() = default;

bool SourceHostPacedMessages::peek_removed()
{
    return impl_->peek();
}
std::uint32_t SourceHostPacedMessages::message_id() const
{
    return impl_->native.message_id();
}
std::uint32_t SourceHostPacedMessages::source_wparam() const
{
    return impl_->native.source_wparam();
}
void SourceHostPacedMessages::translate()
{
    impl_->native.translate();
}
void SourceHostPacedMessages::dispatch()
{
    impl_->native.dispatch();
}

// NEW HOST (R30) scheduling (see the header). Optional OS entry points are
// resolved at run time from System32 only (no import-library dependency, no
// DLL search-path exposure); each refused step is skipped and reported.
namespace
{
struct PowerThrottlingState // PROCESS_/THREAD_POWER_THROTTLING_STATE (Windows 10 1709+)
{
    ULONG version, control_mask, state_mask;
};
constexpr ULONG power_throttling_version = 1, power_execution_speed = 0x1, power_ignore_timer_resolution = 0x4;
constexpr int process_power_throttling_class = 4; // ProcessPowerThrottling
constexpr int thread_power_throttling_class = 3;  // ThreadPowerThrottling
using SetProcessInformationFn = BOOL(WINAPI *)(HANDLE, int, LPVOID, DWORD);
using SetThreadInformationFn = BOOL(WINAPI *)(HANDLE, int, LPVOID, DWORD);
using TimePeriodFn = UINT(WINAPI *)(UINT);
using AvSetFn = HANDLE(WINAPI *)(LPCWSTR, LPDWORD);
using AvRevertFn = BOOL(WINAPI *)(HANDLE);
template <class Function> Function resolve(HMODULE module, const char *name) noexcept
{
    return module ? reinterpret_cast<Function>(GetProcAddress(module, name)) : nullptr;
}
// SGN_SCHED: "0" none, "2" all but MMCSS, anything else (or unset) all.
int scheduling_level() noexcept
{
    char buffer[8]{};
    std::size_t size = 0;
    if (getenv_s(&size, buffer, sizeof buffer, "SGN_SCHED") == 0 && size > 1)
    {
        if (buffer[0] == '0')
            return 0;
        if (buffer[0] == '2')
            return 2;
    }
    return 1;
}
const char *outcome(bool ok) noexcept
{
    return ok ? "applied" : "refused by Windows (skipped)";
}
} // namespace

struct SourceHostScheduling::Impl
{
    DWORD thread_id{GetCurrentThreadId()};
    HMODULE winmm{}, avrt{};
    TimePeriodFn time_end{};
    bool period{};
    HANDLE mmcss{};
    AvRevertFn mm_revert{};
    int previous_priority{THREAD_PRIORITY_ERROR_RETURN};
    bool priority_set{};

    Impl()
    {
        const int level = scheduling_level();
        if (level == 0)
        {
            sgn::host_profile::info("sched", "off (SGN_SCHED=0)");
            OutputDebugStringA("SGN sched: off (SGN_SCHED=0)\n");
            return;
        }
        // 1 ms system timer: millisecond timeouts and Sleep wake on time. The
        // pacing waits already use high-resolution waitable timers.
        winmm = LoadLibraryExW(L"winmm.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        const auto time_begin = resolve<TimePeriodFn>(winmm, "timeBeginPeriod");
        time_end = resolve<TimePeriodFn>(winmm, "timeEndPeriod");
        period = time_begin && time_end && time_begin(1) == 0;
        sgn::host_profile::info("sched.timer_period_1ms", outcome(period));
        // Power throttling (EcoQoS / execution speed) off for the process and this
        // thread; timer resolution requests honoured while occluded (Windows 11).
        const HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
        const auto set_process = resolve<SetProcessInformationFn>(kernel, "SetProcessInformation");
        const auto set_thread = resolve<SetThreadInformationFn>(kernel, "SetThreadInformation");
        bool process_speed = false, process_timer = false, thread_speed = false;
        if (set_process)
        {
            PowerThrottlingState speed{power_throttling_version, power_execution_speed, 0};
            process_speed =
                set_process(GetCurrentProcess(), process_power_throttling_class, &speed, sizeof speed) != FALSE;
            PowerThrottlingState resolution{power_throttling_version, power_ignore_timer_resolution, 0};
            process_timer = set_process(GetCurrentProcess(), process_power_throttling_class, &resolution,
                                        sizeof resolution) != FALSE;
        }
        if (set_thread)
        {
            PowerThrottlingState speed{power_throttling_version, power_execution_speed, 0};
            thread_speed = set_thread(GetCurrentThread(), thread_power_throttling_class, &speed, sizeof speed) != FALSE;
        }
        sgn::host_profile::info("sched.process_throttling_off", outcome(process_speed));
        sgn::host_profile::info("sched.keep_timer_resolution", outcome(process_timer));
        sgn::host_profile::info("sched.thread_throttling_off", outcome(thread_speed));
        // Above-normal priority first (kept if MMCSS is refused or disabled).
        previous_priority = GetThreadPriority(GetCurrentThread());
        priority_set = previous_priority != THREAD_PRIORITY_ERROR_RETURN &&
                       SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL) != FALSE;
        sgn::host_profile::info("sched.thread_priority_above_normal", outcome(priority_set));
        if (level == 1)
        {
            avrt = LoadLibraryExW(L"avrt.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            const auto mm_set = resolve<AvSetFn>(avrt, "AvSetMmThreadCharacteristicsW");
            mm_revert = resolve<AvRevertFn>(avrt, "AvRevertMmThreadCharacteristics");
            DWORD task = 0;
            if (mm_set && mm_revert)
                mmcss = mm_set(L"Games", &task);
            sgn::host_profile::info("sched.mmcss_games", outcome(mmcss != nullptr));
        }
        else
            sgn::host_profile::info("sched.mmcss_games", "off (SGN_SCHED=2)");
        char text[200];
        std::snprintf(text, sizeof text,
                      "SGN sched: timer 1ms %d, process throttling off %d, keep timer resolution %d, thread "
                      "throttling off %d, priority above normal %d, MMCSS Games %d\n",
                      int(period), int(process_speed), int(process_timer), int(thread_speed), int(priority_set),
                      int(mmcss != nullptr));
        OutputDebugStringA(text);
    }
    ~Impl()
    {
        if (GetCurrentThreadId() == thread_id)
        {
            if (mmcss && mm_revert)
                mm_revert(mmcss);
            if (priority_set)
                SetThreadPriority(GetCurrentThread(), previous_priority);
        }
        if (period && time_end)
            time_end(1);
        if (avrt)
            FreeLibrary(avrt);
        if (winmm)
            FreeLibrary(winmm);
    }
    Impl(const Impl &) = delete;
    Impl &operator=(const Impl &) = delete;
};
SourceHostScheduling::SourceHostScheduling() noexcept
{
    try
    {
        impl_ = std::make_unique<Impl>();
    }
    catch (...)
    {
        // Scheduling is an optimisation: the game runs without it.
    }
}
SourceHostScheduling::~SourceHostScheduling() = default;

} // namespace sgn::retail
