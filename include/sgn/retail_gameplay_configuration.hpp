#pragma once
#include "sgn/assets.hpp"
#include <array>

namespace sgn::retail
{
// The indexed source arrays are single world owners, not a second set of
// engine/performance/stunt settings. Consumers construct borrowed call views.
// Descriptor order and remaining bytes are mutable source parser state: a
// repeated load must not silently restore the original descriptor table.
struct GameplayRules
{
    std::array<float, 31> values{};                   // 60F2D0..60F348, index0 cleared but unused
    std::array<std::uint32_t, 30> descriptor_order{}; // 482F40 table row identities
    std::uint32_t pending{1}, remaining_bytes{124};   // 482F38/3C, pinned initial words
    GameplayRules();
};
struct ScoreRules
{
    std::array<std::uint32_t, 37> values{};           // 60D688..60D718, index0 cleared but unused
    std::array<std::uint32_t, 36> descriptor_order{}; // 482720 table row identities
    std::uint32_t pending{1}, remaining_bytes{148};   // 482718/1C, pinned initial words
    ScoreRules();
};
// Complete caller state transitions443BD0/445620 with bounded text from the
// real resource reader. CSV token/line semantics and descriptor removal retained.
void load_gameplay_text(std::string_view, GameplayRules &);
void load_score_text(std::string_view, ScoreRules &);
void load_gameplay_configuration(const AssetStore &, GameplayRules &);
void load_score_configuration(const AssetStore &, ScoreRules &);
void ensure_gameplay_configuration(const AssetStore &, GameplayRules &); // 4457E0
void ensure_score_configuration(const AssetStore &, ScoreRules &);       // 443D70
} // namespace sgn::retail
