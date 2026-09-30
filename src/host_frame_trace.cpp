#include "sgn/host_frame_trace.hpp"
#include "sgn/host_presentation.hpp"
#include "sgn/host_presentation_presenter.hpp"
#include "sgn/host_profiler.hpp"
#include <windows.h>
#include <dxgi.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

// NEW HOST (R34) frame trace and stutter detector (see the header). The game
// thread only appends fixed-size records to a single-producer ring; the
// background writer thread drains it every 100 ms, derives the displayed
// frames from the DXGI statistics, detects stutters, attributes causes and
// writes the logs. No hook changes any value the game, the pacing, the
// presenter or the renderer reads.
namespace sgn::host_frame_trace
{
namespace
{
constexpr std::size_t detector_ring_events = std::size_t{1} << 15; // ~8 s of detector events (2.6 MB)
constexpr std::size_t trace_ring_events = std::size_t{1} << 18;    // ~50 s of trace events (21 MB)
constexpr unsigned detector_scope_depth = 2;                       // detector: scopes at depth 0 and 1
constexpr unsigned trace_individual_depth = 3;                     // trace: depth 0..2 always one record each
constexpr std::int64_t trace_individual_ns = 50'000;               // deeper scopes: one record from 50 us on
constexpr std::size_t sum_slots = 64;                              // deeper short scopes summed per name
constexpr int writer_period_ms = 100;
constexpr std::int64_t second_ns = 1'000'000'000;
constexpr std::int64_t history_ns = 2'500'000'000; // writer-side history kept for causes
constexpr std::int64_t context_ns = 500'000'000;   // context of one stutter
constexpr std::size_t present_slots = 4096;        // presents kept by id (power of two)
constexpr std::size_t stutter_lines_per_second = 40;
constexpr std::uint64_t log_rotate_bytes = 8ull << 20;

enum class Type : std::uint8_t
{
    pass_begin,
    pass_end,
    span,
    span_sum,
    value,
    loading,
    interp,
    camera,
    present,
    stats
};
enum Scene : std::uint8_t
{
    scene_unknown,
    scene_race,
    scene_pause,
    scene_menu,
    scene_loading,
    scene_other
};
constexpr const char *scene_names[] = {"unknown", "race", "pause", "menu", "loading", "other"};
// Flags of the present record.
constexpr std::uint8_t present_sync = 1, present_tearing = 2, present_interp = 4, present_in_pass = 8;
// Flags of the camera record.
constexpr std::uint8_t camera_cut = 1, camera_view_changed = 2, camera_pair = 4, camera_interpolated = 8;

struct Event
{
    std::int64_t t{}; // ns, steady_clock (QPC) base
    const char *name{};
    Type type{};
    std::uint8_t scene{}, flags{}, depth{};
    std::uint32_t u{};
    std::array<std::int64_t, 7> v{};
};
std::int64_t pack(double x) noexcept
{
    return std::bit_cast<std::int64_t>(x);
}
double unpack(std::int64_t x) noexcept
{
    return std::bit_cast<double>(x);
}
std::int64_t pack2(float a, float b) noexcept
{
    return static_cast<std::int64_t>(std::uint64_t{std::bit_cast<std::uint32_t>(a)} |
                                     (std::uint64_t{std::bit_cast<std::uint32_t>(b)} << 32));
}
float unpack_low(std::int64_t x) noexcept
{
    return std::bit_cast<float>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(x)));
}
float unpack_high(std::int64_t x) noexcept
{
    return std::bit_cast<float>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(x) >> 32));
}

