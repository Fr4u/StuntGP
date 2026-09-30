// Authored receiving cases only. This executable has not been built or run.
#include "sgn/source_music_drive_type_prefix.hpp"
#include "sgn/source_music_period_host.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session.hpp"
#include <array>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <new>
#include <optional>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>

namespace { thread_local bool deny_allocations{}; }
void* operator new(std::size_t size) {
    if (deny_allocations) throw std::bad_alloc();
    if (void* p = std::malloc(size == 0 ? 1 : size)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

namespace {
using namespace sgn::retail;
using DriveSignature = UINT(WINAPI*)(LPCSTR);
struct DriveMarker {};
static_assert(std::is_same_v<decltype(&::GetDriveTypeA), DriveSignature>);
static_assert(!std::is_default_constructible_v<SourceMusicDriveRootAccess>);
void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
template<class Call> void rejects(Call&& call) {
    bool rejected = false;
    try { call(); } catch (const std::logic_error&) { rejected = true; }
    require(rejected, "checked borrow/host precondition did not reject");
}
struct Stats {
    AudioStreamState* actual{};
    std::array<char, 32> order{};
    std::size_t count{};
    std::uint32_t begin_result{};
    bool mutate_caps{}, mutate_begin{}, deny_after_begin{};
    char release_at{};
    std::shared_ptr<RaceSessionPlatform>* external{};
    std::optional<SourceMusicTimerCapsAccess> caps_access;
    void event(char kind) noexcept {
        if (count == order.size()) std::terminate();
        order[count++] = kind;
    }
    void release(char stage) { if (release_at == stage) external->reset(); }
};
struct Calls final : SourceMusicPeriodCalls {
    Stats& s;
    explicit Calls(Stats& stats) : s(stats) {}
    std::uint32_t begin(std::uint32_t period) noexcept override {
        s.event('B');
        if (period != 3 || s.actual->timer_period != 3) std::terminate();
        if (s.mutate_begin) s.actual->field_df54 = 0x7e2fff80u;
        s.release('B');
        if (s.deny_after_begin) deny_allocations = true;
        return s.begin_result;
    }
    std::uint32_t end(std::uint32_t period) noexcept override {
        if (period != 3) std::terminate();
        s.event('E');
        return 0;
    }
};
struct Caps final : SourceMusicTimerCapsReceiving {
    Stats& s;
    std::uint32_t result{};
    bool throw_after_minimum{};
    explicit Caps(Stats& stats) : s(stats) {}
    std::uint32_t time_get_dev_caps(SourceMusicTimerCapsAccess output, std::uint32_t bytes) override {
        s.event('C');
        require(bytes == 8, "capability size changed");
        s.caps_access = output;
        output.minimum(3);
        if (throw_after_minimum) throw std::runtime_error("explicit capability exception");
        output.maximum(10);
        if (s.mutate_caps) s.actual->field_df54 = 0x7e5c3a51u;
        s.release('C');
        return result;
    }
};
struct Drive final : SourceMusicDriveTypeReceiving {
    Stats& s;
    std::uint32_t result{3};
    std::array<std::uint8_t, 5> observed{};
    std::optional<SourceMusicDriveRootAccess> escaped;
    std::function<void(SourceMusicDriveRootAccess)> during;
    explicit Drive(Stats& stats) : s(stats) {}
    std::uint32_t get_drive_type_a(SourceMusicDriveRootAccess root) override {
        s.event('D');
        escaped = root;
        for (std::size_t i = 0; i != observed.size(); ++i) observed[i] = root.byte(i);
        s.release('D');
        if (during) during(root);
        return result;
    }
};
struct Fixture {
    Stats stats;
    std::shared_ptr<RaceSessionPlatform> platform = std::make_shared<RaceSessionPlatform>(
        SourceColdConstructionOnly{}, std::filesystem::path{});
    std::shared_ptr<Calls> calls = std::make_shared<Calls>(stats);
    std::shared_ptr<const SourceMusicPeriodReport> report;
    Caps caps{stats};
    Drive drive{stats};
    explicit Fixture(bool prepare = true) {
        stats.actual = &platform->state().audio_stream;
        stats.external = &platform;
        auto& s = *stats.actual;
        s.timer_capabilities.minimum() = 9;
        s.timer_capabilities.maximum() = 100;
        s.timer_period = 77;
        s.field_df54 = 0x7e5c3a43u;
        s.filename_tail.fill(0xabu);
        s.result = 0x89abcdefu;
        s.flags = 0x10203040u;
        s.timer = 0x76543210u;
        s.allocated = 0;
        if (prepare) report = prepare_receiving_music_period_owner(platform, calls);
    }
    SourceMusicDriveTypePrefixObservation run() {
        return receive_source_music_drive_type_prefix_45fda0(platform, caps, drive);
    }
};
auto unchanged(const AudioStreamState& s) {
    return std::tuple{s.field_df54, s.filename_tail, s.result, s.allocated, s.flags, s.timer};
}
void order(const Stats& s, const char* wanted, std::size_t size) {
    require(s.count == size, "call count differs");
    for (std::size_t i = 0; i != size; ++i) require(s.order[i] == wanted[i], "call order differs");
}

void result_partitions() {
    for (auto value : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 0x80000000u, 0xffffffffu}) {
        Fixture f;
        f.drive.result = value;
        const auto before = unchanged(*f.stats.actual);
        const auto out = f.run();
        require(out.minimum == 3 && out.maximum == 10 && out.selected_period == 3 && out.drive_type == value, "drive result was mapped or clamp changed");
        if (value == 2) {
            require(out.original_function_returned() && out.source_return == 0x80004005u && !out.next_source_site, "result2 did not return exact source bits");
        } else {
            require(!out.original_function_returned() && !out.source_return && out.next_source_site == 0x45fe30u, "non2 admitted later code or returned success/failure");
        }
        order(f.stats, "CBD", 3);
        require(unchanged(*f.stats.actual) == before, "drive leaf wrote source/result state");
        require(f.report->snapshot().pending_calls == 1 && f.report->snapshot().end_calls == 0, "leaf implicitly ended Begin");
        rejects([&] { (void)f.drive.escaped->byte(0); });
    }
}
void root_partitions() {
    struct Row { std::uint32_t filename; std::array<std::uint8_t, 5> root; };
    constexpr std::array<Row, 6> rows{{
        {0x7e434200u, {0,0,0,0,0}}, {0x7e430041u, {65,0,0,0,0}},
        {0x7e004241u, {65,66,0,0,0}}, {0x7e434241u, {65,66,67,0,0}},
        {0x7e2fff80u, {128,255,47,0,0}}, {0x7e5c3a43u, {67,58,92,0,0}}
    }};
    for (const auto& row : rows) {
        Fixture f;
        f.stats.actual->field_df54 = row.filename;
        const auto before = unchanged(*f.stats.actual);
        (void)f.run();
        require(f.drive.observed == row.root, "root bytes replaced, normalized or copied beyond count3");
        require(before == unchanged(*f.stats.actual), "root source/tail changed");
        order(f.stats, "CBD", 3); //empty and invalid-looking roots still reach the API seam
    }
}
void late_filename_after_both_providers() {
    Fixture f;
    f.stats.mutate_caps = f.stats.mutate_begin = true;
    (void)f.run();
    require(f.drive.observed == std::array<std::uint8_t,5>{128,255,47,0,0}, "root used filename before the actual Begin provider");
}
void root_is_separate_live_local() {
    Fixture f;
    f.drive.during = [&](SourceMusicDriveRootAccess root) {
        f.stats.actual->field_df54 = 0;
        f.stats.actual->filename_tail.fill(0);
        for (std::size_t i = 0; i != 5; ++i) require(root.byte(i) == f.drive.observed[i], "drive root aliases mutable filename instead of copied local");
        rejects([&] { (void)f.stats.caps_access->minimum(); });
    };
    (void)f.run();
    require(f.stats.actual->field_df54 == 0, "provider mutation was rolled back");
}
void capability_failure_and_exception() {
    for (bool throws : {false, true}) {
        Fixture f;
        f.caps.result = 97;
        f.caps.throw_after_minimum = throws;
        bool failed = false;
        try { (void)f.run(); } catch (const std::runtime_error&) { failed = true; }
        require(failed && !f.drive.escaped && f.stats.actual->timer_period == 77 && f.stats.actual->timer_capabilities.minimum() == 3, "capability failure reached drive or rolled back partial write");
        order(f.stats, "C", 1);
        require(f.report->snapshot().begin_calls == 0, "capability failure requested period");
    }
}
void missing_period_owner() {
    Fixture f(false);
    rejects([&] { (void)f.run(); });
    order(f.stats, "C", 1);
    require(!f.drive.escaped && f.stats.actual->timer_period == 3, "missing owner admitted drive or rolled back earlier store");
}
void ignored_begin_failure() {
    Fixture f;
    f.stats.begin_result = 97;
    f.drive.result = 2;
    require(f.run().source_return == 0x80004005u, "Begin result replaced drive branch");
    order(f.stats, "CBD", 3);
    require(f.report->snapshot().pending_calls == 1 && f.report->snapshot().begin_failures == 1, "failed reached Begin obligation lost");
}
void drive_exception_and_unwind() {
    for (bool release : {false, true}) {
        Fixture f;
        std::weak_ptr<RaceSessionPlatform> weak = f.platform;
        if (release) f.stats.release_at = 'D';
        f.drive.during = [&](SourceMusicDriveRootAccess root) {
            require(root.byte(0) == 67 && !weak.expired(), "owner/root died during drive");
            f.stats.actual->result = 0x11223344u;
            throw DriveMarker{};
        };
        bool failed = false;
        try { (void)f.run(); } catch (const DriveMarker&) { failed = true; }
        require(failed, "drive exception converted into a source result");
        rejects([&] { (void)f.drive.escaped->byte(0); });
        if (release) {
            require(weak.expired() && f.report->snapshot().finalized, "escaped root kept platform alive");
            order(f.stats, "CBDE", 4); //host teardown after unwind, not source End
        } else {
            require(f.stats.actual->result == 0x11223344u && f.report->snapshot().end_calls == 0, "exception rolled back provider write or auto-ended period");
            order(f.stats, "CBD", 3);
        }
    }
}
void retained_owner_and_expired_borrow() {
    for (char stage : {'C', 'B', 'D'}) {
        Fixture f;
        std::weak_ptr<RaceSessionPlatform> weak = f.platform;
        f.stats.release_at = stage;
        f.drive.during = [&](SourceMusicDriveRootAccess root) { require(!weak.expired() && root.byte(0) == 67, "owner not retained across earlier provider release"); };
        (void)f.run();
        require(weak.expired() && !f.platform && f.report->snapshot().finalized, "escaped borrow retained platform");
        order(f.stats, "CBDE", 4);
        rejects([&] { (void)f.drive.escaped->byte(0); });
        bool rejected = false;
        std::thread worker([&] { try { (void)f.drive.escaped->byte(0); } catch (const std::logic_error&) { rejected = true; } });
        worker.join();
        require(rejected, "foreign escaped borrow touched dead root");
    }
}
void active_borrow_bounds_and_thread() {
    Fixture f;
    f.drive.during = [&](SourceMusicDriveRootAccess root) {
        rejects([&] { (void)root.byte(5); });
        bool rejected = false;
        std::thread worker([&] { try { (void)root.byte(0); } catch (const std::logic_error&) { rejected = true; } });
        worker.join();
        require(rejected && root.byte(0) == 67, "foreign borrow changed active owner scope");
    };
    (void)f.run();
}
void null_and_foreign_entry() {
    Fixture f;
    bool null_rejected = false;
    try { (void)receive_source_music_drive_type_prefix_45fda0({}, f.caps, f.drive); }
    catch (const std::invalid_argument&) { null_rejected = true; }
    require(null_rejected, "null entry reached providers");
    bool foreign_rejected = false;
    std::thread worker([&] { try { (void)f.run(); } catch (const std::logic_error&) { foreign_rejected = true; } });
    worker.join();
    require(foreign_rejected && f.stats.count == 0 && f.stats.actual->timer_period == 77, "foreign entry touched source/providers");
}
void repeated_and_independent_owners() {
    Fixture f, other;
    other.stats.actual->field_df54 = 0x7e5c3a51u;
    (void)f.run();
    const auto old = *f.drive.escaped;
    (void)other.run();
    f.stats.actual->field_df54 = 0x7e5c3a44u;
    (void)f.run();
    require(f.drive.observed[0] == 68 && other.drive.observed[0] == 81, "root shared between calls/owners");
    rejects([&] { (void)old.byte(0); });
    order(f.stats, "CBDCBD", 6);
    require(f.report->snapshot().pending_calls == 2 && other.report->snapshot().pending_calls == 1, "repeated leaf reused prior admission");
}
void no_allocation_after_begin() {
    Fixture f;
    f.stats.deny_after_begin = true;
    bool failed = false;
    try { (void)f.run(); } catch (const std::bad_alloc&) { failed = true; }
    deny_allocations = false;
    require(!failed && f.drive.observed[0] == 67, "drive continuation allocated after reached Begin");
    order(f.stats, "CBD", 3);
}
void legacy_entry_preserved() {
    Fixture f;
    const auto out = receive_source_music_timer_prefix_45fda0(f.platform, f.caps);
    require(out.next_source_site == 0x45fe17 && !out.original_function_returned && !f.drive.escaped, "existing98f entry advanced into drive");
    order(f.stats, "CB", 2);
}
void nested_private_roots() {
    Fixture outer, inner;
    inner.stats.actual->field_df54 = 0x7e5c3a51u;
    outer.drive.during = [&](SourceMusicDriveRootAccess outer_root) {
        inner.drive.during = [&](SourceMusicDriveRootAccess inner_root) {
            require(outer_root.byte(0) == 67 && inner_root.byte(0) == 81, "nested calls alias one scratch/scope");
        };
        (void)inner.run();
        require(outer_root.byte(0) == 67, "inner return expired outer scope");
        rejects([&] { (void)inner.drive.escaped->byte(0); });
    };
    (void)outer.run();
    rejects([&] { (void)outer.drive.escaped->byte(0); });
}
}
int main() {
    try {
        result_partitions(); root_partitions(); late_filename_after_both_providers();
        root_is_separate_live_local(); capability_failure_and_exception(); missing_period_owner();
        ignored_begin_failure(); drive_exception_and_unwind(); retained_owner_and_expired_borrow();
        active_borrow_bounds_and_thread(); null_and_foreign_entry(); repeated_and_independent_owners();
        no_allocation_after_begin(); legacy_entry_preserved(); nested_private_roots();
        std::cout << "music drive type prefix receiving: 15 groups passed\n";
        return 0;
    } catch (const std::exception& error) {
        deny_allocations = false;
        std::cerr << error.what() << '\n';
        return 1;
    }
}
