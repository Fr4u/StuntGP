#include "sgn/host_profiler.hpp"
#include "sgn/host_frame_trace.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// NEW HOST diagnostics (see the header). Everything below is reached only while
// SGN_PROFILE is set; the game thread is the only recording thread (the first
// thread that records; calls from any other thread are ignored).
namespace sgn::host_profile
{
namespace
{
using steady = std::chrono::steady_clock;
constexpr double window_seconds = 2.0;
constexpr std::size_t ring_size = 16384; // events kept for the breakdowns (power of two)
constexpr std::size_t worst_kept = 10;
constexpr std::size_t breakdown_items = 24;
constexpr std::size_t transitions_kept = 48;
constexpr std::size_t detail_chars = 112;
constexpr std::size_t series_count = 2;
constexpr const char *series_names[series_count] = {"present", "race_pass"};
// NEW HOST (R30): stages whose longest spans are kept with their nested breakdown.
constexpr const char *tracked_stages[] = {"host.source_pass", "pass.update_442810", "pass.draw_4427b0",
                                          "host.interp_present"};
constexpr std::size_t tracked_count = sizeof tracked_stages / sizeof tracked_stages[0];

enum class Kind : std::uint8_t
{
    span,
    value,
    sample
};
struct Event
{
    const char *stage;
    std::int64_t time;  // ns since the profiler epoch (span: its end)
    std::int64_t value; // span: duration; value: nanoseconds; sample: quantity
    std::uint16_t depth;
    Kind kind;
};
struct Item
{
    const char *stage;
    std::int64_t total, worst;
    std::uint32_t calls;
    std::uint16_t depth;
    Kind kind;
};
struct Record
{
    std::int64_t start{}, length{};
    std::array<Item, breakdown_items> items{};
    std::size_t count{};
    std::uint32_t dropped{}; // distinct stages beyond breakdown_items
    std::int64_t covered{};  // outermost spans inside the interval (clipped)
    unsigned outer{};        // depth of those spans
    bool truncated{};        // the event ring no longer reached the interval start
};
struct Worst
{
    std::array<Record, worst_kept> records{};
    std::size_t count{};
    // The record to overwrite for a new candidate of `length`, or nullptr.
    Record *slot(std::int64_t length) noexcept
    {
        if (count < worst_kept)
            return &records[count++];
        auto *least = &records[0];
        for (auto &r : records)
            if (r.length < least->length)
                least = &r;
        return length > least->length ? least : nullptr;
    }
    void reset() noexcept
    {
        count = 0;
    }
};
struct Entry
{
    const char *stage;
    std::int64_t total{};
    std::uint64_t calls{};
    std::int64_t worst{};
    int tracked{-1};
};
struct SeriesData
{
    bool has_last{};
    std::int64_t last{};
    std::vector<std::int64_t> intervals;
    Worst worst;
};
struct Tracked
{
    std::vector<std::int64_t> lengths;
    Worst worst;
};
struct Transition
{
    std::int64_t time;
    const char *what;
    std::array<char, detail_chars> detail;
};
struct Stats
{
    std::size_t n{};
    double mean{}, p50{}, p90{}, p99{}, p999{}, max{}, low1_fps{}, low01_fps{};
    std::size_t over15{}, over2{};
};

struct State
{
    bool enabled{};
    std::string path;
    steady::time_point epoch{steady::now()};
    bool owner_set{};
    std::thread::id owner;
    std::vector<Entry> entries, samples;
    std::vector<Event> ring;
    std::uint64_t ring_next{};
    std::array<SeriesData, series_count> series;
    std::array<Tracked, tracked_count> tracked;
    std::vector<Transition> transitions;
    std::size_t transitions_dropped{};
    std::vector<std::pair<std::string, std::string>> infos;
    std::uint64_t frames{}, window_index{};
    std::uint64_t sim_ticks{}; // NEW HOST (R33b)
    std::int64_t window_start{};
    std::string text;                // report scratch (game thread)
    std::vector<std::int64_t> order; // percentile scratch
    // Background writer.
    std::mutex mutex;
    std::condition_variable wake;
    std::string pending;
    bool pending_ready{}, writer_started{};
    State()
    {
        char *value = nullptr;
        std::size_t size = 0;
        if (_dupenv_s(&value, &size, "SGN_PROFILE") == 0 && value)
        {
            path = value;
            std::free(value);
        }
        enabled = !path.empty();
        if (!enabled)
            return;
        try
        {
            ring.resize(ring_size);
            entries.reserve(256);
            samples.reserve(64);
            for (auto &s : series)
                s.intervals.reserve(8192);
            for (auto &t : tracked)
                t.lengths.reserve(2048);
            transitions.reserve(transitions_kept);
            text.reserve(64 * 1024);
            order.reserve(8192);
        }
        catch (...)
        {
            enabled = false;
        }
    }
};
State &state()
{
    // Intentionally never destroyed: the detached writer may still wait on it at exit.
    static State *s = new State;
    return *s;
}
thread_local unsigned depth_of_thread = 0;

std::int64_t ns(const State &s, steady::time_point t) noexcept
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(t - s.epoch).count();
}
// Recording is limited to the first recording thread (the game thread).
bool recording(State &s) noexcept
{
    if (!s.enabled)
        return false;
    const auto id = std::this_thread::get_id();
    if (!s.owner_set)
    {
        s.owner = id;
        s.owner_set = true;
        return true;
    }
    return s.owner == id;
}
void log_event(State &s, const char *stage, std::int64_t time, std::int64_t value, unsigned depth, Kind kind) noexcept
{
    s.ring[static_cast<std::size_t>(s.ring_next++) & (ring_size - 1)] = {
        stage, time, value, static_cast<std::uint16_t>((std::min)(depth, 0xffffu)), kind};
}
Entry *entry(std::vector<Entry> &table, const char *stage) noexcept
{
    for (auto &e : table)
        if (e.stage == stage)
            return &e;
    try
    {
        Entry e{stage};
        for (std::size_t i = 0; i < tracked_count; ++i)
            if (std::strcmp(stage, tracked_stages[i]) == 0)
                e.tracked = static_cast<int>(i);
        table.push_back(e);
        return &table.back();
    }
    catch (...)
    {
        return nullptr;
    }
}
void accumulate(Entry &e, std::int64_t value) noexcept
{
    e.total += value;
    ++e.calls;
    e.worst = (std::max)(e.worst, value);
}
// Breakdown of every event that ended in (start, end]. only_deeper: nested
// spans of a span at depth `parent` (values and samples in the interval too).
void collect(const State &s, Record &r, std::int64_t start, std::int64_t end, bool only_deeper,
             unsigned parent) noexcept
{
    r.start = start;
    r.length = end - start;
    r.count = 0;
    r.dropped = 0;
    r.covered = 0;
    r.outer = (std::numeric_limits<unsigned>::max)();
    r.truncated = false;
    const std::uint64_t available = (std::min)(s.ring_next, std::uint64_t{ring_size});
    bool reached = false;
    for (std::uint64_t k = 0; k < available; ++k)
    {
        const auto &e = s.ring[static_cast<std::size_t>(s.ring_next - 1 - k) & (ring_size - 1)];
        if (e.time <= start)
        {
            reached = true;
            break;
        }
        if (e.time > end)
            continue;
        if (only_deeper && e.kind == Kind::span && e.depth <= parent)
            continue;
        const std::int64_t amount = e.kind == Kind::span ? e.time - (std::max)(e.time - e.value, start) : e.value;
        if (e.kind == Kind::span)
            r.outer = (std::min)(r.outer, unsigned(e.depth));
        Item *item = nullptr;
        for (std::size_t i = 0; i < r.count; ++i)
            if (r.items[i].stage == e.stage && r.items[i].depth == e.depth && r.items[i].kind == e.kind)
            {
                item = &r.items[i];
                break;
            }
        if (!item)
        {
            if (r.count == breakdown_items)
            {
                ++r.dropped;
                continue;
            }
            item = &r.items[r.count++];
            *item = {e.stage, 0, 0, 0, e.depth, e.kind};
        }
        item->total += amount;
        item->worst = (std::max)(item->worst, amount);
        ++item->calls;
    }
    r.truncated = !reached && s.ring_next > ring_size;
    for (std::size_t i = 0; i < r.count; ++i)
        if (r.items[i].kind == Kind::span && r.items[i].depth == r.outer)
            r.covered += r.items[i].total;
}
Stats statistics(std::vector<std::int64_t> &order, const std::vector<std::int64_t> &values)
{
    Stats st;
    st.n = values.size();
    if (!st.n)
        return st;
    order.assign(values.begin(), values.end());
    std::sort(order.begin(), order.end());
    const auto at = [&](double p) {
        const auto rank = static_cast<std::size_t>(std::ceil(p * double(st.n)));
        return double(order[(std::min)(st.n - 1, rank ? rank - 1 : 0)]) / 1e6;
    };
    double sum = 0;
    for (const auto v : order)
        sum += double(v);
    st.mean = sum / double(st.n) / 1e6;
    st.p50 = at(0.5);
    st.p90 = at(0.9);
    st.p99 = at(0.99);
    st.p999 = at(0.999);
    st.max = double(order.back()) / 1e6;
    const auto low = [&](std::size_t divisor) {
        const std::size_t k = (std::max)(std::size_t{1}, st.n / divisor);
        double slow = 0;
        for (std::size_t i = st.n - k; i < st.n; ++i)
            slow += double(order[i]);
        return slow > 0 ? 1e9 * double(k) / slow : 0.0;
    };
    st.low1_fps = low(100);
    st.low01_fps = low(1000);
    const double median = double(order[(st.n - 1) / 2]);
    for (const auto v : order)
    {
        st.over15 += double(v) > 1.5 * median;
        st.over2 += double(v) > 2.0 * median;
    }
    return st;
}
void appendf(std::string &out, const char *format, ...)
{
    char line[512];
    va_list args;
    va_start(args, format);
    const int n = std::vsnprintf(line, sizeof line, format, args);
    va_end(args);
    if (n > 0)
        out.append(line, (std::min)(static_cast<std::size_t>(n), sizeof line - 1));
}
void write_record(std::string &out, const Record &r, std::int64_t window_start, double median_ms, const char *label,
                  std::size_t rank)
{
    appendf(out, "#%zu %s at +%.3f s: %.3f ms", rank, label, double(r.start + r.length - window_start) / 1e9,
            double(r.length) / 1e6);
    if (median_ms > 0)
        appendf(out, " (%.1fx median)", double(r.length) / 1e6 / median_ms);
    if (r.outer != (std::numeric_limits<unsigned>::max)())
        appendf(out, "; scopes at depth %u cover %.3f ms, untracked %.3f ms", r.outer, double(r.covered) / 1e6,
                double(r.length - r.covered) / 1e6);
    if (r.truncated)
        out += " [event ring truncated]";
    out += '\n';
    std::array<const Item *, breakdown_items> sorted{};
    for (std::size_t i = 0; i < r.count; ++i)
        sorted[i] = &r.items[i];
    std::sort(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(r.count), [](const Item *a, const Item *b) {
        if (a->kind != b->kind)
            return a->kind < b->kind;
        return a->total > b->total;
    });
    for (std::size_t i = 0; i < r.count; ++i)
    {
        const auto &it = *sorted[i];
        if (it.kind == Kind::span)
            appendf(out, "      span  d%-2u %-44s x%-4u %9.3f ms  worst %8.3f ms\n", unsigned(it.depth), it.stage,
                    it.calls, double(it.total) / 1e6, double(it.worst) / 1e6);
        else if (it.kind == Kind::value)
        {
            if (it.total == 0)
                appendf(out, "      count     %-44s x%-4u\n", it.stage, it.calls);
            else
                appendf(out, "      value     %-44s x%-4u %9.3f ms  worst %8.3f ms\n", it.stage, it.calls,
                        double(it.total) / 1e6, double(it.worst) / 1e6);
        }
        else
            appendf(out, "      sample    %-44s x%-4u total %lld max %lld\n", it.stage, it.calls,
                    static_cast<long long>(it.total), static_cast<long long>(it.worst));
    }
    if (r.dropped)
        appendf(out, "      (%u more events of other stages)\n", r.dropped);
}
void writer_main(State *s)
{
    std::string local;
    for (;;)
    {
        {
            std::unique_lock lock(s->mutex);
            s->wake.wait(lock, [s] { return s->pending_ready; });
            local.swap(s->pending);
            s->pending_ready = false;
        }
        std::FILE *file = nullptr;
        if (fopen_s(&file, s->path.c_str(), "w") == 0 && file)
        {
            std::fwrite(local.data(), 1, local.size(), file);
            std::fclose(file);
        }
        const auto history = s->path + ".history";
        if (fopen_s(&file, history.c_str(), "a") == 0 && file)
        {
            std::fwrite(local.data(), 1, local.size(), file);
            std::fputs("\n", file);
            std::fclose(file);
        }
    }
}
void format_report(State &s, double seconds)
{
    auto &out = s.text;
    out.clear();
    const auto &present = s.series[static_cast<unsigned>(Series::present)];
    // NEW HOST (R33b): the simulation rate first; it must read 60.00 (the source clock).
    appendf(out, "sim.tick_rate %.2f ticks/s (%llu ticks in %.3f s)%s\n", double(s.sim_ticks) / seconds,
            static_cast<unsigned long long>(s.sim_ticks), seconds,
            s.sim_ticks && double(s.sim_ticks) / seconds < 59.5 ? "  <<< BELOW THE 60 Hz SOURCE CLOCK" : "");
    appendf(out, "window %llu seconds %.3f frames %llu fps %.2f presents %zu presents_per_s %.2f\n",
            static_cast<unsigned long long>(s.window_index), seconds, static_cast<unsigned long long>(s.frames),
            s.frames / seconds, present.intervals.size(), double(present.intervals.size()) / seconds);
    for (const auto &[key, text] : s.infos)
        appendf(out, "info %-24s %s\n", key.c_str(), text.c_str());
    out += "\n== intervals between marks (ms); low fps = average rate over the slowest 1% / 0.1% ==\n";
    appendf(out, "%-10s %6s %7s %7s %7s %7s %7s %7s %9s %10s %8s %7s\n", "series", "count", "mean", "p50", "p90", "p99",
            "p99.9", "max", "1%low", "0.1%low", ">1.5xmed", ">2xmed");
    std::array<Stats, series_count> series_stats{};
    for (std::size_t i = 0; i < series_count; ++i)
    {
        const auto st = statistics(s.order, s.series[i].intervals);
        series_stats[i] = st;
        appendf(out, "%-10s %6zu %7.3f %7.3f %7.3f %7.3f %7.3f %7.3f %9.1f %10.1f %8zu %7zu\n", series_names[i], st.n,
                st.mean, st.p50, st.p90, st.p99, st.p999, st.max, st.low1_fps, st.low01_fps, st.over15, st.over2);
    }
    out += "\n== tracked spans (ms) ==\n";
    appendf(out, "%-28s %6s %7s %7s %7s %7s %7s %7s\n", "stage", "count", "mean", "p50", "p90", "p99", "p99.9", "max");
    std::array<Stats, tracked_count> tracked_stats{};
    for (std::size_t i = 0; i < tracked_count; ++i)
    {
        const auto st = statistics(s.order, s.tracked[i].lengths);
        tracked_stats[i] = st;
        appendf(out, "%-28s %6zu %7.3f %7.3f %7.3f %7.3f %7.3f %7.3f\n", tracked_stages[i], st.n, st.mean, st.p50,
                st.p90, st.p99, st.p999, st.max);
    }
    out += "\n";
    std::sort(s.entries.begin(), s.entries.end(), [](const Entry &a, const Entry &b) { return a.total > b.total; });
    appendf(out, "%-44s %12s %10s %12s %12s\n", "stage", "ms_per_frame", "calls", "avg_us", "worst_ms");
    const double frames = s.frames ? static_cast<double>(s.frames) : 1.0;
    for (const auto &e : s.entries)
        if (e.calls)
            appendf(out, "%-44s %12.3f %10llu %12.2f %12.3f\n", e.stage, e.total / 1e6 / frames,
                    static_cast<unsigned long long>(e.calls), e.calls ? e.total / 1e3 / e.calls : 0.0, e.worst / 1e6);
    if (!s.samples.empty())
    {
        out += "\n== quantities ==\n";
        appendf(out, "%-44s %10s %14s %14s %16s\n", "name", "calls", "avg", "max", "total");
        for (const auto &e : s.samples)
            if (e.calls)
                appendf(out, "%-44s %10llu %14.1f %14lld %16lld\n", e.stage, static_cast<unsigned long long>(e.calls),
                        double(e.total) / double(e.calls), static_cast<long long>(e.worst),
                        static_cast<long long>(e.total));
    }
    out += "\n== transitions ==\n";
    for (const auto &t : s.transitions)
        appendf(out, "+%.3f s %-36s %s\n", double(t.time - s.window_start) / 1e9, t.what, t.detail.data());
    if (s.transitions_dropped)
        appendf(out, "(%zu more transitions)\n", s.transitions_dropped);
    for (std::size_t i = 0; i < series_count; ++i)
    {
        auto &sr = s.series[i];
        const double median = series_stats[i].p50;
        std::array<const Record *, worst_kept> list{};
        std::size_t n = 0;
        for (std::size_t k = 0; k < sr.worst.count; ++k)
            if (double(sr.worst.records[k].length) / 1e6 > 2.0 * median)
                list[n++] = &sr.worst.records[k];
        std::sort(list.begin(), list.begin() + static_cast<std::ptrdiff_t>(n),
                  [](const Record *a, const Record *b) { return a->length > b->length; });
        appendf(out, "\n== spikes: %s intervals > 2x median %.3f ms (%zu in window, worst %zu listed) ==\n",
                series_names[i], median, series_stats[i].over2, n);
        for (std::size_t k = 0; k < n; ++k)
            write_record(out, *list[k], s.window_start, median, "interval", k + 1);
    }
    for (std::size_t i = 0; i < tracked_count; ++i)
    {
        auto &t = s.tracked[i];
        if (!t.worst.count)
            continue;
        std::array<const Record *, worst_kept> list{};
        for (std::size_t k = 0; k < t.worst.count; ++k)
            list[k] = &t.worst.records[k];
        std::sort(list.begin(), list.begin() + static_cast<std::ptrdiff_t>(t.worst.count),
                  [](const Record *a, const Record *b) { return a->length > b->length; });
        appendf(out, "\n== worst %s (median %.3f ms), nested scopes ==\n", tracked_stages[i], tracked_stats[i].p50);
        for (std::size_t k = 0; k < t.worst.count; ++k)
            write_record(out, *list[k], s.window_start, tracked_stats[i].p50, "span", k + 1);
    }
}
void hand_to_writer(State &s)
{
    {
        std::lock_guard lock(s.mutex);
        s.pending.swap(s.text); // the writer's previous buffer comes back as scratch
        s.pending_ready = true;
    }
    s.wake.notify_one();
    if (!s.writer_started)
    {
        std::thread(writer_main, &s).detach();
        s.writer_started = true;
    }
}
void reset_window(State &s, std::int64_t now) noexcept
{
    for (auto &e : s.entries)
    {
        e.total = 0;
        e.calls = 0;
        e.worst = 0;
    }
    for (auto &e : s.samples)
    {
        e.total = 0;
        e.calls = 0;
        e.worst = 0;
    }
    for (auto &sr : s.series)
    {
        sr.intervals.clear();
        sr.worst.reset();
    }
    for (auto &t : s.tracked)
    {
        t.lengths.clear();
        t.worst.reset();
    }
    s.transitions.clear();
    s.transitions_dropped = 0;
    s.frames = 0;
    s.sim_ticks = 0;
    s.window_start = now;
    ++s.window_index;
}
// Called only at safe points between frames (marks and pass ends).
void maybe_flush(State &s, std::int64_t now) noexcept
{
    const double seconds = double(now - s.window_start) / 1e9;
    if (seconds < window_seconds)
        return;
    const auto begin = steady::now();
    try
    {
        format_report(s, seconds);
        hand_to_writer(s);
    }
    catch (...)
    {
    }
    reset_window(s, now);
    const auto end = steady::now();
    // Its own cost is visible in the next window (and in any spike it causes).
    if (Entry *e = entry(s.entries, "profiler.flush"))
        accumulate(*e, ns(s, end) - ns(s, begin));
    log_event(s, "profiler.flush", ns(s, end), ns(s, end) - ns(s, begin), depth_of_thread, Kind::span);
}
void series_mark(State &s, Series which, std::int64_t now) noexcept
{
    auto &sr = s.series[static_cast<unsigned>(which)];
    if (sr.has_last && now > sr.last)
    {
        const std::int64_t length = now - sr.last;
        try
        {
            sr.intervals.push_back(length);
        }
        catch (...)
        {
        }
        if (Record *r = sr.worst.slot(length))
            collect(s, *r, sr.last, now, false, 0);
    }
    sr.has_last = true;
    sr.last = now;
}
} // namespace
bool enabled() noexcept
{
    return state().enabled;
}
bool scopes_enabled() noexcept
{
    return state().enabled || host_frame_trace::scope_wanted(depth_of_thread); // NEW HOST (R34)
}
unsigned enter() noexcept
{
    return depth_of_thread++;
}
void leave() noexcept
{
    if (depth_of_thread)
        --depth_of_thread;
}
void add(const char *stage, std::int64_t nanoseconds) noexcept
{
    host_frame_trace::value(stage, nanoseconds); // NEW HOST (R34): no-op unless a frame trace session records
    auto &s = state();
    if (!recording(s))
        return;
    if (Entry *e = entry(s.entries, stage))
        accumulate(*e, nanoseconds);
    log_event(s, stage, ns(s, steady::now()), nanoseconds, depth_of_thread, Kind::value);
}
void sample(const char *stage, std::int64_t value) noexcept
{
    auto &s = state();
    if (!recording(s))
        return;
    if (Entry *e = entry(s.samples, stage))
        accumulate(*e, value);
    log_event(s, stage, ns(s, steady::now()), value, depth_of_thread, Kind::sample);
}
void span(const char *stage, steady::time_point start, steady::time_point end, unsigned depth) noexcept
{
    host_frame_trace::span(stage, start, end, depth); // NEW HOST (R34): no-op unless a frame trace session records
    auto &s = state();
    if (!recording(s))
        return;
    const std::int64_t t0 = ns(s, start), t1 = ns(s, end), length = t1 - t0;
    Entry *e = entry(s.entries, stage);
    if (e)
    {
        accumulate(*e, length);
        if (e->tracked >= 0)
        {
            auto &t = s.tracked[static_cast<std::size_t>(e->tracked)];
            try
            {
                t.lengths.push_back(length);
            }
            catch (...)
            {
            }
            // Children are in the ring already; this span is logged after them.
            if (Record *r = t.worst.slot(length))
                collect(s, *r, t0, t1, true, depth);
        }
    }
    log_event(s, stage, t1, length, depth, Kind::span);
    if (depth == 0)
        maybe_flush(s, t1); // a top-level scope ended: between frames
}
void frame_boundary() noexcept
{
    auto &s = state();
    if (!recording(s))
        return;
    ++s.frames;
    const auto now = ns(s, steady::now());
    series_mark(s, Series::race_pass, now);
    if (depth_of_thread == 0) // never inside a measured scope (the pacing's host.source_pass holds this one)
        maybe_flush(s, now);
}
void ticks(std::uint32_t count) noexcept
{
    auto &s = state();
    if (!recording(s))
        return;
    s.sim_ticks += count;
}
void mark(Series which) noexcept
{
    auto &s = state();
    if (!recording(s) || static_cast<unsigned>(which) >= series_count)
        return;
    const auto now = ns(s, steady::now());
    series_mark(s, which, now);
    log_event(s, which == Series::present ? "mark.present" : "mark.race_pass", now, 0, depth_of_thread, Kind::value);
    if (depth_of_thread == 0)
        maybe_flush(s, now);
}
void mark_break(Series which) noexcept
{
    auto &s = state();
    if (!recording(s) || static_cast<unsigned>(which) >= series_count)
        return;
    s.series[static_cast<unsigned>(which)].has_last = false;
}
void transition(const char *what, const char *detail) noexcept
{
    auto &s = state();
    if (!recording(s) || !what)
        return;
    const auto now = ns(s, steady::now());
    log_event(s, what, now, 0, depth_of_thread, Kind::value);
    if (s.transitions.size() >= transitions_kept)
    {
        ++s.transitions_dropped;
        return;
    }
    Transition t{now, what, {}};
    if (detail)
        strncpy_s(t.detail.data(), t.detail.size(), detail, _TRUNCATE);
    s.transitions.push_back(t); // reserved: no allocation
}
void info(const char *key, const char *text) noexcept
{
    auto &s = state();
    if (!s.enabled || !key)
        return;
    try
    {
        for (auto &[k, v] : s.infos)
            if (k == key)
            {
                v = text ? text : "";
                return;
            }
        s.infos.emplace_back(key, text ? text : "");
    }
    catch (...)
    {
    }
}
} // namespace sgn::host_profile
