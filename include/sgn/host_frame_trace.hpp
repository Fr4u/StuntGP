#pragma once
// NEW HOST (R34) frame trace and stutter detector. Diagnostics only, not
// recovered behaviour: nothing here is read by the game, the pacing, the
// presenter or the renderer, and no hook changes a decision, a value, a wait or
// a pixel. Every hook is a no-op until a Session exists (the game path creates
// one in run_source_menu) and on any thread but the one that created it.
//
// DISPLAYED FRAMES are the primary metric. After every Present the renderer
// hook reads IDXGISwapChain::GetLastPresentCount and GetFrameStatistics
// (flip model: PresentCount, PresentRefreshCount, SyncRefreshCount,
// SyncQPCTime), so the vblank on which each new frame appeared is known. A
// background writer thread turns that into displayed frames (time between two
// distinct frames on screen), displayed FPS per second, 1% / 0.1% lows, and
// detects every STUTTER on its own, in menus, loading screens and races:
//  - held: a frame stayed on screen longer than 1.5x the expected frame period
//    (interpolated presents: the refresh cadence; source 60 Hz frames: one
//    tick), i.e. vblanks without a new frame (lost refreshes);
//  - late: a frame reached the screen later than the usual present-to-display
//    latency (one refresh or more) without a held predecessor;
//  - anim_time: the presented state time advanced by a different amount than
//    the display time (or went backwards) between two displayed frames;
//  - motion / turn: the camera displacement or rotation per displayed frame
//    deviates from the local median at the current speed (>1.5x or <0.5x);
//  - cut: the presenter's cut rule (cut_distance / max_turn / view change);
//  - sim: fewer than 59.5 source ticks in a sliding 1 s window.
// Each stutter gets a likely cause from the preceding events (source passes,
// catch-up ticks, resyncs, per-stage scopes, pacing waits, Present blocking,
// presenter path: gpu / cpu build / at_N / cut shown as B / held / fallback,
// scene transitions, loading frames).
//
// Modes (environment, read when the Session starts):
//  - default: detector only. Outputs <root>/reports/stutter-log.txt (one
//    human-readable line per stutter) and <root>/reports/fps-log.txt (one line
//    per second: displayed FPS, 1% / 0.1% low, repeated vblanks, stutters,
//    scene). Records only the coarse hooks plus the outermost two profiler
//    scope levels into a small in-memory ring (no file I/O on the game thread).
//  - SGN_STUTTER=0: off (every hook is a single test).
//  - SGN_TRACE=<file>: additionally every event (all profiler scopes, deeper
//    short ones summed per outermost scope) is written as CSV to <file> by the
//    background thread; analyse it with tools/frame_trace_analyze.py.
//    SGN_TRACE_MAX_MB (default 1024) bounds the file: when reached it is moved
//    to <file>.1 and a new one starts.
// Independent of SGN_PROFILE; with SGN_PROFILE set the profiler report header
// also shows a running "stutter" info line.
#include <chrono>
#include <cstdint>
#include <filesystem>

struct IDXGISwapChain;
namespace sgn::host_presentation
{
struct ViewSnapshot;
struct PresenterOptions;
} // namespace sgn::host_presentation

namespace sgn::host_frame_trace
{
// host_profiler: whether a scope at this nesting depth should be timed for the
// trace (SGN_TRACE: every depth; detector: depths 0 and 1).
bool scope_wanted(unsigned depth) noexcept;
// host_profiler forwards every timed scope and every add() value.
void span(const char *stage, std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point end,
          unsigned depth) noexcept;
void value(const char *stage, std::int64_t nanoseconds) noexcept;

// Pacing (source_host_pacing_win32.cpp): a released source pass. due_qpc: QPC
// time of its due tick (0 when the host cadence released it); scheduled /
// increment: the 612D00 clock words (ticks = scheduled delta / increment).
void pass_begin(std::int64_t due_qpc, std::uint32_t scheduled, std::uint32_t increment, std::uint64_t counter_frequency,
                bool presenter_active) noexcept;
void pass_end(std::uint32_t scheduled) noexcept;
// One 445940 loading-sequence counter read (SourceHostLoadingPacedCounter).
void loading_frame() noexcept;

// True while this thread records (the presenter skips building its arguments otherwise).
bool active() noexcept;
// Presenter: the pose sample of one interpolated present (before its Present).
// at / state: source counter units (state = at - 1.5 ticks); alpha: blend
// factor of the pair (a = N-1 or null, b = N).
void interp_sample(const host_presentation::PresenterOptions &options, double at, double state,
                   double counter_frequency, double alpha, const host_presentation::ViewSnapshot *a,
                   const host_presentation::ViewSnapshot &b, std::uint32_t tick_a, std::uint32_t tick_b,
                   double pose_delay_seconds) noexcept;

// Renderer: around the single IDXGISwapChain::Present call. present_begin()
// returns 0 while inactive; present_end() then does nothing.
std::int64_t present_begin() noexcept;
void present_end(IDXGISwapChain *swap, std::int64_t begin, long status, unsigned sync_interval, bool tearing) noexcept;

// The game path's trace lifetime (game thread). scene_word: 612BE8 (1 race,
// 2 menu); pause_word: 61294C frame_pause (1 = race paused). reports: the
// directory of stutter-log.txt / fps-log.txt. Destruction stops recording,
// drains the ring and writes the session summary.
class Session
{
  public:
    Session(const std::uint32_t &scene_word, const std::uint32_t &pause_word,
            const std::filesystem::path &reports) noexcept;
    ~Session();
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;
};
} // namespace sgn::host_frame_trace
