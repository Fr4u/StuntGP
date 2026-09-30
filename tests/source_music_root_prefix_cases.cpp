// Authored receiving cases, not execution results. No installed API is selected.
#include "sgn/source_music_timer_entry_prefix.hpp"
#include "sgn/source_music_period_host.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session.hpp"
#include "../src/source_music_root_scratch.hpp"
#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <new>
#include <optional>
#include <stdexcept>
#include <thread>
#include <tuple>

namespace { thread_local bool deny_allocations{}; }
void* operator new(std::size_t bytes) {
    if (deny_allocations) throw std::bad_alloc();
    if (void* p = std::malloc(bytes == 0 ? 1 : bytes)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

namespace sgn::retail {
// Defined only in this test translation unit. It never captures the prefix's
// local, creates a public provider, or lets production resume an observation.
struct SourceMusicRootScratchTestAccess {
    static std::array<std::uint8_t, 5> initial() {
        SourceMusicRootScratch root;
        return root.bytes_;
    }
    static std::array<std::uint8_t, 5> copy(const RaceSessionPlatform& actual) {
        actual.require_owner_thread();
        SourceMusicRootScratch root;
        root.copy_filename(actual.state().audio_stream);
        return root.bytes_;
    }
};
}

namespace {
using namespace sgn::retail;
void require(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
struct Event { char kind{}; std::uint32_t argument{}, stored_period{}, filename{}; };
struct Calls final : SourceMusicPeriodCalls {
    AudioStreamState* actual{}; // test-only borrow; same real platform owns it
    std::array<Event, 16> events{};
    std::size_t count{};
    std::uint32_t begin_result{}, end_result{};
    bool deny_after_begin{}, mutate_in_begin{}, saved_access_rejected{};
    std::uint32_t begin_filename{0x7e2f4243u}, begin_period_store{909};
    std::shared_ptr<RaceSessionPlatform>* release_in_begin{};
    std::optional<SourceMusicTimerCapsAccess> escaped;
    bool test_expired_in_begin{};
    void record(char kind, std::uint32_t value) noexcept {
        if (count == events.size()) std::terminate();
        events[count++] = {kind, value, actual ? actual->timer_period : 0u,
            actual ? actual->field_df54 : 0u};
    }
    std::uint32_t begin(std::uint32_t period) noexcept override {
        record('B', period);
        if (test_expired_in_begin) {
            try { (void)escaped->minimum(); }
            catch (const std::logic_error&) { saved_access_rejected = true; }
        }
        if (mutate_in_begin) {
            actual->field_df54 = begin_filename;
            actual->timer_period = begin_period_store;
        }
        if (release_in_begin) release_in_begin->reset();
        if (deny_after_begin) deny_allocations = true;
        return begin_result;
    }
    std::uint32_t end(std::uint32_t period) noexcept override {
        record('E', period);
        return end_result;
    }
};
struct Caps final : SourceMusicTimerCapsReceiving {
    Calls& calls;
    std::uint32_t minimum{3}, maximum{10}, result{};
    bool write_maximum{true}, throw_after_minimum{}, deny_after_caps{}, mutate_filename{};
    std::uint32_t filename{0x7e5c3a43u};
    std::shared_ptr<RaceSessionPlatform>* release_in_caps{};
    explicit Caps(Calls& value) : calls(value) {}
    std::uint32_t time_get_dev_caps(SourceMusicTimerCapsAccess output, std::uint32_t bytes) override {
        calls.record('C', bytes);
        require(bytes == 8, "capability byte count changed");
        calls.escaped = output;
        output.minimum(minimum);
        require(calls.actual->timer_capabilities.minimum() == minimum, "capability writes missed actual state");
        if (throw_after_minimum) throw std::runtime_error("receiving capability exception");
        if (write_maximum) output.maximum(maximum);
        if (mutate_filename) calls.actual->field_df54 = filename;
        if (release_in_caps) release_in_caps->reset();
        if (deny_after_caps) deny_allocations = true;
        return result;
    }
};
struct Fixture {
    std::shared_ptr<RaceSessionPlatform> platform = std::make_shared<RaceSessionPlatform>(
        SourceColdConstructionOnly{}, std::filesystem::path{});
    std::shared_ptr<Calls> calls = std::make_shared<Calls>();
    std::shared_ptr<const SourceMusicPeriodReport> report;
    Caps caps{*calls};
    explicit Fixture(bool bind = true) {
        calls->actual = &platform->state().audio_stream;
        auto& s = *calls->actual;
        s.timer_capabilities.minimum() = 9;
        s.timer_capabilities.maximum() = 100;
        s.timer_period = 77;
        s.field_df54 = 0x7e5c3a44u;
        s.filename_tail.fill(0xabu);
        s.result = 0x89abcdefu;
        s.allocated = 0;
        s.flags = 0x10203040u;
        s.timer = 0x76543210u;
        if (bind) report = prepare_receiving_music_period_owner(platform, calls);
    }
};
auto preserved_source(const AudioStreamState& s) {
    return std::tuple{s.field_df54, s.filename_tail, s.result, s.allocated, s.flags, s.timer};
}
void check_expired(const Calls& calls) {
    bool rejected = false;
    try { (void)calls.escaped->minimum(); } catch (const std::logic_error&) { rejected = true; }
    require(rejected, "capability access escaped its synchronous scope");
}

void unsigned_clamp_and_stored_argument() {
    // Independent literal expectations from the pinned unsigned branches.
    constexpr std::array<std::array<std::uint32_t, 3>, 16> cases{{
        {{0,0,0}},{{0,1,1}},{{0,15,1}},{{1,0,0}},{{1,1,1}},{{1,15,1}},
        {{2,1,1}},{{2,2,2}},{{3,10,3}},{{10,3,3}},{{0xffffffffu,0,0}},
        {{0xffffffffu,0xffffffffu,0xffffffffu}},{{0x80000000u,0x7fffffffu,0x7fffffffu}},
        {{0x7fffffffu,0x80000000u,0x7fffffffu}},{{2,0xffffffffu,2}},{{0,0xffffffffu,1}}
    }};
    for (const auto& row : cases) {
        Fixture f; f.caps.minimum = row[0]; f.caps.maximum = row[1];
        const auto before = preserved_source(*f.calls->actual);
        const auto out = receive_source_music_timer_prefix_45fda0(f.platform, f.caps);
        require(out.minimum == row[0] && out.maximum == row[1] && out.selected_period == row[2], "unsigned branch result differs");
        require(out.next_source_site == 0x45fe17 && !out.original_function_returned, "prefix endpoint overclaims source return");
        require(f.calls->count == 2 && f.calls->events[0].kind == 'C' && f.calls->events[1].kind == 'B', "one-call prefix duplicated or reordered calls");
        require(f.calls->events[1].argument == row[2] && f.calls->events[1].stored_period == row[2], "Begin preceded the actual source store or received another period");
        require(before == preserved_source(*f.calls->actual), "root/prefix changed unrelated source state");
        require(f.report->snapshot().pending_calls == 1 && f.report->snapshot().end_calls == 0, "prefix ended its persistent request");
    }
}
void root_null_partitions_and_canaries() {
    constexpr std::array<std::uint8_t, 5> zeros{};
    require(SourceMusicRootScratchTestAccess::initial() == zeros, "five prologue bytes not zero");
    struct Row { std::uint32_t word; std::array<std::uint8_t, 5> expected; };
    constexpr std::array<Row, 6> cases{{
        {0x7e434200u,{0,0,0,0,0}}, {0x7e430041u,{65,0,0,0,0}},
        {0x7e004241u,{65,66,0,0,0}}, {0x7e434241u,{65,66,67,0,0}},
        {0x7e2fff80u,{128,255,47,0,0}}, {0x7e5c3a43u,{67,58,92,0,0}}
    }};
    Fixture f;
    for (const auto& row : cases) {
        f.calls->actual->field_df54 = row.word;
        const auto before = preserved_source(*f.calls->actual);
        require(SourceMusicRootScratchTestAccess::copy(*f.platform) == row.expected, "count3 root bytes differ from pinned cases");
        require(before == preserved_source(*f.calls->actual), "child wrote its source or tail");
    }
    require(f.calls->count == 0, "private child inspection invoked a backend");
    // This private inspection proves helper values, not an end-to-end capture
    // of the prefix's inaccessible local or generated machine read widths.
}
void capability_errors_do_not_begin() {
    for (const auto result : {1u,97u,0x80000000u,0xffffffffu}) {
        Fixture f; f.caps.result = result; f.caps.minimum = 7; f.caps.write_maximum = false;
        bool trapped = false;
        try { (void)receive_source_music_timer_prefix_45fda0(f.platform,f.caps); }
        catch (const SourceMusicTimerCapabilityTrap& e) { trapped = e.capability_result() == result; }
        require(trapped && f.calls->count == 1 && f.report->snapshot().begin_calls == 0, "nonzero capability result reached Begin");
        require(f.calls->actual->timer_capabilities.minimum() == 7 && f.calls->actual->timer_capabilities.maximum() == 100 && f.calls->actual->timer_period == 77, "error rolled back partial writes or selected a period");
        require(f.calls->actual->result == 0x89abcdefu, "capability trap stored a fake source result");
        check_expired(*f.calls);
    }
}
void capability_exception_preserves_partial_write() {
    Fixture f; f.caps.throw_after_minimum = true; f.caps.minimum = 8;
    bool threw = false;
    try { (void)receive_source_music_timer_prefix_45fda0(f.platform,f.caps); }
    catch (const std::runtime_error&) { threw = true; }
    require(threw && f.calls->count == 1 && f.calls->actual->timer_capabilities.minimum() == 8 && f.calls->actual->timer_period == 77, "capability exception lost order/state");
    check_expired(*f.calls);
}
void live_state_after_providers() {
    Fixture f; f.caps.mutate_filename = true; f.calls->mutate_in_begin = true;
    const auto out = receive_source_music_timer_prefix_45fda0(f.platform,f.caps);
    require(f.calls->events[1].filename == f.caps.filename, "Begin saw a prefetched/copied filename owner");
    require(f.calls->actual->field_df54 == f.calls->begin_filename && f.calls->actual->timer_period == 909, "post-Begin state was overwritten");
    require(out.selected_period == 3, "observation rewrote the original selection");
    require(SourceMusicRootScratchTestAccess::copy(*f.platform) == std::array<std::uint8_t,5>{67,66,47,0,0}, "same helper does not select current actual filename");
    // The actual prefix-to-helper call/order is independently source-reviewed;
    // this later private helper check is not claimed as that local's snapshot.
    f.platform.reset();
    require(f.calls->events[2].kind == 'E' && f.calls->events[2].argument == 3, "cleanup used mutated source period instead of its exact recorded request");
}
void failed_begin_is_ignored_by_source() {
    Fixture f; f.calls->begin_result = 97;
    const auto before = preserved_source(*f.calls->actual);
    const auto out = receive_source_music_timer_prefix_45fda0(f.platform,f.caps);
    const auto report = f.report->snapshot();
    require(out.next_source_site == 0x45fe17 && report.begin_failures == 1 && report.begin_successes == 0 && report.pending_calls == 1, "Begin failure trapped, vanished or was counted as success");
    require(before == preserved_source(*f.calls->actual), "ignored MMRESULT changed source result or filename");
    f.platform.reset(); require(f.calls->events[2].argument == 3, "failed reached Begin lost host pairing");
}
void repeated_calls_reenter_prologue() {
    Fixture f;
    (void)receive_source_music_timer_prefix_45fda0(f.platform,f.caps);
    f.caps.minimum = 6;
    (void)receive_source_music_timer_prefix_45fda0(f.platform,f.caps);
    require(f.calls->count == 4 && f.calls->events[2].kind == 'C' && f.calls->events[3].kind == 'B' && f.calls->events[3].argument == 6, "second entry resumed or collapsed a prior call");
    require(f.report->snapshot().pending_calls == 2, "repeated prefix lost obligation");
    f.platform.reset(); require(f.calls->count == 6 && f.calls->events[4].argument == 3 && f.calls->events[5].argument == 6, "prefix cleanup order differs");
}
void explicit_end_not_duplicated() {
    Fixture f; (void)receive_source_music_timer_prefix_45fda0(f.platform,f.caps);
    require(end_owned_music_period(f.platform,3) == 0, "explicit host End failed");
    f.platform.reset(); require(f.calls->count == 3 && f.report->snapshot().teardown_end_calls == 0, "successful explicit End repeated at teardown");
}
void reservation_failure_after_source_store() {
    Fixture f; f.caps.deny_after_caps = true; bool failed = false;
    try { (void)receive_source_music_timer_prefix_45fda0(f.platform,f.caps); }
    catch (const std::bad_alloc&) { failed = true; }
    deny_allocations = false;
    require(failed && f.calls->count == 1 && f.report->snapshot().begin_calls == 0 && f.calls->actual->timer_period == 3, "reservation failure occurred after Begin or rolled back source store");
    check_expired(*f.calls);
}
void no_allocation_after_begin() {
    Fixture f; f.calls->deny_after_begin = true; bool failed = false;
    try { (void)receive_source_music_timer_prefix_45fda0(f.platform,f.caps); }
    catch (const std::bad_alloc&) { failed = true; }
    deny_allocations = false;
    require(!failed && f.calls->count == 2 && f.report->snapshot().pending_calls == 1, "root continuation introduced post-Begin allocation");
}
void owner_retained_through_capability_call() {
    Fixture f; std::weak_ptr<RaceSessionPlatform> weak = f.platform; f.caps.release_in_caps = &f.platform;
    const auto out = receive_source_music_timer_prefix_45fda0(f.platform,f.caps);
    require(out.next_source_site == 0x45fe17 && !f.platform && weak.expired(), "prefix did not release retained owner at return");
    require(f.calls->count == 3 && f.calls->events[1].kind == 'B' && f.calls->events[2].kind == 'E' && f.report->snapshot().finalized, "platform died before continuation or leaked after return");
    check_expired(*f.calls);
}
void owner_retained_through_begin_call() {
    Fixture f; f.calls->release_in_begin = &f.platform;
    const auto out = receive_source_music_timer_prefix_45fda0(f.platform,f.caps);
    require(out.selected_period == 3 && !f.platform && f.calls->count == 3 && f.report->snapshot().begin_successes == 1 && f.report->snapshot().finalized, "Begin release destroyed actual state before prefix finished");
}
void capability_access_expired_before_begin() {
    Fixture f; f.calls->test_expired_in_begin = true;
    (void)receive_source_music_timer_prefix_45fda0(f.platform,f.caps);
    require(f.calls->saved_access_rejected && f.calls->count == 2, "capability borrow stayed active into Begin");
}
void foreign_entry_rejected_before_calls() {
    Fixture f; bool rejected = false;
    std::thread worker([&] {
        try { (void)receive_source_music_timer_prefix_45fda0(f.platform,f.caps); }
        catch (const std::logic_error&) { rejected = true; }
    });
    worker.join();
    require(rejected && f.calls->count == 0 && f.calls->actual->timer_period == 77, "foreign entry touched source or backend");
}
void null_owner_rejected_before_calls() {
    Fixture f; bool rejected = false;
    try { (void)receive_source_music_timer_prefix_45fda0({},f.caps); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected && f.calls->count == 0, "null owner reached provider");
}
void missing_period_owner_stops_at_begin_boundary() {
    Fixture f(false); bool rejected = false;
    try { (void)receive_source_music_timer_prefix_45fda0(f.platform,f.caps); }
    catch (const std::logic_error&) { rejected = true; }
    require(rejected && f.calls->count == 1 && f.calls->actual->timer_period == 3 && f.calls->actual->result == 0x89abcdefu, "missing owner was fabricated or changed source failure policy");
    check_expired(*f.calls);
}
void no_broad_backend_prepared() {
    Fixture f; (void)receive_source_music_timer_prefix_45fda0(f.platform,f.caps);
    const auto ready = f.platform->host_readiness();
    require(!ready.assets && !ready.audio_device && !ready.audio_stream && !ready.window_input, "prefix prepared a broader host backend");
    require(f.calls->actual->allocated == 0 && f.calls->actual->result == 0x89abcdefu, "prefix fabricated source readiness/result");
}
}

int main() {
    try {
        unsigned_clamp_and_stored_argument(); root_null_partitions_and_canaries();
        capability_errors_do_not_begin(); capability_exception_preserves_partial_write();
        live_state_after_providers(); failed_begin_is_ignored_by_source();
        repeated_calls_reenter_prologue(); explicit_end_not_duplicated();
        reservation_failure_after_source_store(); no_allocation_after_begin();
        owner_retained_through_capability_call(); owner_retained_through_begin_call();
        capability_access_expired_before_begin(); foreign_entry_rejected_before_calls();
        null_owner_rejected_before_calls(); missing_period_owner_stops_at_begin_boundary();
        no_broad_backend_prepared();
        std::cout << "music root prefix receiving: 17 cases passed\n";
        return 0;
    } catch (const std::exception& error) {
        deny_allocations = false;
        std::cerr << error.what() << '\n';
        return 1;
    }
}
