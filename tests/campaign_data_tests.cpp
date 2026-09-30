#include "sgn/retail_campaign_data.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace sgn::retail;
struct Fixture {
    std::array<std::byte, campaign_data_size> rules;
    std::array<std::byte, campaign_numeric_token_size> scratch;
    std::uint32_t cursor = 0;
    Fixture() { rules.fill(std::byte{0xA5}); scratch.fill(std::byte{0xA5}); }
    CampaignDataViews view() { return {rules, scratch}; }
};
unsigned checks = 0;
void check(bool value) { if (!value) throw std::runtime_error("campaign host check failed at " + std::to_string(checks)); ++checks; }
std::uint32_t word(const Fixture& f, std::size_t at = 0) {
    std::uint32_t result = 0; for (unsigned i = 0; i < 4; ++i) result |= std::uint32_t{std::to_integer<unsigned char>(f.rules[at + i])} << (i * 8u); return result;
}
void reject(Fixture& f, std::string_view text) {
    const auto before = f; bool rejected = false;
    try { parse_campaign_data_435b60(f.view(), text, f.cursor); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected && f.rules == before.rules && f.scratch == before.scratch && f.cursor == before.cursor);
}
}
int main() {
    try {
        Fixture f; const std::string partial = "IgnoredLabel,42\r\r\n";
        check(parse_campaign_data_435b60(f.view(), partial, f.cursor) == 1);
        check(word(f) == 42 && f.cursor == partial.size());
        check(std::all_of(f.rules.begin() + 4, f.rules.end(), [](auto b) { return b == std::byte{0xA5}; }));
        check(f.scratch[0] == std::byte{0});
        f.cursor = 0; parse_campaign_data_435b60(f.view(), "NumberOfRounds,invalid", f.cursor); check(word(f) == 42);
        f = Fixture{}; const std::string retained = "9876";
        for (std::size_t i = 0; i < retained.size(); ++i) f.scratch[i] = static_cast<std::byte>(retained[i]);
        f.scratch[4] = std::byte{0}; parse_campaign_data_435b60(f.view(), "NumberOfRounds,\"12", f.cursor);
        check(word(f) == 1276); // Unterminated numeric token scans the original retained tail.
        for (const auto& text : {std::string(65, 'X') + ",1", std::string("N,") + std::string(256, '9'),
                                  std::string("N,7\r\n") + std::string(65, 'X') + ",1", std::string("N,\"12")}) {
            f = Fixture{}; reject(f, text);
        }
        f = Fixture{}; f.cursor = 2; reject(f, "X");
        f = Fixture{}; const auto before = f;
        bool rejected = false;
        try { parse_campaign_data_435b60({f.rules, std::span<std::byte, campaign_numeric_token_size>{f.rules.data(), campaign_numeric_token_size}}, "N,1", f.cursor); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected && f.rules == before.rules && f.scratch == before.scratch && f.cursor == 0);
        rejected = false;
        try { parse_campaign_data_435b60(f.view(), {reinterpret_cast<const char*>(f.rules.data()), 10}, f.cursor); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected && f.rules == before.rules && f.scratch == before.scratch && f.cursor == 0);
        std::cout << "{\"passed\":true,\"checks\":" << checks << ",\"host_rejections_atomic\":true,\"gpu_used\":false}\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
