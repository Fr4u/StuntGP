#include "sgn/clock.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using Ns = std::chrono::nanoseconds;
constexpr std::int64_t billion = 1'000'000'000;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

// Deliberately not a physics model: an ordered, nonlinear tick fingerprint.
// Display callbacks never advance this state and cannot alter input selection.
struct Trace {
    std::uint64_t count{};
    std::uint64_t digest = 0x123456789abcdef0;
    void operator()(std::uint64_t tick) {
        require(tick == count, "nonsequential or duplicated tick");
        const auto input = (tick / 7) % 5;
        digest ^= tick + input * 0x9e3779b97f4a7c15ULL;
        digest = (digest << 17) | (digest >> 47);
        digest *= 0xbf58476d1ce4e5b9ULL;
        ++count;
    }
};

void expected(const sgn::TickClock& clock, const Trace& trace, std::int64_t elapsed) {
    // Analytic elapsed-time endpoint, independent of display frame count.
    const auto expected_ticks = static_cast<std::uint64_t>(elapsed / billion) * 60
        + static_cast<std::uint64_t>((elapsed % billion) * 60 / billion);
    require(clock.ticks() == expected_ticks && trace.count == expected_ticks, "simulation drift");
    const auto remainder = (elapsed % billion) * 60 % billion;
    require(std::abs(clock.alpha() - static_cast<double>(remainder) / static_cast<double>(billion)) < 1e-14,
            "interpolation phase drift");
    require(clock.alpha() >= 0 && clock.alpha() < 1, "interpolation escaped [0,1)");
}

std::uint64_t regular(std::int64_t numerator, std::int64_t denominator, std::int64_t seconds) {
    sgn::TickClock clock;
    Trace trace;
    std::int64_t previous = 0;
    const auto frames = seconds * numerator / denominator;
    const auto step = [&](std::int64_t now) {
        clock.advance(Ns(now - previous), [&](auto tick) { trace(tick); });
        previous = now;
    };
    for (std::int64_t frame = 1; frame <= frames; ++frame) {
        // Multiplication split to remain safe for a day at 2 kHz.
        const auto rational = frame * denominator;
        step((rational / numerator) * billion + (rational % numerator) * billion / numerator);
        if (frame % 1024 == 0) expected(clock, trace, previous);
    }
    step(seconds * billion);
    expected(clock, trace, seconds * billion);
    require(clock.discarded_ns() == 0, "normal display schedule dropped simulation time");
    return trace.digest;
}

void cadence_changes() {
    sgn::TickClock clock;
    Trace trace;
    std::int64_t elapsed = 0;
    std::uint32_t random = 129467;
    for (unsigned frame = 0; frame < 2'000'000; ++frame) {
        random = random * 1664525u + 1013904223u;
        const auto delta = static_cast<std::int64_t>(random % 250'000'001u);
        elapsed += delta;
        clock.advance(Ns(delta), [&](auto tick) { trace(tick); });
        if (frame % 1024 == 0) expected(clock, trace, elapsed);
        const auto before = trace.digest;
        for (unsigned view = 0; view < (random & 3); ++view) {
            [[maybe_unused]] const auto presentation_alpha = clock.alpha();
        }
        require(trace.digest == before, "presentation changed the tick trace");
    }
    expected(clock, trace, elapsed);
    require(clock.discarded_ns() == 0, "valid jitter lost time");
}

void boundaries() {
    sgn::TickClock clock;
    Trace trace;
    clock.advance(Ns(16'666'666), [&](auto tick) { trace(tick); });
    require(clock.ticks() == 0, "tick rounded down to an integer nanosecond");
    clock.advance(Ns(1), [&](auto tick) { trace(tick); });
    require(clock.ticks() == 1, "exact rational boundary skipped a tick");
    const auto phase = clock.alpha();
    try {
        clock.advance(Ns(-1), [&](auto tick) { trace(tick); });
        throw std::runtime_error("negative time accepted");
    } catch (const std::invalid_argument&) {}
    require(clock.alpha() == phase && clock.ticks() == 1, "negative delta mutated state");
    clock.suspend();
    require(clock.alpha() == 0 && clock.ticks() == 1, "suspension changed tick identity");
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    require(clock.advance(Ns(maximum), [&](auto tick) { trace(tick); }) == 15,
            "large stall exceeded bounded catch-up");
    require(clock.discarded_ns() == static_cast<std::uint64_t>(maximum - 250'000'000),
            "large stall lost discard telemetry");
    require(clock.ticks() == 16 && clock.alpha() == 0, "large delta overflowed phase");
    clock.advance(Ns(0), [&](auto tick) { trace(tick); });
    require(clock.ticks() == 16, "zero delta caused phantom tick");
}
}

int main() {
    try {
        constexpr std::array rates{4, 5, 7, 24, 30, 50, 59, 60, 75, 90, 100, 120, 144, 165,
                                   240, 360, 480, 500, 1000, 2000};
        const auto reference = regular(60, 1, 60);
        for (const auto hz : rates)
            require(regular(hz, 1, 60) == reference, "refresh changed ordered tick trace");
        for (const auto numerator : {24000, 30000, 60000, 120000, 144000})
            require(regular(numerator, 1001, 60) == reference, "fractional refresh changed trace");
        require(regular(60, 1, 86400) == regular(60000, 1001, 86400), "24 hour fractional cadence drift");
        cadence_changes();
        boundaries();
        std::cout << "{\"passed\":true,\"refresh_schedules\":25,\"max_hz\":2000,"
                     "\"long_run_seconds\":86400,\"jitter_intervals\":2000000,"
                     "\"physical_monitor_certification\":false,\"game_physics_parity\":false}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
