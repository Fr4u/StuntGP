#pragma once
#include "sgn/retail_race_hud_submission.hpp"
#include "sgn/retail_ui_line.hpp"

namespace sgn::retail
{
// Persistent source UI owners for a shared frontend/race world. The existing39 scalar
// words and nine sparse line-scratch words start at the pinned PE zeros; this is not a per-frame reset policy.
// See docs/SERIES6-SESSION-UI-OWNERSHIP.md for source writers and lifetimes.
struct SessionUiState
{
    SessionUiState() = default;
    SessionUiState(const SessionUiState &) = delete;
    SessionUiState &operator=(const SessionUiState &) = delete;
    SessionUiState(SessionUiState &&) = delete;
    SessionUiState &operator=(SessionUiState &&) = delete;
    std::array<std::uint32_t, 35> descriptor{}; // 576C38..576CC3, loading and HUD
    std::uint32_t selected_material{};          // 61C5E4, every UI producer borrows this
    std::uint32_t loading_tile_base{};          // 61B628
    std::uint32_t loading_fade{};               // 61B630
    std::uint32_t second_pause{};               // 612958; not frame/audio pause
    UiLineClipScratch line_clip;                // 592018/1C/20,592030/34/38,592040/44/48 only
    RetailFontResources fonts;                  // full source font records, metrics, map, selectors
    RaceHudStarPhases stars;                    // 485258+12*i; retained across HUD/controller calls
};
} // namespace sgn::retail