struct Sum
{
    const char *name{};
    std::int64_t total{};
    std::uint32_t count{};
};
struct Config
{
    bool trace{};
    std::filesystem::path trace_path, reports;
    std::uint64_t trace_cap{};
    std::int64_t t0{}; // ns
    FILETIME wall0{};  // UTC at t0
};
class Analyzer;
// Shared state. Intentionally never destroyed (a writer that outlives an
// unorderly exit never touches a destroyed object).
struct Shared
{
    std::atomic<bool> active{};
    std::atomic<DWORD> owner{};
    bool trace{};
    const std::uint32_t *scene_word{}, *pause_word{};
    std::uint8_t scene{scene_unknown};
    bool pass_open{}, loading{}, interp_pending{};
    std::int64_t qpc_frequency{1};
    std::vector<Event> ring;
    std::uint64_t mask{};
    std::atomic<std::uint64_t> head{}, tail{}, dropped{};
    std::array<Sum, sum_slots> sums{};
    std::array<std::uint8_t, sum_slots> sum_used{};
    std::size_t sum_count{};
    std::int64_t info_next{};
    // Writer.
    std::thread writer;
    std::mutex mutex;
    std::condition_variable wake;
    bool stop{};
    // Published by the writer for the profiler header (read by the game thread).
    std::atomic<std::uint64_t> stutters{}, stutters_race{}, stutters_menu{}, stutters_loading{}, lost{};
    std::atomic<std::uint32_t> fps_x10{}, low1_x10{};
};
Shared &shared() noexcept
{
    static Shared *s = new Shared;
    return *s;
}
bool on(const Shared &s) noexcept
{
    return s.active.load(std::memory_order_relaxed) && GetCurrentThreadId() == s.owner.load(std::memory_order_relaxed);
}
std::int64_t now_ns() noexcept
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
std::int64_t ns_of(std::chrono::steady_clock::time_point t) noexcept
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch()).count();
}
// The steady_clock scaling of a QPC value (whole seconds + remainder, no overflow).
std::int64_t qpc_to_ns(std::int64_t qpc, std::int64_t frequency) noexcept
{
    if (frequency <= 0)
        return 0;
    const std::int64_t whole = (qpc / frequency) * 1'000'000'000;
    const std::int64_t part = (qpc % frequency) * 1'000'000'000 / frequency;
    return whole + part;
}
void push(Shared &s, const Event &e) noexcept
{
    const auto h = s.head.load(std::memory_order_relaxed);
    if (h - s.tail.load(std::memory_order_acquire) > s.mask)
    {
        s.dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    s.ring[static_cast<std::size_t>(h & s.mask)] = e;
    s.head.store(h + 1, std::memory_order_release);
}
std::uint8_t scene_now(const Shared &s) noexcept
{
    if (!s.scene_word)
        return scene_unknown;
    const auto raw = *s.scene_word;
    if (raw == 1u)
        return s.pause_word && *s.pause_word == 1u ? scene_pause : scene_race;
    if (raw == 2u)
        return scene_menu;
    return scene_other;
}
void flush_sums(Shared &s, std::int64_t t) noexcept
{
    for (std::size_t i = 0; i < s.sum_count; ++i)
    {
        auto &slot = s.sums[s.sum_used[i]];
        Event e;
        e.t = t;
        e.type = Type::span_sum;
        e.name = slot.name;
        e.scene = s.scene;
        e.v[0] = slot.total;
        e.v[1] = slot.count;
        push(s, e);
        slot = {};
    }
    s.sum_count = 0;
}
bool add_sum(Shared &s, const char *name, std::int64_t duration) noexcept
{
    const auto hash = (reinterpret_cast<std::uintptr_t>(name) >> 3) * 0x9E3779B97F4A7C15ull;
    for (std::size_t probe = 0; probe < sum_slots; ++probe)
    {
        const auto index = static_cast<std::size_t>((hash + probe) & (sum_slots - 1));
        auto &slot = s.sums[index];
        if (slot.name == name)
        {
            slot.total += duration;
            ++slot.count;
            return true;
        }
        if (!slot.name)
        {
            slot = {name, duration, 1};
            s.sum_used[s.sum_count++] = static_cast<std::uint8_t>(index);
            return true;
        }
    }
    return false;
}
void publish_info(Shared &s, std::int64_t now) noexcept
{
    if (now < s.info_next || !sgn::host_profile::enabled())
        return;
    s.info_next = now + second_ns;
    char text[240];
    std::snprintf(
        text, sizeof text,
        "%llu (race %llu, menu %llu, loading %llu), lost refreshes %llu; last second displayed %.1f fps, "
        "1%% low %.1f",
        static_cast<unsigned long long>(s.stutters.load()), static_cast<unsigned long long>(s.stutters_race.load()),
        static_cast<unsigned long long>(s.stutters_menu.load()),
        static_cast<unsigned long long>(s.stutters_loading.load()), static_cast<unsigned long long>(s.lost.load()),
        double(s.fps_x10.load()) / 10.0, double(s.low1_x10.load()) / 10.0);
    sgn::host_profile::info("stutter", text);
}

// ---------------------------------------------------------------------------
// Writer side.
// ---------------------------------------------------------------------------
using V3 = std::array<double, 3>;
enum class Group : std::uint8_t
{
    race,
    menu,
    loading
};
constexpr const char *group_names[] = {"race", "menu", "loading"};
Group group_of(std::uint8_t scene) noexcept
{
    if (scene == scene_race)
        return Group::race;
    if (scene == scene_loading)
        return Group::loading;
    return Group::menu;
}
const char *scene_name(std::uint8_t scene) noexcept
{
    return scene < std::size(scene_names) ? scene_names[scene] : "unknown";
}
double ms(std::int64_t ns) noexcept
{
    return double(ns) / 1e6;
}
double median_of(std::vector<double> values)
{
    if (values.empty())
        return 0.0;
    const auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
    std::nth_element(values.begin(), middle, values.end());
    return *middle;
}
// A bounded window of recent values with its median.
class Window
{
    std::deque<double> values_;
    std::size_t limit_;

  public:
    explicit Window(std::size_t limit) : limit_(limit) {}
    void add(double x)
    {
        values_.push_back(x);
        if (values_.size() > limit_)
            values_.pop_front();
    }
    void clear() noexcept
    {
        values_.clear();
    }
    std::size_t size() const noexcept
    {
        return values_.size();
    }
    double median() const
    {
        return median_of(std::vector<double>(values_.begin(), values_.end()));
    }
};
// Frame-time histogram of one scene group (0.05 ms bins up to 250 ms, exact above).
struct Histogram
{
    static constexpr double bin_ms = 0.05;
    static constexpr std::size_t bins = 5000;
    std::vector<std::uint32_t> counts = std::vector<std::uint32_t>(bins);
    std::vector<double> over;
    std::uint64_t n{};
    double total_ms{}, max_ms{};
    void add(double x)
    {
        ++n;
        total_ms += x;
        max_ms = (std::max)(max_ms, x);
        const auto bin = static_cast<std::size_t>(x / bin_ms);
        if (bin < bins)
            ++counts[bin];
        else
            over.push_back(x);
    }
    double percentile(double p) const
    {
        if (!n)
            return 0.0;
        const auto rank = static_cast<std::uint64_t>(std::ceil(p * double(n)));
        std::uint64_t seen = 0;
        for (std::size_t i = 0; i < bins; ++i)
        {
            seen += counts[i];
            if (seen >= rank)
                return (double(i) + 0.5) * bin_ms;
        }
        auto sorted = over;
        std::sort(sorted.begin(), sorted.end());
        const auto index = static_cast<std::size_t>(rank - seen - (rank > seen ? 1 : 0));
        return sorted.empty() ? max_ms : sorted[(std::min)(index, sorted.size() - 1)];
    }
    // Average rate over the slowest 1/divisor of the frames (profiler definition).
    double low(std::uint64_t divisor) const
    {
        if (!n)
            return 0.0;
        std::uint64_t k = (std::max)(std::uint64_t{1}, n / divisor), taken = 0;
        double slow = 0;
        auto sorted = over;
        std::sort(sorted.rbegin(), sorted.rend());
        for (const auto x : sorted)
        {
            if (taken == k)
                break;
            slow += x;
            ++taken;
        }
        for (std::size_t i = bins; i-- > 0 && taken < k;)
        {
            const auto use = (std::min)(std::uint64_t{counts[i]}, k - taken);
            slow += double(use) * (double(i) + 0.5) * bin_ms;
            taken += use;
        }
        return slow > 0 ? 1000.0 * double(taken) / slow : 0.0;
    }
};
// Low fps of a small set of frame times (one second).
double low_of(std::vector<double> frames, std::size_t divisor)
{
    if (frames.empty())
        return 0.0;
    std::sort(frames.rbegin(), frames.rend());
    const std::size_t k = (std::max)(std::size_t{1}, frames.size() / divisor);
    double slow = 0;
    for (std::size_t i = 0; i < k; ++i)
        slow += frames[i];
    return slow > 0 ? 1000.0 * double(k) / slow : 0.0;
}

struct PassInfo
{
    std::int64_t begin{}, end{}, due{};
    std::uint32_t sched0{}, sched1{}, increment{};
    std::uint8_t scene{}, scene_end{};
    bool presenter{}, complete{}, resync{}, loading{};
    std::int64_t ticks{};
};
struct SpanInfo
{
    std::int64_t end{}, duration{};
    const char *name{};
    std::uint8_t depth{};
    std::uint32_t count{1};
};
struct ValueInfo
{
    std::int64_t t{};
    const char *name{};
};
struct Sample
{
    bool valid{}, pair{}, in_pass{}, camera{}, cut{}, view_changed{}, interpolated{};
    double at{}, state{}, alpha{}, pose_delay{}, frequency{};
    std::uint32_t tick_a{}, tick_b{};
    V3 position{};
    std::array<float, 3> forward{}, up{};
    float pair_distance{}, pair_turn{};
    const char *path{"interp"};
};
struct PresentRec
{
    std::int64_t id{-1};
    std::int64_t begin{}, end{};
    std::int32_t status{};
    std::uint8_t scene{};
    bool sync{}, tearing{}, interp{}, in_pass{};
    Sample sample;
};
struct Stutter
{
    std::int64_t t{};
    std::uint8_t scene{};
    const char *type{};
    double lost{}, ms{}, expected{}, severity{};
    std::string cause, detail, context;
    std::int64_t first_id{-1}, last_id{-1};
    std::uint32_t frames{1};
};
struct Bucket
{
    std::vector<double> frames;
    std::array<std::uint32_t, std::size(scene_names)> scenes{};
    std::uint32_t repeats{}, lost{}, stutters{}, presents{}, ticks{}, late_ticks{}, catch_ups{}, long_passes{};
    std::uint32_t drops{};
    std::uint32_t cpu_frames{};
    bool cpu_timing() const noexcept
    {
        return 2 * std::size_t{cpu_frames} > frames.size();
    }
};
struct Cause
{
    std::string key, detail;
};
struct Tally
{
    std::uint64_t count{};
    double lost{};
};

class Analyzer
{
    Config config_;
    std::FILE *trace_{}, *stutter_log_{}, *fps_log_{};
    std::uint64_t trace_bytes_{};
    std::string out_; // trace rows (flushed each drain)
    std::int64_t last_t_{};
    // Tick clock.
    double tick_ms_{1000.0 / 60.0};
    // History.
    std::deque<PassInfo> passes_;
    PassInfo open_pass_;
    bool pass_open_{};
    std::deque<SpanInfo> spans_;
    std::deque<ValueInfo> values_;
    // NEW HOST (R35) window activation (track_window).
    bool window_active_{true}, window_minimized_{};
    std::int64_t paused_since_{-1};
    std::deque<std::pair<std::int64_t, std::int64_t>> paused_;
    std::deque<std::pair<std::int64_t, std::int64_t>> tick_window_; // pass begin, ticks
    std::int64_t tick_window_start_{-1}, tick_recovered_{-1};
    bool tick_rate_low_{};
    // Presents.
    std::vector<PresentRec> presents_ = std::vector<PresentRec>(present_slots);
    Sample pending_;
    bool pending_valid_{};
    std::int64_t last_interp_id_{-1};
    std::uint8_t scene_{scene_unknown};
    // Display model (DXGI).
    struct Base
    {
        bool valid{};
        std::int64_t pc{}, prc{}, src{}, sync{};
    } base_;
    struct Anchor
    {
        bool valid{};
        std::int64_t src{}, sync{};
    } anchor_;
    double period_ns_{};
    int period_mismatch_{};
    bool base_start_known_{};
    std::int64_t base_start_{};
    std::int64_t last_finalized_{-1};
    std::int64_t last_present_id_{-1}, id_offset_{}; // DXGI PresentCount - GetLastPresentCount (expected 0)
    std::int64_t last_stats_ok_{};
    bool stats_ever_ok_{};
    std::uint64_t stats_errors_{}, stats_disjoint_{};
    std::int64_t query_total_{}, query_worst_{}, query_count_{}; // own cost: the statistics query after Present
    std::vector<double> scratch_refreshes_;
    // Detectors.
    Window interp_refreshes_{64}, interp_intervals_{64}, latency_interp_{64}, latency_source_{64};
    Window speeds_{32}, turns_{32};
    struct Previous
    {
        bool valid{};
        std::int64_t id{-1}, start{}, end{};
        bool interp{}, sync{};
        double latency{};
        Sample sample;
    } previous_;
    std::int64_t last_held_id_{-1};
    std::uint32_t last_cut_tick_{0xffffffffu};
    Stutter pending_stutter_;
    bool pending_stutter_valid_{};
    // Aggregates.
    std::map<std::int64_t, Bucket> buckets_;
    std::int64_t closed_second_{-1};
    std::array<Histogram, 3> histograms_;
    std::array<std::map<std::string, Tally>, 3> by_type_, by_cause_;
    std::array<std::uint64_t, 3> repeats_total_{}, lost_total_{};
    std::uint64_t stutter_total_{}, drops_total_{}, late_ticks_total_{}, catch_ups_total_{}, long_passes_total_{},
        resyncs_total_{}, passes_total_{}, presents_total_{};
    std::size_t lines_this_second_{};
    std::int64_t lines_second_{-1};
    std::uint64_t lines_suppressed_{};
    std::int64_t last_suppressed_{};
    bool finishing_{};
    std::vector<std::pair<std::int64_t, std::string>> lines_;

  public:
    explicit Analyzer(Config config) : config_(std::move(config)) {}
    ~Analyzer()
    {
        close_file(trace_);
        close_file(stutter_log_);
        close_file(fps_log_);
    }
    Analyzer(const Analyzer &) = delete;
    Analyzer &operator=(const Analyzer &) = delete;

    void open()
    {
        std::error_code error;
        std::filesystem::create_directories(config_.reports, error);
        stutter_log_ = open_log(config_.reports / "stutter-log.txt");
        fps_log_ = open_log(config_.reports / "fps-log.txt");
        const auto stamp = wall(config_.t0, true);
        if (stutter_log_)
            std::fprintf(stutter_log_,
                         "=== session %s: stutter detector (NEW HOST R34)%s. One line per stutter: time scene: what "
                         "- cause | key=value ===\n",
                         stamp.c_str(), config_.trace ? ", full trace on" : "");
        if (fps_log_)
            std::fprintf(fps_log_, "=== session %s: displayed frames per second (DXGI vblanks) ===\n", stamp.c_str());
        if (config_.trace)
            open_trace();
    }
    // One drained event.
    void process(const Event &e)
    {
        last_t_ = (std::max)(last_t_, e.t);
        switch (e.type)
        {
        case Type::pass_begin:
            on_pass_begin(e);
            break;
        case Type::pass_end:
            on_pass_end(e);
            break;
        case Type::span:
        case Type::span_sum:
            on_span(e);
            break;
        case Type::value:
            on_value(e);
            break;
        case Type::loading:
            if (pass_open_)
                open_pass_.loading = true;
            scene_ = scene_loading;
            if (trace_)
                row("L,%.4f\n", rel(e.t));
            break;
        case Type::interp:
            on_interp(e);
            break;
        case Type::camera:
            on_camera(e);
            break;
        case Type::present:
            on_present(e);
            break;
        case Type::stats:
            on_stats(e);
            break;
        }
    }
    void after_drain(std::uint64_t dropped)
    {
        trim(last_t_);
        if (dropped && trace_)
            row("C,%.4f,ring_dropped_total,%llu\n", rel(last_t_), static_cast<unsigned long long>(dropped));
        flush_trace();
        write_lines(last_t_ - context_ns);
        if (stutter_log_)
            std::fflush(stutter_log_);
        if (fps_log_)
            std::fflush(fps_log_);
    }
    void finish(std::uint64_t dropped)
    {
        finishing_ = true;
        flush_stutter();
        close_buckets((std::numeric_limits<std::int64_t>::max)());
        write_lines((std::numeric_limits<std::int64_t>::max)());
        summary(dropped);
        flush_trace();
    }

  private:
    // ---- output helpers ----------------------------------------------------
    static void close_file(std::FILE *&file) noexcept
    {
        if (file)
            std::fclose(file);
        file = nullptr;
    }
    static std::FILE *open_log(const std::filesystem::path &path)
    {
        std::error_code error;
        const auto size = std::filesystem::file_size(path, error);
        if (!error && size > log_rotate_bytes)
        {
            auto old = path;
            old.replace_extension(".old.txt");
            std::filesystem::rename(path, old, error);
        }
        std::FILE *file = nullptr;
        if (_wfopen_s(&file, path.c_str(), L"ab") != 0)
            return nullptr;
        return file;
    }
    void open_trace()
    {
        if (_wfopen_s(&trace_, config_.trace_path.c_str(), L"wb") != 0)
        {
            trace_ = nullptr;
            return;
        }
        trace_bytes_ = 0;
        const auto stamp = wall(config_.t0, true);
        row("# StuntGP NEW frame trace (NEW HOST R34), session %s. Times t: ms since the session start. Analyse with "
            "tools/frame_trace_analyze.py.\n",
            stamp.c_str());
        out_ += "# C,t,key,value                                        configuration / info\n"
                "# PB,t,scene,due_ms,late_ms,scheduled,presenter          source pass released (due/late empty: host "
                "cadence)\n"
                "# PE,t,scene,dur_ms,ticks,resync,scheduled,loading       source pass ended (next peek)\n"
                "# S,t_end,name,depth,dur_ms                              profiler scope\n"
                "# SS,t,name,count,total_ms                               short deep scopes summed since the last "
                "outermost scope\n"
                "# V,t,name,value_ms                                      profiler add() value or counter\n"
                "# L,t                                                    445940 loading frame (first of its pass)\n"
                "# I,t,scene,at_ms,state_ms,alpha,tick_a,tick_b,pose_delay_ms,in_pass,pair   interpolated pose sample "
                "(source counter ms)\n"
                "# K,t,px,py,pz,fx,fy,fz,ux,uy,uz,pair_dist,pair_turn_deg,cut,view_changed,interpolated   displayed "
                "camera\n"
                "# P,t_end,scene,origin,id,call_ms,status,sync,tearing,in_pass   Present call (origin interp/source)\n"
                "# D,t,present_count,present_refresh,sync_refresh,sync_ms,hr,query_us   DXGI frame statistics after "
                "it\n"
                "# F,t_disp,scene,origin,id,refreshes,ms,expected_ms,latency_ms,path,timing   displayed frame "
                "(derived)\n"
                "# A,t,kind,value,detail                                  tick/pass anomaly (late_tick, catch_up, "
                "long_pass, resync, tick_rate)\n"
                "# X,t,scene,type,lost_refreshes,ms,expected_ms,frames,cause,detail   STUTTER (derived)\n"
                "# XC,t,context                                           0.5 s before the stutter above\n"
                "# SEC,t,scene,fps,low1,low01,p99_ms,max_ms,repeats,lost,stutters,ticks,late_ticks,catch_ups,"
                "presents,drops,timing   one displayed second\n";
    }
    void row(const char *format, ...)
    {
        if (!trace_)
            return;
        char line[768];
        va_list args;
        va_start(args, format);
        const int n = std::vsnprintf(line, sizeof line, format, args);
        va_end(args);
        if (n > 0)
            out_.append(line, static_cast<std::size_t>((std::min)(n, int(sizeof line) - 1)));
    }
    void flush_trace()
    {
        if (!trace_ || out_.empty())
        {
            out_.clear();
            return;
        }
        std::fwrite(out_.data(), 1, out_.size(), trace_);
        trace_bytes_ += out_.size();
        out_.clear();
        std::fflush(trace_);
        if (config_.trace_cap && trace_bytes_ >= config_.trace_cap)
        {
            close_file(trace_);
            std::error_code error;
            auto old = config_.trace_path;
            old += ".1";
            std::filesystem::remove(old, error);
            std::filesystem::rename(config_.trace_path, old, error);
            open_trace();
        }
    }
    double rel(std::int64_t t) const noexcept
    {
        return ms(t - config_.t0);
    }
    // Local wall-clock text of an event time.
    std::string wall(std::int64_t t, bool date) const
    {
        ULARGE_INTEGER base{};
        base.LowPart = config_.wall0.dwLowDateTime;
        base.HighPart = config_.wall0.dwHighDateTime;
        base.QuadPart += static_cast<ULONGLONG>((std::max)(std::int64_t{0}, t - config_.t0) / 100);
        FILETIME utc{base.LowPart, base.HighPart}, local{};
        SYSTEMTIME st{};
        if (!FileTimeToLocalFileTime(&utc, &local) || !FileTimeToSystemTime(&local, &st))
            return "?";
        char text[48];
        if (date)
            std::snprintf(text, sizeof text, "%04u-%02u-%02u %02u:%02u:%02u.%03u", st.wYear, st.wMonth, st.wDay,
                          st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        else
            std::snprintf(text, sizeof text, "%02u:%02u:%02u.%03u", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        return text;
    }
    static std::string csv(std::string text)
    {
        std::replace(text.begin(), text.end(), ',', ';');
        std::replace(text.begin(), text.end(), '\n', ' ');
        return text;
    }
    // ---- source passes -----------------------------------------------------
    void on_pass_begin(const Event &e)
    {
        if (pass_open_)
            close_open_pass(e.t, e.scene, open_pass_.sched0); // a pass without its end (never expected)
        pass_open_ = true;
        open_pass_ = {};
        open_pass_.begin = e.t;
        open_pass_.due = e.v[0];
        open_pass_.sched0 = static_cast<std::uint32_t>(e.v[1]);
        open_pass_.increment = static_cast<std::uint32_t>(e.v[2]);
        open_pass_.scene = e.scene;
        open_pass_.presenter = (e.flags & 1u) != 0;
        scene_ = e.scene;
        if (e.v[2] > 0 && e.v[3] > 0)
            tick_ms_ = 1000.0 * double(e.v[2]) / double(e.v[3]);
        if (trace_)
        {
            if (open_pass_.due)
                row("PB,%.4f,%s,%.4f,%.4f,%u,%d\n", rel(e.t), scene_name(e.scene), rel(open_pass_.due),
                    ms(e.t - open_pass_.due), open_pass_.sched0, int(open_pass_.presenter));
            else
                row("PB,%.4f,%s,,,%u,%d\n", rel(e.t), scene_name(e.scene), open_pass_.sched0,
                    int(open_pass_.presenter));
        }
        if (open_pass_.due && e.t - open_pass_.due > 1'000'000)
        {
            ++late_ticks_total_;
            bucket(e.t).late_ticks++;
            anomaly(e.t, "late_tick", ms(e.t - open_pass_.due), "pass started after its due tick");
        }
    }
    void on_pass_end(const Event &e)
    {
        if (!pass_open_)
            return;
        close_open_pass(e.t, e.scene, static_cast<std::uint32_t>(e.v[1]));
    }
    void close_open_pass(std::int64_t end, std::uint8_t scene_end, std::uint32_t scheduled)
    {
        auto p = open_pass_;
        pass_open_ = false;
        p.end = end;
        p.sched1 = scheduled;
        p.scene_end = scene_end;
        p.complete = true;
        const std::uint32_t delta = scheduled - p.sched0;
        if (p.increment)
        {
            p.ticks = std::int64_t{delta / p.increment};
            p.resync = delta % p.increment != 0 || static_cast<std::int32_t>(delta) < 0;
        }
        passes_.push_back(p);
        ++passes_total_;
        scene_ = scene_end;
        const double duration = ms(p.end - p.begin);
        if (trace_)
            row("PE,%.4f,%s,%.4f,%lld,%d,%u,%d\n", rel(end), scene_name(scene_end), duration,
                static_cast<long long>(p.ticks), int(p.resync), scheduled, int(p.loading));
        auto &b = bucket(end);
        b.ticks += static_cast<std::uint32_t>((std::max)(std::int64_t{0}, p.ticks));
        if (p.ticks > 1 && !p.resync)
        {
            ++catch_ups_total_;
            b.catch_ups++;
            anomaly(end, "catch_up", double(p.ticks), "ticks advanced in one pass");
        }
        if (p.resync && p.due)
        {
            ++resyncs_total_;
            anomaly(end, "resync", double(delta), "clock words moved by a non-multiple of the increment");
        }
        const double budget = p.presenter ? 0.5 * tick_ms_ : tick_ms_;
        if (duration > budget && p.scene == p.scene_end && !p.loading)
        {
            ++long_passes_total_;
            b.long_passes++;
            char text[96];
            std::snprintf(text, sizeof text, "pass longer than its budget %.2f ms", budget);
            anomaly(end, "long_pass", duration, text);
        }
        tick_rate(p);
    }
    // Sliding 1 s tick rate of clock-gated passes (secondary: simulation speed).
    // Sliding 1 s tick rate of clock-gated passes (secondary: simulation speed): the ticks of
    // the passes that began within the last second. A late or long pass does not lower it (its
    // ticks still begin inside the window, a catch-up pass counts all of its ticks); only ticks
    // that the clock really did not run within that second do.
    void tick_rate(const PassInfo &p)
    {
        if (!p.due || p.resync || p.scene != p.scene_end)
        {
            tick_window_.clear();
            tick_rate_low_ = false;
            tick_window_start_ = -1;
            return;
        }
        if (tick_window_start_ < 0)
            tick_window_start_ = p.begin;
        tick_window_.emplace_back(p.begin, p.ticks);
        while (!tick_window_.empty() && tick_window_.front().first <= p.begin - second_ns)
            tick_window_.pop_front();
        if (p.begin - tick_window_start_ < second_ns)
            return;
        std::int64_t ticks = 0;
        for (const auto &[begin, count] : tick_window_)
            ticks += count;
        const double nominal = tick_ms_ > 0 ? 1000.0 / tick_ms_ : 60.0;
        const double rate = double(ticks);
        if (!tick_rate_low_ && rate < nominal - 0.5)
        {
            tick_rate_low_ = true;
            char text[160];
            std::snprintf(text, sizeof text, "%.0f ticks in the last second (nominal %.2f/s): simulation behind", rate,
                          nominal);
            anomaly(p.end, "tick_rate", rate, text);
            Stutter s;
            s.t = p.end;
            s.scene = p.scene;
            s.type = "sim";
            s.lost = 0;
            s.ms = rate;
            s.cause = "tick_rate";
            s.detail = text;
            emit_stutter(s);
        }
        else if (tick_rate_low_ && rate >= nominal - 0.2)
        {
            // One episode until the rate stayed recovered for a whole second.
            if (tick_recovered_ < 0)
                tick_recovered_ = p.begin;
            if (p.begin - tick_recovered_ >= second_ns)
                tick_rate_low_ = false;
        }
        if (rate < nominal - 0.2)
            tick_recovered_ = -1;
    }
    void anomaly(std::int64_t t, const char *kind, double value, const char *detail)
    {
        if (trace_)
            row("A,%.4f,%s,%.4f,%s\n", rel(t), kind, value, detail);
    }
    // ---- scopes and values -------------------------------------------------
    void on_span(const Event &e)
    {
        SpanInfo s{e.t, e.v[0], e.name, e.depth, 1};
        if (e.type == Type::span_sum)
        {
            s.count = static_cast<std::uint32_t>(e.v[1]);
            s.depth = 0xff;
            if (trace_)
                row("SS,%.4f,%s,%u,%.4f\n", rel(e.t), e.name ? e.name : "?", s.count, ms(s.duration));
        }
        else if (trace_)
            row("S,%.4f,%s,%u,%.4f\n", rel(e.t), e.name ? e.name : "?", unsigned(e.depth), ms(s.duration));
        spans_.push_back(s);
    }
    static const char *path_of(std::string_view name) noexcept
    {
        if (name == "interp.gpu_path_frames")
            return "gpu";
        if (name == "interp.cpu_fallback_frames")
            return "cpu_fallback";
        if (name == "interp.frame_interpolated")
            return "cpu";
        if (name == "interp.frame_cut_shown_as_B")
            return "cut_shown_as_B";
        if (name == "interp.frame_first")
            return "first";
        if (name == "interp.frame_at_N")
            return "at_N";
        if (name == "interp.frame_at_pair_end")
            return "at_pair_end";
        if (name == "interp.drop_rejected_interpolated_frame")
            return "rejected_shown_as_B";
        return nullptr;
    }
    // NEW HOST (R35): the window's activation state (WM_ACTIVATEAPP, 44E4B0 minimize/restore), from the
    // host.window_* values of the window hooks. While the window is inactive or minimized the source pauses
    // its frames (44EB09/44EB12: no update, no draw, no Present), as the original game does.
    void track_window(const Event &e)
    {
        const std::string_view name(e.name);
        const bool was_paused = !window_active_ || window_minimized_;
        if (name == "host.window_activated")
            window_active_ = true;
        else if (name == "host.window_deactivated")
            window_active_ = false;
        else if (name == "host.window_minimized")
            window_minimized_ = true;
        else if (name == "host.window_restored")
            window_minimized_ = false;
        else
            return;
        const bool paused = !window_active_ || window_minimized_;
        if (paused && !was_paused)
            paused_since_ = e.t;
        else if (!paused && was_paused && paused_since_ >= 0)
        {
            paused_.emplace_back(paused_since_, e.t);
            if (paused_.size() > 64)
                paused_.pop_front();
            paused_since_ = -1;
        }
    }
    // Time of (a, b] during which the window was inactive or minimized.
    std::int64_t paused_within(std::int64_t a, std::int64_t b) const noexcept
    {
        std::int64_t total = paused_since_ >= 0 ? overlap(paused_since_, b, a, b) : 0;
        for (const auto &[from, to] : paused_)
            total += overlap(from, to, a, b);
        return total;
    }
    void on_value(const Event &e)
    {
        if (trace_)
            row("V,%.4f,%s,%.4f\n", rel(e.t), e.name ? e.name : "?", ms(e.v[0]));
        if (!e.name)
            return;
        track_window(e); // NEW HOST (R35)
        values_.push_back({e.t, e.name});
        if (const char *path = path_of(e.name))
        {
            // Before its Present (pending sample) or after it (gpu / cpu fallback / rejected).
            if (pending_valid_)
                pending_.path = path;
            else if (last_interp_id_ >= 0)
                if (auto *p = find(last_interp_id_))
                    p->sample.path = path;
        }
    }
    // ---- presenter samples -------------------------------------------------
    void on_interp(const Event &e)
    {
        pending_ = {};
        pending_valid_ = true;
        last_interp_id_ = -1;
        auto &s = pending_;
        s.valid = true;
        s.at = unpack(e.v[0]);
        s.state = unpack(e.v[1]);
        s.alpha = unpack(e.v[2]);
        s.tick_a = static_cast<std::uint32_t>(static_cast<std::uint64_t>(e.v[3]) >> 32);
        s.tick_b = static_cast<std::uint32_t>(static_cast<std::uint64_t>(e.v[3]));
        s.pose_delay = unpack(e.v[4]);
        s.frequency = unpack(e.v[5]);
        s.in_pass = (e.flags & 1u) != 0;
        s.pair = (e.flags & 2u) != 0;
        if (trace_)
        {
            const double k = s.frequency > 0 ? 1000.0 / s.frequency : 0.0;
            row("I,%.4f,%s,%.4f,%.4f,%.4f,%u,%u,%.4f,%d,%d\n", rel(e.t), scene_name(e.scene), s.at * k, s.state * k,
                s.alpha, s.tick_a, s.tick_b, s.pose_delay * 1000.0, int(s.in_pass), int(s.pair));
        }
    }
    void on_camera(const Event &e)
    {
        if (!pending_valid_)
            return;
        auto &s = pending_;
        s.camera = true;
        s.position = {unpack(e.v[0]), unpack(e.v[1]), unpack(e.v[2])};
        s.forward = {unpack_low(e.v[3]), unpack_high(e.v[3]), unpack_low(e.v[4])};
        s.up = {unpack_high(e.v[4]), unpack_low(e.v[5]), unpack_high(e.v[5])};
        s.pair_distance = unpack_low(e.v[6]);
        s.pair_turn = unpack_high(e.v[6]);
        s.cut = (e.flags & camera_cut) != 0;
        s.view_changed = (e.flags & camera_view_changed) != 0;
        s.interpolated = (e.flags & camera_interpolated) != 0;
        if (trace_)
            row("K,%.3f,%.3f,%.3f,%.3f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.3f,%.4f,%d,%d,%d\n", rel(e.t), s.position[0],
                s.position[1], s.position[2], double(s.forward[0]), double(s.forward[1]), double(s.forward[2]),
                double(s.up[0]), double(s.up[1]), double(s.up[2]), double(s.pair_distance),
                double(s.pair_turn) * 57.29577951308232, int(s.cut), int(s.view_changed),
                int((e.flags & camera_interpolated) != 0));
    }
    // ---- presents and the display model -------------------------------------
    PresentRec *find(std::int64_t id)
    {
        if (id < 0)
            return nullptr;
        auto &p = presents_[static_cast<std::size_t>(id) & (present_slots - 1)];
        return p.id == id ? &p : nullptr;
    }
    void on_present(const Event &e)
    {
        const std::int64_t id = e.v[1];
        if (id >= 0)
            last_present_id_ = id;
        PresentRec rec;
        rec.id = id;
        rec.begin = e.v[0];
        rec.end = e.t;
        rec.status = static_cast<std::int32_t>(e.v[2]);
        rec.scene = e.scene;
        rec.sync = (e.flags & present_sync) != 0;
        rec.tearing = (e.flags & present_tearing) != 0;
        rec.interp = (e.flags & present_interp) != 0;
        rec.in_pass = (e.flags & present_in_pass) != 0;
        if (rec.interp && pending_valid_)
            rec.sample = pending_;
        pending_valid_ = false;
        last_interp_id_ = rec.interp ? id : -1;
        ++presents_total_;
        bucket(e.t).presents++;
        if (trace_)
            row("P,%.4f,%s,%s,%lld,%.4f,0x%08x,%d,%d,%d\n", rel(e.t), scene_name(e.scene),
                rec.interp ? "interp" : "source", static_cast<long long>(id), ms(e.t - rec.begin),
                static_cast<unsigned>(rec.status), int(rec.sync), int(rec.tearing), int(rec.in_pass));
        if (id >= 0)
            presents_[static_cast<std::size_t>(id) & (present_slots - 1)] = rec;
        // Without DXGI statistics for a while, displayed frames follow the Present calls.
        if (id >= 0 &&
            ((!stats_ever_ok_ && e.t - config_.t0 > second_ns) || (stats_ever_ok_ && e.t - last_stats_ok_ > second_ns)))
            cpu_timed_frame(id);
    }
    void cpu_timed_frame(std::int64_t id)
    {
        const auto *next = find(id);
        const auto *rec = find(id - 1);
        if (!rec || !next || id - 1 <= last_finalized_)
            return;
        const double refreshes = period_ns_ > 0 ? double(next->end - rec->end) / period_ns_ : 1.0;
        finalize(id - 1, rec->end, refreshes, next->end, true, true);
    }
    void update_period(std::int64_t src, std::int64_t sync)
    {
        if (!anchor_.valid || src < anchor_.src || sync <= anchor_.sync)
        {
            anchor_ = {true, src, sync};
            return;
        }
        const std::int64_t refreshes = src - anchor_.src;
        if (refreshes < 30)
            return;
        const double p = double(sync - anchor_.sync) / double(refreshes);
        if (!(p > 1e6 && p < 1e8)) // 10..1000 Hz
        {
            anchor_ = {true, src, sync};
            return;
        }
        if (period_ns_ <= 0 || std::abs(p - period_ns_) < 0.02 * period_ns_)
        {
            period_ns_ = p;
            period_mismatch_ = 0;
        }
        else if (++period_mismatch_ >= 3) // a new display mode
        {
            period_ns_ = p;
            period_mismatch_ = 0;
            anchor_ = {true, src, sync};
            if (trace_)
                row("C,%.4f,refresh_period_ms,%.5f\n", rel(sync), p / 1e6);
            return;
        }
        if (refreshes >= 2400) // re-anchor about every 10 s
            anchor_ = {true, src, sync};
    }
    void on_stats(const Event &e)
    {
        const auto hr = static_cast<HRESULT>(e.v[4]);
        if (trace_)
            row("D,%.4f,%lld,%lld,%lld,%.4f,0x%08x,%.1f\n", rel(e.t), static_cast<long long>(e.v[0]),
                static_cast<long long>(e.v[1]), static_cast<long long>(e.v[2]), e.v[3] ? rel(e.v[3]) : 0.0,
                static_cast<unsigned>(hr), double(e.v[5]) / 1000.0);
        query_total_ += e.v[5];
        query_worst_ = (std::max)(query_worst_, e.v[5]);
        ++query_count_;
        if (FAILED(hr) || !e.v[3])
        {
            if (hr == DXGI_ERROR_FRAME_STATISTICS_DISJOINT)
                ++stats_disjoint_;
            else
                ++stats_errors_;
            base_.valid = false;
            base_start_known_ = false;
            return;
        }
        const std::int64_t prc = e.v[1], src = e.v[2], sync = e.v[3];
        if (e.v[0] - id_offset_ > last_present_id_ && last_present_id_ >= 0)
        {
            id_offset_ = e.v[0] - last_present_id_;
            if (trace_)
                row("C,%.4f,present_count_offset,%lld\n", rel(e.t), static_cast<long long>(id_offset_));
            base_.valid = false;
        }
        const std::int64_t pc = e.v[0] - id_offset_;
        stats_ever_ok_ = true;
        last_stats_ok_ = e.t;
        update_period(src, sync);
        if (!base_.valid || pc < base_.pc || prc < base_.prc)
        {
            base_ = {true, pc, prc, src, sync};
            base_start_known_ = period_ns_ > 0;
            base_start_ = sync - std::llround(double(src - prc) * period_ns_);
            return;
        }
        if (pc == base_.pc)
            return; // no new frame reached the screen since the last sample
        if (period_ns_ <= 0)
        {
            base_ = {true, pc, prc, src, sync};
            base_start_known_ = false;
            return;
        }
        const std::int64_t dp = pc - base_.pc, dr = prc - base_.prc;
        const std::int64_t start_pc = sync - std::llround(double(src - prc) * period_ns_);
        distribute(base_.pc, dp, dr);
        std::int64_t start = base_start_known_ ? base_start_ : start_pc - std::llround(double(dr) * period_ns_);
        for (std::int64_t k = 0; k < dp; ++k)
        {
            const std::int64_t id = base_.pc + k;
            const double r = scratch_refreshes_[static_cast<std::size_t>(k)];
            const std::int64_t next_start = k == dp - 1 ? start_pc : start + std::llround(r * period_ns_);
            if (id > last_finalized_)
                finalize(id, start, r, next_start, base_start_known_ || k > 0, false);
            start = next_start;
        }
        base_ = {true, pc, prc, src, sync};
        base_start_ = start_pc;
        base_start_known_ = true;
        if (dr < dp)
        {
            drops_total_ += static_cast<std::uint64_t>(dp - dr);
            bucket(start_pc).drops += static_cast<std::uint32_t>(dp - dr);
        }
    }
    // Refreshes of frames first..first+dp-1 (together dr). One frame: exact. Several
    // frames between two samples: each at least one refresh, the rest follows their
    // Present call intervals (the frame whose successor was presented latest gets it).
    void distribute(std::int64_t first, std::int64_t dp, std::int64_t dr)
    {
        scratch_refreshes_.assign(static_cast<std::size_t>(dp), 1.0);
        if (dp == 1)
        {
            scratch_refreshes_[0] = double(dr);
            return;
        }
        if (dr < dp) // more frames than refreshes: vsync off (tearing) or dropped frames
        {
            for (auto &r : scratch_refreshes_)
                r = double(dr) / double(dp);
            return;
        }
        std::vector<double> estimate(static_cast<std::size_t>(dp), 1.0);
        double total = 0;
        for (std::int64_t k = 0; k < dp; ++k)
        {
            const auto *a = find(first + k);
            const auto *b = find(first + k + 1);
            double e = 1.0;
            if (a && b && period_ns_ > 0)
                e = (std::max)(1.0, std::round(double(b->end - a->end) / period_ns_));
            estimate[static_cast<std::size_t>(k)] = e;
            total += e;
        }
        double remaining = double(dr) - total;
        while (std::abs(remaining) >= 0.5)
        {
            auto it = std::max_element(estimate.begin(), estimate.end());
            if (remaining > 0)
            {
                *it += remaining;
                remaining = 0;
            }
            else
            {
                if (*it <= 1.0)
                    break;
                const double take = (std::min)(*it - 1.0, -remaining);
                *it -= take;
                remaining += take;
            }
        }
        scratch_refreshes_ = estimate;
    }
    double period_ms() const noexcept
    {
        return period_ns_ > 0 ? period_ns_ / 1e6 : 1000.0 / 60.0;
    }
    double expected_ms(const PresentRec *rec) const
    {
        if (!rec || !rec->interp)
            return tick_ms_; // source frames: one per 60 Hz tick (menus, loading, race without interpolation)
        if (rec->sync && period_ns_ > 0)
        {
            const double refreshes = interp_refreshes_.size() >= 8 ? interp_refreshes_.median() : 1.0;
            return period_ms() * (std::max)(1.0, std::round(refreshes));
        }
        const double interval = interp_intervals_.size() >= 8 ? interp_intervals_.median() : period_ms();
        return std::clamp(interval, 0.5, tick_ms_);
    }
    // One displayed frame whose display duration is known.
    void finalize(std::int64_t id, std::int64_t start, double refreshes, std::int64_t next_start, bool known,
                  bool cpu_timing)
    {
        last_finalized_ = (std::max)(last_finalized_, id);
        if (!known)
        {
            previous_.valid = false;
            return;
        }
        const auto *rec = find(id);
        const auto *next = find(id + 1);
        const std::uint8_t scene = rec ? rec->scene : scene_;
        const auto group = static_cast<std::size_t>(group_of(scene));
        const bool vsync = rec && rec->sync && !cpu_timing;
        double frame_ms = ms(next_start - start);
        if (rec && next && (!vsync))
            frame_ms = ms(next->end - rec->end);
        if (refreshes <= 0.0 && !cpu_timing)
        {
            // Replaced before any refresh showed it (vsync off): not a displayed frame.
            previous_.valid = false;
            return;
        }
        const double expected = expected_ms(rec);
        const double period = period_ms();
        const double latency = rec && vsync ? ms(start - rec->end) : 0.0;
        const char *path = rec && rec->interp ? rec->sample.path : "source";
        if (trace_)
            row("F,%.4f,%s,%s,%lld,%.2f,%.4f,%.4f,%.4f,%s,%s\n", rel(start), scene_name(scene),
                rec && rec->interp ? "interp" : "source", static_cast<long long>(id), refreshes, frame_ms, expected,
                latency, path, cpu_timing ? "cpu" : "dxgi");
        histograms_[group].add(frame_ms);
        auto &b = bucket(start);
        b.frames.push_back(frame_ms);
        b.scenes[scene < b.scenes.size() ? scene : 0]++;
        b.cpu_frames += cpu_timing ? 1u : 0u;
        const double expected_refreshes = (std::max)(1.0, std::ceil(expected / period - 0.01));
        if (vsync)
        {
            const auto repeats = static_cast<std::uint32_t>((std::max)(0.0, std::round(refreshes) - 1.0));
            const auto lost = static_cast<std::uint32_t>((std::max)(0.0, std::round(refreshes - expected_refreshes)));
            b.repeats += repeats;
            b.lost += lost;
            repeats_total_[group] += repeats;
            lost_total_[group] += lost;
        }
        // HELD: this frame stayed on screen longer than 1.5x the expected period.
        bool held = false;
        if (frame_ms > 1.5 * expected + 0.05 && (!vsync || refreshes >= expected_refreshes + 1.0 - 1e-6))
        {
            held = true;
            last_held_id_ = id;
            Stutter s;
            s.t = next_start;
            s.scene = scene;
            s.type = "held";
            s.lost = vsync ? (std::max)(1.0, std::round(refreshes - expected_refreshes))
                           : (std::max)(1.0, std::round(frame_ms / expected) - 1.0);
            s.ms = frame_ms;
            s.expected = expected;
            s.severity = frame_ms / expected;
            s.first_id = s.last_id = id;
            const auto cause = gap_cause(rec, next, expected);
            s.cause = cause.key;
            s.detail = cause.detail;
            add_stutter(s, next ? next->end : next_start);
        }
        // LATE: shown a refresh or more after its usual present-to-display latency.
        auto &latencies = rec && rec->interp ? latency_interp_ : latency_source_;
        const bool latency_step =
            previous_.valid && previous_.id == id - 1 && previous_.sync && latency - previous_.latency >= 0.75 * period;
        if (vsync && latencies.size() >= 16 && !held && last_held_id_ != id - 1 && latency_step)
        {
            const double usual = latencies.median();
            if (latency - usual >= 0.75 * period)
            {
                Stutter s;
                s.t = start;
                s.scene = scene;
                s.type = "late";
                s.lost = (std::max)(1.0, std::round((latency - usual) / period));
                s.ms = latency;
                s.expected = usual;
                s.severity = (latency - usual) / period;
                s.first_id = s.last_id = id;
                const auto *previous = find(id - 1);
                const auto cause = gap_cause(previous, rec, expected);
                s.cause = cause.key;
                s.detail = "shown " + fixed(latency - usual, 2) + " ms after its usual latency " + fixed(usual, 2) +
                           " ms; " + cause.detail;
                add_stutter(s, rec->end);
            }
        }
        if (vsync)
            latencies.add(latency);
        motion_checks(rec, id, start, vsync, expected);
        if (rec && rec->interp)
        {
            if (vsync)
                interp_refreshes_.add(std::round(refreshes));
            else if (next)
                interp_intervals_.add(ms(next->end - rec->end));
        }
        previous_.valid = rec != nullptr;
        previous_.id = id;
        previous_.start = start;
        previous_.end = rec ? rec->end : 0;
        previous_.interp = rec && rec->interp;
        previous_.sync = vsync;
        previous_.latency = latency;
        if (rec)
            previous_.sample = rec->sample;
        close_buckets(start);
    }
    static double angle_deg(const std::array<float, 3> &a, const std::array<float, 3> &b) noexcept
    {
        const double d = double(a[0]) * b[0] + double(a[1]) * b[1] + double(a[2]) * b[2];
        const double la = std::sqrt(double(a[0]) * a[0] + double(a[1]) * a[1] + double(a[2]) * a[2]);
        const double lb = std::sqrt(double(b[0]) * b[0] + double(b[1]) * b[1] + double(b[2]) * b[2]);
        if (!(la > 1e-9 && lb > 1e-9))
            return 0.0;
        return std::acos(std::clamp(d / (la * lb), -1.0, 1.0)) * 57.29577951308232;
    }
    // ANIMATION TIME, MOTION, TURN and CUT checks of consecutive displayed interpolated frames.
    void motion_checks(const PresentRec *rec, std::int64_t id, std::int64_t start, bool vsync, double expected)
    {
        if (!rec || !rec->interp || !rec->sample.valid)
        {
            speeds_.clear();
            turns_.clear();
            return;
        }
        const auto &s = rec->sample;
        if (s.camera && s.cut && s.tick_b != last_cut_tick_)
        {
            last_cut_tick_ = s.tick_b;
            Stutter st;
            st.t = start;
            st.scene = rec->scene;
            st.type = "cut";
            st.ms = double(s.pair_distance);
            st.severity = double(s.pair_distance);
            st.first_id = st.last_id = id;
            st.cause = s.view_changed ? "camera_view_change" : "camera_cut";
            st.detail = "presenter cut rule: camera moved " + fixed(double(s.pair_distance), 1) + " units, turned " +
                        fixed(double(s.pair_turn) * 57.29577951308232, 1) + " deg between ticks " +
                        std::to_string(s.tick_a) + " and " + std::to_string(s.tick_b) +
                        (s.view_changed ? " (controller/camera mode changed)" : "") + "; frame N shown as is";
            add_stutter(st, rec->end);
            speeds_.clear();
            turns_.clear();
        }
        if (!previous_.valid || previous_.id != id - 1 || !previous_.interp || !previous_.sample.valid ||
            s.frequency <= 0)
            return;
        const auto &p = previous_.sample;
        const double period = period_ms();
        const double dstate = (s.state - p.state) * 1000.0 / s.frequency;
        const double ddisp = vsync ? ms(start - previous_.start) : ms(rec->end - previous_.end);
        const double tolerance = vsync ? 0.5 * period : (std::max)(1.0, 0.5 * expected);
        if (dstate < -0.01 || std::abs(dstate - ddisp) > tolerance)
        {
            Stutter st;
            st.t = start;
            st.scene = rec->scene;
            st.type = "anim_time";
            st.lost = std::round(std::abs(dstate - ddisp) / period);
            st.ms = dstate;
            st.expected = ddisp;
            st.severity = std::abs(dstate - ddisp) / period;
            st.first_id = st.last_id = id;
            const auto cause = pose_cause(*rec, p, dstate, ddisp);
            st.cause = cause.key;
            st.detail = (dstate < -0.01 ? "presented state time went BACK " + fixed(-dstate, 2) + " ms"
                                        : "presented state advanced " + fixed(dstate, 2) +
                                              " ms while the display "
                                              "advanced " +
                                              fixed(ddisp, 2) + " ms") +
                        "; " + cause.detail;
            add_stutter(st, rec->end);
        }
        if (!s.camera || !p.camera || s.cut || p.cut || dstate <= 0.05)
            return;
        const double d = std::sqrt((s.position[0] - p.position[0]) * (s.position[0] - p.position[0]) +
                                   (s.position[1] - p.position[1]) * (s.position[1] - p.position[1]) +
                                   (s.position[2] - p.position[2]) * (s.position[2] - p.position[2]));
        const double turn = (std::max)(angle_deg(s.forward, p.forward), angle_deg(s.up, p.up));
        check_rate(speeds_, d, dstate, 4.0, linear(s, p, dstate, false), "motion", " units", *rec, p, id, start);
        check_rate(turns_, turn, dstate, 0.25, linear(s, p, dstate, true), "turn", " deg", *rec, p, id, start);
    }
    // The displacement (or turn) the linear interpolation of the source itself gives between two
    // interpolated frames: the pair's own speed, split at the pair change; -1 when a frame is not
    // interpolated (alpha 1, first, cut), i.e. its pose left the source's linear path.
    double linear(const Sample &s, const Sample &p, double dstate, bool rotation) const
    {
        if (!s.interpolated || !p.interpolated || !s.pair || !p.pair || tick_ms_ <= 0)
            return -1.0;
        const auto speed = [&](const Sample &x) {
            const double span = double((std::max)(1u, x.tick_b - x.tick_a)) * tick_ms_;
            return double(rotation ? x.pair_turn * 57.29577951308232f : x.pair_distance) / span;
        };
        if (s.tick_a == p.tick_a && s.tick_b == p.tick_b)
            return speed(s) * dstate;
        const double into_new =
            std::clamp(s.alpha * double((std::max)(1u, s.tick_b - s.tick_a)) * tick_ms_, 0.0, dstate);
        return speed(p) * (dstate - into_new) + speed(s) * into_new;
    }
    // Irregular against the local median at the current speed (1.5x / 0.5x). When the shown pose
    // is exactly the source's own linear path (the source camera accelerated or jumped, e.g. a
    // crash), only a larger deviation counts (2.5x / 0.4x) and the cause is the source motion.
    void check_rate(Window &window, double amount, double dstate, double floor, double linear, const char *type,
                    const char *unit, const PresentRec &rec, const Sample &p, std::int64_t id, std::int64_t start)
    {
        const double rate = amount / dstate;
        if (window.size() >= 8)
        {
            const double expected = window.median() * dstate;
            const bool source_path =
                linear >= 0 && std::abs(amount - linear) <= 0.25 * (std::max)(amount, linear) + 0.5 * floor;
            const double high = source_path ? 2.5 : 1.5, low = source_path ? 0.4 : 0.5;
            const bool jump = amount > high * expected && amount - expected > floor;
            const bool stall = expected > floor && amount < low * expected;
            if (jump || stall)
            {
                Stutter st;
                st.t = start;
                st.scene = rec.scene;
                st.type = type;
                st.ms = amount;
                st.expected = expected;
                st.severity = expected > 0 ? amount / expected : 0.0;
                st.first_id = st.last_id = id;
                auto cause = pose_cause(rec, p, dstate, dstate);
                if (source_path)
                    cause = {"source_motion", "the shown pose follows the source's own path linearly: the source "
                                              "camera itself " +
                                                  std::string(jump ? "jumped/accelerated" : "stopped/decelerated") +
                                                  " between ticks " + std::to_string(p.tick_b) + " and " +
                                                  std::to_string(rec.sample.tick_b)};
                else if (cause.key == "source_motion")
                    cause = {"presenter_pose", "the shown pose left the source's linear path (alpha " +
                                                   fixed(rec.sample.alpha, 3) + ", path " +
                                                   std::string(rec.sample.path ? rec.sample.path : "?") + ")"};
                st.cause = cause.key;
                st.detail = std::string(jump ? "jump: " : "stall: ") + fixed(amount, 2) + unit + " in one displayed " +
                            "frame, expected " + fixed(expected, 2) + unit + " at the current speed; " + cause.detail;
                add_stutter(st, rec.end);
            }
        }
        window.add(rate);
    }
    // Likely cause of a visible pose irregularity between two displayed frames.
    Cause pose_cause(const PresentRec &rec, const Sample &p, double dstate, double ddisp)
    {
        const auto &s = rec.sample;
        const std::string_view path = s.path ? s.path : "";
        const std::string_view previous_path = p.path ? p.path : "";
        if (path == "first" || previous_path == "first")
            return {"interp_restart", "interpolation (re)started without a pair: pose snapped to frame N"};
        if (path == "at_N" || path == "at_pair_end" || previous_path == "at_N")
            return {"waiting_for_tick_frame",
                    "presenter at alpha 1 (path " + std::string(path) +
                        "): the canonical frame of the next tick was not ready (source pass late)"};
        if (path == "cut_shown_as_B" || path == "rejected_shown_as_B")
            return {"shown_as_B", "path " + std::string(path) + ": frame N shown without interpolation"};
        if (s.tick_b - s.tick_a > 1 && s.pair)
            return {"pair_spans_ticks", "pair N-1/N spans " + std::to_string(s.tick_b - s.tick_a) +
                                            " source ticks (catch-up: two ticks in one pass)"};
        if (s.tick_b != p.tick_b && s.tick_a != p.tick_b && p.pair && s.pair)
            return {"pair_skipped", "pair jumped from ticks " + std::to_string(p.tick_a) + "/" +
                                        std::to_string(p.tick_b) + " to " + std::to_string(s.tick_a) + "/" +
                                        std::to_string(s.tick_b) + " (a canonical frame was never paired)"};
        if (std::abs(dstate - ddisp) > 0.5 * period_ms())
            return {"pose_time_mismatch", "pose sampled for another refresh than the one it was shown on (pose "
                                          "delay " +
                                              fixed(s.pose_delay * 1000.0, 2) + " ms, in-pass " +
                                              std::to_string(int(s.in_pass)) + ")"};
        if (path == "cpu_fallback")
            return {"cpu_fallback", "GPU re-projection unavailable: CPU build"};
        return {"source_motion", "the source camera itself moved irregularly between ticks " +
                                     std::to_string(s.tick_a) + " and " + std::to_string(s.tick_b)};
    }
    static std::string fixed(double x, int digits)
    {
        char text[48];
        std::snprintf(text, sizeof text, "%.*f", digits, x);
        return text;
    }
    // Overlap of [a0,a1] and [b0,b1] in ns.
    static std::int64_t overlap(std::int64_t a0, std::int64_t a1, std::int64_t b0, std::int64_t b1) noexcept
    {
        return (std::max)(std::int64_t{0}, (std::min)(a1, b1) - (std::max)(a0, b0));
    }
    // The spans inside (a, b] summed by name, longest first.
    std::vector<std::pair<const char *, std::int64_t>> top_spans(std::int64_t a, std::int64_t b, std::size_t count,
                                                                 bool waits_only = false) const
    {
        std::vector<std::pair<const char *, std::int64_t>> totals;
        for (auto it = spans_.rbegin(); it != spans_.rend(); ++it)
        {
            if (it->end <= a)
                break;
            if (!it->name)
                continue;
            const auto amount =
                it->depth == 0xff ? (it->end > b ? 0 : it->duration) : overlap(it->end - it->duration, it->end, a, b);
            if (amount <= 0)
                continue;
            if (waits_only && !is_wait(it->name))
                continue;
            auto found = std::find_if(totals.begin(), totals.end(), [&](const auto &x) { return x.first == it->name; });
            if (found == totals.end())
                totals.emplace_back(it->name, amount);
            else
                found->second += amount;
        }
        std::sort(totals.begin(), totals.end(), [](const auto &x, const auto &y) { return x.second > y.second; });
        if (totals.size() > count)
            totals.resize(count);
        return totals;
    }
    static bool is_wait(std::string_view name) noexcept
    {
        return name == "host.pacing_wait_for_tick" || name == "host.interp_wait_display_slot" ||
               name == "host.frame_latency_wait" || name == "host.frame_limiter_wait";
    }
    static bool starts_with(const char *name, std::string_view prefix) noexcept
    {
        return name && std::string_view(name).substr(0, prefix.size()) == prefix;
    }
    std::string list(const std::vector<std::pair<const char *, std::int64_t>> &items) const
    {
        std::string text;
        for (const auto &[name, amount] : items)
        {
            if (!text.empty())
                text += ", ";
            text += std::string(name) + " " + fixed(ms(amount), 2);
        }
        return text;
    }
    // Likely cause of a late next frame: what the game thread did between the
    // Present of the held frame (a) and the Present of the next one (b).
    Cause gap_cause(const PresentRec *rec, const PresentRec *next, double expected)
    {
        if (!rec || !next)
            return {"unknown", "no Present record (before the session or evicted)"};
        const std::int64_t a = rec->end, b = next->end;
        const double gap = ms(b - a);
        const double call = ms(next->end - next->begin);
        std::string extra;
        for (auto it = values_.rbegin(); it != values_.rend(); ++it)
        {
            if (it->t <= a - context_ns / 5)
                break;
            if (it->t > b)
                continue;
            if (starts_with(it->name, "interp.reject") || starts_with(it->name, "interp.hold") ||
                starts_with(it->name, "interp.drop") || starts_with(it->name, "interp.reject_failed"))
            {
                if (extra.find(it->name) == std::string::npos)
                    extra += std::string(extra.empty() ? "" : ", ") + it->name;
            }
        }
        const auto with_extra = [&](Cause c) {
            if (!extra.empty())
                c.detail += " [" + extra + "]";
            return c;
        };
        if (rec->scene != next->scene)
            return with_extra({"scene_transition", std::string("scene ") + scene_name(rec->scene) + " -> " +
                                                       scene_name(next->scene) + ", no frame for " + fixed(gap, 1) +
                                                       " ms"});
        const PassInfo *longest = nullptr;
        std::int64_t best = 0;
        const auto consider = [&](const PassInfo &p) {
            const auto o = overlap(p.begin, p.complete ? p.end : last_t_, a, b);
            if (o > best)
            {
                best = o;
                longest = &p;
            }
        };
        for (auto it = passes_.rbegin(); it != passes_.rend(); ++it)
        {
            if ((it->complete ? it->end : last_t_) <= a)
                break;
            consider(*it);
        }
        if (pass_open_)
            consider(open_pass_);
        if (longest && (longest->scene != longest->scene_end && longest->complete))
            return with_extra({"scene_transition", "source pass " + fixed(ms(longest->end - longest->begin), 1) +
                                                       " ms changed scene " + scene_name(longest->scene) + " -> " +
                                                       scene_name(longest->scene_end)});
        if (next->scene == scene_loading || (longest && longest->loading))
            return with_extra({"loading", "loading screen frame (445940), " + fixed(gap, 1) + " ms between frames"});
        if (!rec->interp && next->interp)
            return with_extra({"interp_started", "interpolation (re)activated after source frames"});
        if (rec->interp && !next->interp)
            return with_extra({"interp_stopped", "interpolation inactive: back to the source 60 Hz frames"});
        // NEW HOST (R35): a gap spent mostly with the window inactive or minimized is the source's own pause
        // (the original game stops updating and drawing while it is not the active application).
        if (const auto paused = paused_within(a, b); paused > 0 && double(paused) >= 0.5 * double(b - a))
            return with_extra({"window_inactive", "the game window was inactive or minimized for " +
                                                      fixed(ms(paused), 1) + " ms of the " + fixed(gap, 1) +
                                                      " ms gap (WM_ACTIVATEAPP / minimize): the source pauses its "
                                                      "frames then, as the original does"});
        if (gap <= 1.25 * expected + 0.5)
        {
            const double lat_next = ms(next->end - next->begin);
            return with_extra(
                {"display_late", "both Presents on time (" + fixed(gap, 2) + " ms apart, call " + fixed(lat_next, 2) +
                                     " ms), the new frame reached the screen late: GPU work, compositor or the "
                                     "present queue"});
        }
        if (call >= 0.4 * gap)
            return with_extra({"present_blocked", "the Present call itself blocked " + fixed(call, 2) + " ms (gap " +
                                                      fixed(gap, 2) + " ms)"});
        if (longest && best >= static_cast<std::int64_t>(0.4 * double(b - a)))
        {
            const auto &p = *longest;
            const auto end = p.complete ? p.end : last_t_;
            std::string detail = "source pass " + fixed(ms(end - p.begin), 2) + " ms";
            if (p.ticks > 1)
                detail += ", " + std::to_string(p.ticks) + " ticks (catch-up)";
            if (p.due)
                detail += ", started " + fixed(ms(p.begin - p.due), 2) + " ms after its due tick";
            const auto top = top_spans(p.begin, end, 3);
            if (!top.empty())
                detail += ": " + list(top);
            detail += "; no present for " + fixed(gap, 2) + " ms";
            // NEW HOST (R35): a pass spent mostly in the source's own Sleep (458DE9, 500 ms after a memory-card
            // operation such as the startup profile load) is the original game's pause, not host work.
            // (The value is recorded in every mode; the scope of the same name only with SGN_TRACE.)
            for (auto it = values_.rbegin(); it != values_.rend() && it->t >= p.begin; ++it)
                if (it->t <= end && starts_with(it->name, "source.sleep_"))
                    return with_extra({"source_sleep", detail + " (the source's own Sleep, as the original)"});
            return with_extra({p.ticks > 1 ? "catch_up" : (p.resync ? "resync" : "long_pass"), detail});
        }
        const auto waits = top_spans(a, b, 2, true);
        if (!waits.empty() && double(waits.front().second) >= 0.4 * double(b - a))
            return with_extra({"pacing_wait", "pacing waited: " + list(waits) + " (gap " + fixed(gap, 2) + " ms)"});
        const auto top = top_spans(a, b, 3);
        if (!top.empty() && double(top.front().second) >= 0.3 * double(b - a))
        {
            const char *name = top.front().first;
            const char *key = starts_with(name, "swap.") || starts_with(name, "gpu.upload") ? "texture_upload"
                              : starts_with(name, "host.interp")                            ? "interp_build"
                              : starts_with(name, "gpu.")                                   ? "renderer_cpu"
                                                                                            : "stage";
            return with_extra({key, list(top) + " (gap " + fixed(gap, 2) + " ms)"});
        }
        return with_extra({"untraced", "no traced work covers most of the " + fixed(gap, 2) +
                                           " ms gap (OS scheduling, window messages, driver)" +
                                           (top.empty() ? std::string() : "; traced: " + list(top))});
    }
    std::string context(std::int64_t t) const
    {
        const std::int64_t a = t - context_ns;
        std::size_t passes = 0, catch_ups = 0, late = 0, loading = 0;
        double longest = 0;
        for (auto it = passes_.rbegin(); it != passes_.rend(); ++it)
        {
            if (it->end <= a)
                break;
            if (it->begin > t)
                continue;
            ++passes;
            longest = (std::max)(longest, ms(it->end - it->begin));
            catch_ups += it->ticks > 1;
            late += it->due && it->begin - it->due > 1'000'000;
            loading += it->loading;
        }
        std::map<std::string_view, int> paths;
        std::size_t presents = 0, in_pass = 0;
        double worst_call = 0;
        for (const auto &p : presents_)
        {
            if (p.id < 0 || p.end <= a || p.end > t)
                continue;
            ++presents;
            in_pass += p.in_pass;
            worst_call = (std::max)(worst_call, ms(p.end - p.begin));
            paths[p.interp ? std::string_view(p.sample.path ? p.sample.path : "?") : std::string_view("source")]++;
        }
        std::string text = "passes " + std::to_string(passes) + " (max " + fixed(longest, 2) + " ms, catch-up " +
                           std::to_string(catch_ups) + ", late " + std::to_string(late) +
                           (loading ? ", loading " + std::to_string(loading) : std::string()) + "); presents " +
                           std::to_string(presents) + " (in-pass " + std::to_string(in_pass) + ", max call " +
                           fixed(worst_call, 2) + " ms";
        for (const auto &[name, count] : paths)
            text += ", " + std::string(name) + " " + std::to_string(count);
        text += ")";
        const auto top = top_spans(a, t, 4);
        if (!top.empty())
            text += "; top scopes " + list(top);
        return text;
    }
    // Consecutive stutters of one type on consecutive frames are merged.
    void add_stutter(Stutter s, std::int64_t context_end)
    {
        s.context = context(context_end);
        if (pending_stutter_valid_ && s.first_id >= 0 && pending_stutter_.last_id == s.first_id - 1 &&
            std::string_view(pending_stutter_.type) == s.type && pending_stutter_.scene == s.scene)
        {
            auto &p = pending_stutter_;
            p.last_id = s.last_id;
            p.lost += s.lost;
            p.frames += 1;
            if (s.severity > p.severity)
            {
                p.severity = s.severity;
                p.ms = s.ms;
                p.expected = s.expected;
                p.cause = s.cause;
                p.detail = s.detail;
            }
            return;
        }
        flush_stutter();
        pending_stutter_ = std::move(s);
        pending_stutter_valid_ = true;
    }
    void flush_stutter()
    {
        if (!pending_stutter_valid_)
            return;
        pending_stutter_valid_ = false;
        emit_stutter(pending_stutter_);
    }
    std::string describe(const Stutter &s) const
    {
        const std::string_view type = s.type;
        const std::string frames = s.frames > 1 ? " (" + std::to_string(s.frames) + " frames in a row)" : "";
        if (type == "held")
            return "displayed frame held " + fixed(s.ms / period_ms(), 0) + " refreshes (" + fixed(s.ms, 1) +
                   " ms, instant " + fixed(1000.0 / (std::max)(s.ms, 0.001), 0) + " FPS; expected " +
                   fixed(s.expected, 1) + " ms), " + fixed(s.lost, 0) + " refresh(es) lost" + frames;
        if (type == "late")
            return "frame shown " + fixed(s.lost, 0) + " refresh(es) later than intended (latency " + fixed(s.ms, 1) +
                   " ms vs usual " + fixed(s.expected, 1) + " ms)" + frames;
        if (type == "anim_time")
            return "animation time step " + fixed(s.ms, 2) + " ms on a display step of " + fixed(s.expected, 2) +
                   " ms (" + fixed(s.severity, 1) + " refreshes off)" + frames;
        if (type == "motion" || type == "turn")
            return std::string("camera ") + (type == "motion" ? "motion" : "rotation") +
                   " irregular: " + fixed(s.ms, 2) + " vs " + fixed(s.expected, 2) + " expected (" +
                   fixed(s.severity, 2) + "x)" + frames;
        if (type == "cut")
            return "camera cut (pose jump " + fixed(s.ms, 0) + " units)";
        if (type == "sim")
            return "simulation below 60 ticks/s (" + fixed(s.ms, 2) + " ticks/s)";
        return std::string(s.type);
    }
    void emit_stutter(const Stutter &s)
    {
        const auto group = static_cast<std::size_t>(group_of(s.scene));
        ++stutter_total_;
        by_type_[group][s.type].count++;
        by_type_[group][s.type].lost += s.lost;
        by_cause_[group][s.cause].count++;
        by_cause_[group][s.cause].lost += s.lost;
        bucket(s.t).stutters++;
        auto &shared_state = shared();
        shared_state.stutters.fetch_add(1, std::memory_order_relaxed);
        (group == 0   ? shared_state.stutters_race
         : group == 1 ? shared_state.stutters_menu
                      : shared_state.stutters_loading)
            .fetch_add(1, std::memory_order_relaxed);
        shared_state.lost.fetch_add(static_cast<std::uint64_t>((std::max)(0.0, s.lost)), std::memory_order_relaxed);
        if (trace_)
        {
            row("X,%.4f,%s,%s,%.1f,%.4f,%.4f,%u,%s,%s\n", rel(s.t), scene_name(s.scene), s.type, s.lost, s.ms,
                s.expected, s.frames, s.cause.c_str(), csv(s.detail).c_str());
            row("XC,%.4f,%s\n", rel(s.t), csv(s.context).c_str());
        }
        if (!stutter_log_)
            return;
        const std::int64_t second = (s.t - config_.t0) / second_ns;
        if (second != lines_second_)
        {
            if (lines_suppressed_)
                log_line(last_suppressed_, wall(last_suppressed_, false) + " (" + std::to_string(lines_suppressed_) +
                                               " more stutters in that second, up to here; see the trace)\n");
            lines_second_ = second;
            lines_this_second_ = 0;
            lines_suppressed_ = 0;
        }
        if (++lines_this_second_ > stutter_lines_per_second)
        {
            ++lines_suppressed_;
            last_suppressed_ = s.t;
            return;
        }
        char tail[200];
        std::snprintf(tail, sizeof tail, " | type=%s lost=%.0f ms=%.2f scene=%s cause=%s\n", s.type, s.lost, s.ms,
                      scene_name(s.scene), s.cause.c_str());
        log_line(s.t, wall(s.t, false) + " " + scene_name(s.scene) + ": " + describe(s) + " - cause: " + s.cause +
                          ": " + s.detail + " | ctx: " + s.context + tail);
    }
    // Stutter-log lines are detected out of order (display statistics arrive a few
    // frames late); they are written in time order once 0.5 s old.
    void log_line(std::int64_t t, std::string text)
    {
        lines_.emplace_back(t, std::move(text));
    }
    void write_lines(std::int64_t before)
    {
        if (lines_.empty())
            return;
        std::stable_sort(lines_.begin(), lines_.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
        std::size_t n = 0;
        while (n < lines_.size() && lines_[n].first < before)
        {
            if (stutter_log_)
                std::fputs(lines_[n].second.c_str(), stutter_log_);
            ++n;
        }
        lines_.erase(lines_.begin(), lines_.begin() + static_cast<std::ptrdiff_t>(n));
    }
    // ---- per-second buckets ------------------------------------------------
    Bucket &bucket(std::int64_t t)
    {
        const std::int64_t second = (std::max)(std::int64_t{0}, t - config_.t0) / second_ns;
        return buckets_[(std::max)(second, closed_second_ + 1)];
    }
    void close_buckets(std::int64_t displayed_t)
    {
        const std::int64_t current = displayed_t == (std::numeric_limits<std::int64_t>::max)()
                                         ? (std::numeric_limits<std::int64_t>::max)()
                                         : (std::max)(std::int64_t{0}, displayed_t - config_.t0) / second_ns;
        if (current != (std::numeric_limits<std::int64_t>::max)())
            flush_stutter_before(current);
        while (!buckets_.empty() && buckets_.begin()->first < current)
        {
            const auto second = buckets_.begin()->first;
            // Seconds with no displayed frame at all (a freeze) are written too.
            for (std::int64_t gap = closed_second_ + 1;
                 closed_second_ >= 0 && gap < second && gap < closed_second_ + 11; ++gap)
                write_second(gap, Bucket{});
            write_second(second, buckets_.begin()->second);
            closed_second_ = second;
            buckets_.erase(buckets_.begin());
        }
    }
    void flush_stutter_before(std::int64_t second)
    {
        if (pending_stutter_valid_ && (pending_stutter_.t - config_.t0) / second_ns < second)
            flush_stutter();
    }
    void write_second(std::int64_t second, const Bucket &b)
    {
        std::size_t best = 0;
        for (std::size_t i = 1; i < b.scenes.size(); ++i)
            if (b.scenes[i] > b.scenes[best])
                best = i;
        const auto scene = static_cast<std::uint8_t>(best);
        const double fps = double(b.frames.size());
        const double low1 = low_of(b.frames, 100), low01 = low_of(b.frames, 1000);
        std::vector<double> sorted = b.frames;
        std::sort(sorted.begin(), sorted.end());
        const double p99 = sorted.empty() ? 0.0 : sorted[(std::min)(sorted.size() - 1, (sorted.size() * 99) / 100)];
        const double worst = sorted.empty() ? 0.0 : sorted.back();
        const std::int64_t t = config_.t0 + second * second_ns;
        auto &s = shared();
        s.fps_x10.store(static_cast<std::uint32_t>(fps * 10.0), std::memory_order_relaxed);
        s.low1_x10.store(static_cast<std::uint32_t>(low1 * 10.0), std::memory_order_relaxed);
        if (trace_)
            row("SEC,%.4f,%s,%.0f,%.1f,%.1f,%.3f,%.3f,%u,%u,%u,%u,%u,%u,%u,%u,%s\n", rel(t), scene_name(scene), fps,
                low1, low01, p99, worst, b.repeats, b.lost, b.stutters, b.ticks, b.late_ticks, b.catch_ups, b.presents,
                b.drops, b.cpu_timing() ? "cpu" : "dxgi");
        if (fps_log_)
            std::fprintf(
                fps_log_,
                "%s %-7s displayed %5.1f fps  1%% low %6.1f  0.1%% low %6.1f  frame p99 %6.2f ms max %7.2f ms  "
                "repeated vblanks %3u (beyond content rate %3u)  stutters %2u  ticks %2u (late %u, catch-up "
                "%u, long passes %u)  presents %3u%s%s\n",
                wall(t, false).c_str(), scene_name(scene), fps, low1, low01, p99, worst, b.repeats, b.lost, b.stutters,
                b.ticks, b.late_ticks, b.catch_ups, b.long_passes, b.presents,
                b.cpu_timing() ? "  (CPU Present times: no DXGI statistics)" : "", finishing_ ? "  (partial)" : "");
    }
    void summary(std::uint64_t dropped)
    {
        std::string text = "=== session summary (" + wall(last_t_, true) + ") ===\n";
        for (std::size_t g = 0; g < 3; ++g)
        {
            const auto &h = histograms_[g];
            if (!h.n && by_type_[g].empty())
                continue;
            char line[512];
            std::snprintf(line, sizeof line,
                          "%s: %llu displayed frames, average %.1f fps, frame time p50 %.2f p99 %.2f p99.9 %.2f max "
                          "%.2f ms, 1%% low %.1f fps, 0.1%% low %.1f fps; repeated vblanks %llu (beyond content rate "
                          "%llu)\n",
                          group_names[g], static_cast<unsigned long long>(h.n),
                          h.total_ms > 0 ? 1000.0 * double(h.n) / h.total_ms : 0.0, h.percentile(0.5),
                          h.percentile(0.99), h.percentile(0.999), h.max_ms, h.low(100), h.low(1000),
                          static_cast<unsigned long long>(repeats_total_[g]),
                          static_cast<unsigned long long>(lost_total_[g]));
            text += line;
            const auto ranked = [&](const std::map<std::string, Tally> &m, const char *title) {
                std::vector<std::pair<std::string, Tally>> v(m.begin(), m.end());
                std::sort(v.begin(), v.end(), [](const auto &x, const auto &y) {
                    return x.second.lost != y.second.lost ? x.second.lost > y.second.lost
                                                          : x.second.count > y.second.count;
                });
                text += std::string("  ") + title + ":";
                for (const auto &[key, tally] : v)
                    text += " " + key + " " + std::to_string(tally.count) + "x/" + fixed(tally.lost, 0) + "r";
                text += "\n";
            };
            ranked(by_type_[g], "stutters by type (count x / lost refreshes r)");
            ranked(by_cause_[g], "stutters by cause, most lost refreshes first");
        }
        char line[400];
        std::snprintf(
            line, sizeof line,
            "context: %llu passes (%llu late ticks, %llu catch-ups, %llu long, %llu resyncs), %llu presents, "
            "%llu dropped frames; DXGI statistics errors %llu, disjoint %llu; trace ring overflow %llu\n",
            static_cast<unsigned long long>(passes_total_), static_cast<unsigned long long>(late_ticks_total_),
            static_cast<unsigned long long>(catch_ups_total_), static_cast<unsigned long long>(long_passes_total_),
            static_cast<unsigned long long>(resyncs_total_), static_cast<unsigned long long>(presents_total_),
            static_cast<unsigned long long>(drops_total_), static_cast<unsigned long long>(stats_errors_),
            static_cast<unsigned long long>(stats_disjoint_), static_cast<unsigned long long>(dropped));
        text += line;
        std::snprintf(line, sizeof line,
                      "detector cost on the game thread: DXGI statistics query after each Present avg %.1f us, max "
                      "%.1f us%s\n",
                      query_count_ ? double(query_total_) / double(query_count_) / 1000.0 : 0.0,
                      double(query_worst_) / 1000.0,
                      id_offset_ ? "; NOTE: DXGI PresentCount did not follow GetLastPresentCount (re-based)" : "");
        text += line;
        if (stutter_log_)
            std::fputs(text.c_str(), stutter_log_);
        if (trace_)
            out_ += "# " + [&] {
                std::string commented;
                for (const char c : text)
                {
                    commented += c;
                    if (c == '\n')
                        commented += "# ";
                }
                return commented;
            }() + "end\n";
        if (stutter_log_)
            std::fflush(stutter_log_);
        if (fps_log_)
            std::fflush(fps_log_);
    }
    void trim(std::int64_t now)
    {
        const std::int64_t limit = now - history_ns;
        while (!passes_.empty() && passes_.front().end < limit)
            passes_.pop_front();
        while (!spans_.empty() && spans_.front().end < limit)
            spans_.pop_front();
        while (!values_.empty() && values_.front().t < limit)
            values_.pop_front();
    }
};

void writer_main(Shared *s, Analyzer *raw) noexcept
{
    std::unique_ptr<Analyzer> analyzer(raw);
    try
    {
        analyzer->open();
    }
    catch (...)
    {
    }
    std::vector<Event> batch;
    for (;;)
    {
        bool stopping = false;
        {
            std::unique_lock lock(s->mutex);
            s->wake.wait_for(lock, std::chrono::milliseconds(writer_period_ms), [s] { return s->stop; });
            stopping = s->stop;
        }
        try
        {
            // Drain in bounded batches so the ring frees space early.
            for (;;)
            {
                const auto tail = s->tail.load(std::memory_order_relaxed);
                const auto head = s->head.load(std::memory_order_acquire);
                if (tail == head)
                    break;
                const auto count = (std::min)(head - tail, std::uint64_t{8192});
                batch.resize(static_cast<std::size_t>(count));
                for (std::uint64_t i = 0; i < count; ++i)
                    batch[static_cast<std::size_t>(i)] = s->ring[static_cast<std::size_t>((tail + i) & s->mask)];
                s->tail.store(tail + count, std::memory_order_release);
                for (const auto &e : batch)
                    analyzer->process(e);
            }
            analyzer->after_drain(s->dropped.load(std::memory_order_relaxed));
            if (stopping)
            {
                analyzer->finish(s->dropped.load(std::memory_order_relaxed));
                return;
            }
        }
        catch (...)
        {
            // Diagnostics never take the game down: a failed batch is lost.
            if (stopping)
                return;
        }
    }
}
std::filesystem::path environment_path(const char *name)
{
    wchar_t buffer[1024]{};
    std::size_t size = 0;
    std::wstring wide(name, name + std::strlen(name));
    if (_wgetenv_s(&size, buffer, std::size(buffer), wide.c_str()) != 0 || size <= 1)
        return {};
    return std::filesystem::path(buffer);
}
std::string environment_text(const char *name)
{
    char buffer[64]{};
    std::size_t size = 0;
    if (getenv_s(&size, buffer, sizeof buffer, name) != 0 || size <= 1)
        return {};
    return buffer;
}
V3 lerp(const V3 &a, const V3 &b, double t) noexcept
{
    return {a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t};
}
V3 column(const std::array<double, 9> &m, unsigned c) noexcept
{
    return {m[c], m[3 + c], m[6 + c]};
}
V3 normalized(V3 v) noexcept
{
    const double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (!(l > 1e-12) || !std::isfinite(l))
        return v;
    return {v[0] / l, v[1] / l, v[2] / l};
}
} // namespace

bool scope_wanted(unsigned depth) noexcept
{
    const auto &s = shared();
    if (!on(s))
        return false;
    return s.trace || depth < detector_scope_depth;
}
void span(const char *stage, std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point end,
          unsigned depth) noexcept
{
    auto &s = shared();
    if (!on(s))
        return;
    const std::int64_t t1 = ns_of(end), duration = t1 - ns_of(start);
    if (!s.trace && depth >= detector_scope_depth)
        return;
    if (s.trace && depth >= trace_individual_depth && duration < trace_individual_ns && add_sum(s, stage, duration))
        return;
    Event e;
    e.t = t1;
    e.type = Type::span;
    e.name = stage;
    e.scene = s.scene;
    e.depth = static_cast<std::uint8_t>((std::min)(depth, 0xfeu));
    e.v[0] = duration;
    push(s, e);
    if (depth == 0 && s.sum_count)
        flush_sums(s, t1);
}
void value(const char *stage, std::int64_t nanoseconds) noexcept
{
    auto &s = shared();
    if (!on(s))
        return;
    Event e;
    e.t = now_ns();
    e.type = Type::value;
    e.name = stage;
    e.scene = s.scene;
    e.v[0] = nanoseconds;
    push(s, e);
}
void pass_begin(std::int64_t due_qpc, std::uint32_t scheduled, std::uint32_t increment, std::uint64_t counter_frequency,
                bool presenter_active) noexcept
{
    auto &s = shared();
    if (!on(s))
        return;
    const auto now = now_ns();
    if (s.sum_count)
        flush_sums(s, now);
    s.pass_open = true;
    s.loading = false;
    s.interp_pending = false;
    s.scene = scene_now(s);
    Event e;
    e.t = now;
    e.type = Type::pass_begin;
    e.scene = s.scene;
    e.flags = presenter_active ? 1u : 0u;
    e.u = s.scene_word ? *s.scene_word : 0u;
    e.v[0] = due_qpc ? qpc_to_ns(due_qpc, s.qpc_frequency) : 0;
    e.v[1] = scheduled;
    e.v[2] = increment;
    e.v[3] = static_cast<std::int64_t>(counter_frequency);
    push(s, e);
    publish_info(s, now);
}
void pass_end(std::uint32_t scheduled) noexcept
{
    auto &s = shared();
    if (!on(s) || !s.pass_open)
        return;
    const auto now = now_ns();
    if (s.sum_count)
        flush_sums(s, now);
    s.pass_open = false;
    s.loading = false;
    s.scene = scene_now(s);
    Event e;
    e.t = now;
    e.type = Type::pass_end;
    e.scene = s.scene;
    e.v[1] = scheduled;
    push(s, e);
}
void loading_frame() noexcept
{
    auto &s = shared();
    if (!on(s) || s.loading)
        return;
    s.loading = true;
    s.scene = scene_loading;
    Event e;
    e.t = now_ns();
    e.type = Type::loading;
    e.scene = s.scene;
    push(s, e);
}
bool active() noexcept
{
    return on(shared());
}
void interp_sample(const host_presentation::PresenterOptions &options, double at, double state,
                   double counter_frequency, double alpha, const host_presentation::ViewSnapshot *a,
                   const host_presentation::ViewSnapshot &b, std::uint32_t tick_a, std::uint32_t tick_b,
                   double pose_delay_seconds) noexcept
{
    auto &s = shared();
    if (!on(s))
        return;
    const auto now = now_ns();
    s.interp_pending = true;
    // The presenter's cut rule (transforms(): view change, cut_distance, max_turn).
    bool view_changed = false, cut = false;
    double distance = 0, turn = 0;
    if (a)
    {
        view_changed = a->controller != b.controller || a->camera_mode != b.camera_mode ||
                       a->controller_vehicle != b.controller_vehicle;
        const V3 d{b.camera_position[0] - a->camera_position[0], b.camera_position[1] - a->camera_position[1],
                   b.camera_position[2] - a->camera_position[2]};
        distance = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        double inner = 0; // trace(A^T B)
        for (std::size_t i = 0; i < 9; ++i)
            inner += a->world_to_camera[i] * b.world_to_camera[i];
        turn = std::acos(std::clamp((inner - 1.0) * 0.5, -1.0, 1.0));
        cut = view_changed || !(distance <= options.cut_distance) || turn > options.max_turn_radians;
    }
    const bool interpolated = a && alpha < 1.0 && !cut;
    const double t = interpolated ? alpha : 1.0;
    const V3 position = interpolated ? lerp(a->camera_position, b.camera_position, t) : b.camera_position;
    const V3 forward = interpolated ? normalized(lerp(column(a->world_to_camera, 2), column(b.world_to_camera, 2), t))
                                    : column(b.world_to_camera, 2);
    const V3 up = interpolated ? normalized(lerp(column(a->world_to_camera, 1), column(b.world_to_camera, 1), t))
                               : column(b.world_to_camera, 1);
    Event e;
    e.t = now;
    e.type = Type::interp;
    e.scene = s.scene;
    e.flags = static_cast<std::uint8_t>((s.pass_open ? 1u : 0u) | (a ? 2u : 0u));
    e.v[0] = pack(at);
    e.v[1] = pack(state);
    e.v[2] = pack(alpha);
    e.v[3] = static_cast<std::int64_t>((std::uint64_t{tick_a} << 32) | tick_b);
    e.v[4] = pack(pose_delay_seconds);
    e.v[5] = pack(counter_frequency);
    push(s, e);
    Event c;
    c.t = now;
    c.type = Type::camera;
    c.scene = s.scene;
    c.flags = static_cast<std::uint8_t>((cut ? camera_cut : 0u) | (view_changed ? camera_view_changed : 0u) |
                                        (a ? camera_pair : 0u) | (interpolated ? camera_interpolated : 0u));
    c.v[0] = pack(position[0]);
    c.v[1] = pack(position[1]);
    c.v[2] = pack(position[2]);
    c.v[3] = pack2(static_cast<float>(forward[0]), static_cast<float>(forward[1]));
    c.v[4] = pack2(static_cast<float>(forward[2]), static_cast<float>(up[0]));
    c.v[5] = pack2(static_cast<float>(up[1]), static_cast<float>(up[2]));
    c.v[6] = pack2(static_cast<float>(distance), static_cast<float>(turn));
    push(s, c);
}
std::int64_t present_begin() noexcept
{
    return on(shared()) ? now_ns() : 0;
}
void present_end(IDXGISwapChain *swap, std::int64_t begin, long status, unsigned sync_interval, bool tearing) noexcept
{
    if (!begin)
        return;
    auto &s = shared();
    if (!on(s))
        return;
    const auto end = now_ns();
    UINT id = 0;
    DXGI_FRAME_STATISTICS stats{};
    HRESULT id_result = E_POINTER, stats_result = E_POINTER;
    if (swap)
    {
        id_result = swap->GetLastPresentCount(&id);
        stats_result = swap->GetFrameStatistics(&stats);
    }
    const auto after = now_ns();
    Event p;
    p.t = end;
    p.type = Type::present;
    p.scene = s.scene;
    p.flags = static_cast<std::uint8_t>((sync_interval ? present_sync : 0u) | (tearing ? present_tearing : 0u) |
                                        (s.interp_pending ? present_interp : 0u) |
                                        (s.interp_pending && s.pass_open ? present_in_pass : 0u));
    p.v[0] = begin;
    p.v[1] = SUCCEEDED(id_result) ? std::int64_t{id} : -1;
    p.v[2] = status;
    push(s, p);
    Event d;
    d.t = after;
    d.type = Type::stats;
    d.scene = s.scene;
    d.v[0] = stats.PresentCount;
    d.v[1] = stats.PresentRefreshCount;
    d.v[2] = stats.SyncRefreshCount;
    d.v[3] = SUCCEEDED(stats_result) && stats.SyncQPCTime.QuadPart
                 ? qpc_to_ns(stats.SyncQPCTime.QuadPart, s.qpc_frequency)
                 : 0;
    d.v[4] = stats_result;
    d.v[5] = after - end;
    push(s, d);
    s.interp_pending = false;
    if (s.sum_count)
        flush_sums(s, after);
}

Session::Session(const std::uint32_t &scene_word, const std::uint32_t &pause_word,
                 const std::filesystem::path &reports) noexcept
{
    auto &s = shared();
    if (s.active.load() || s.writer.joinable())
        return; // one session at a time
    try
    {
        const auto mode = environment_text("SGN_STUTTER");
        const auto trace_path = environment_path("SGN_TRACE");
        if (!mode.empty() && mode[0] == '0' && trace_path.empty())
            return;
        Config config;
        config.trace = !trace_path.empty();
        config.trace_path = trace_path;
        config.reports = reports;
        std::uint64_t cap_mb = 1024;
        if (const auto cap = environment_text("SGN_TRACE_MAX_MB"); !cap.empty())
            cap_mb = std::strtoull(cap.c_str(), nullptr, 10);
        config.trace_cap = cap_mb << 20;
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        s.qpc_frequency = frequency.QuadPart > 0 ? frequency.QuadPart : 1;
        config.t0 = now_ns();
        GetSystemTimePreciseAsFileTime(&config.wall0);
        const std::size_t size = config.trace ? trace_ring_events : detector_ring_events;
        s.ring.assign(size, Event{});
        s.mask = size - 1;
        s.head.store(0);
        s.tail.store(0);
        s.dropped.store(0);
        s.trace = config.trace;
        s.scene_word = &scene_word;
        s.pause_word = &pause_word;
        s.scene = scene_now(s);
        s.pass_open = s.loading = s.interp_pending = false;
        s.sum_count = 0;
        s.sums = {};
        s.stop = false;
        auto analyzer = std::make_unique<Analyzer>(std::move(config));
        s.writer = std::thread(writer_main, &s, analyzer.get());
        analyzer.release(); // owned by the writer thread
        s.owner.store(GetCurrentThreadId());
        s.active.store(true);
        OutputDebugStringA(s.trace ? "SGN frame trace: stutter detector + full trace (SGN_TRACE)\n"
                                   : "SGN frame trace: stutter detector (reports/stutter-log.txt, fps-log.txt)\n");
    }
    catch (...)
    {
        s.active.store(false);
    }
}
Session::~Session()
{
    auto &s = shared();
    if (!s.writer.joinable() || s.owner.load() != GetCurrentThreadId())
        return;
    s.active.store(false);
    {
        std::lock_guard lock(s.mutex);
        s.stop = true;
    }
    s.wake.notify_one();
    s.writer.join();
}
} // namespace sgn::host_frame_trace
