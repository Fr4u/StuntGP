#pragma once
#include "sgn/host_graphics_menu_model.hpp" //R24: shared rows/strings/session (begin_session, report)
#include "sgn/source_main_page_navigation.hpp"
#include "sgn/retail_text_submission.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <string_view>

// NEW HOST (R23) in-game GRAPHICS options. Not recovered game behaviour: the
// 2001 original has no such page. It edits only sgn::host_graphics::Settings
// (presentation), never a simulation value, a persistence/save word or a
// language resource. The page reuses the recovered frontend machinery as is:
// the 43E000-style 24-byte page object and the 200-byte MenuText child 43C860/
// 43C980 in the same 5FD258 page pool, 43DFA0 push, 43EEF0 pop, 43EE40 publish,
// 438FF0/439040 text boxes and 440890 sounds. Its descriptor, rows and page
// name live in FrontendSourceStorage::host_menu_r23 at host addresses 7E230000..
// that no PE section maps (see retail_frontend_state.hpp).
namespace sgn::retail
{
// Host identities. None is a PE address; all are rejected by source tables.
inline constexpr std::uint32_t host_graphics_page_close_r23 = 0x7e23c000u; // page+0C
inline constexpr std::uint32_t host_graphics_page_event_r23 = 0x7e23c010u; // page+10
// Options row ID of the appended GRAPHICS row (positive, outside 454DA0's 0..5/99).
inline constexpr std::uint32_t host_graphics_options_row_id_r23 = 0x7e23u;

// 454CE0 extension: fills the host copy of the Options descriptor whose rows
// are the rows of the source-selected table (source_descriptor+28, as written
// by 454CE0) followed by the GRAPHICS row and a zero terminator row. Returns
// the host descriptor address for 43C860. Source words are only read.
std::uint32_t build_host_options_descriptor_r23(FrontendState &, std::uint32_t source_descriptor);
// 454DA0 event16 extension: pushes the GRAPHICS page for the GRAPHICS row ID.
// False for every other row ID (they stay with the source switch).
bool route_host_graphics_option_r23(FrontendState &, std::uint32_t selection, SourceNavigationPageServices &,
                                    CampaignScoreMusic &);
// Page close (identity host_graphics_page_close_r23): releases the MenuText
// child like 454D80 and saves the graphics settings file.
void destroy_host_graphics_page_r23(FrontendState &, std::uint32_t page);
// Page events (identity host_graphics_page_event_r23).
std::uint32_t dispatch_host_graphics_page_r23(FrontendState &, std::uint32_t page, std::uint32_t event,
                                              SourceNavigationPageServices &, CampaignScoreMusic &,
                                              SourceMainPageServices &);
// 43C980 event3 row/description text. For a host text identity (the GRAPHICS
// rows and help lines) draws the localized text through the same 439040 box as
// 438FF0 and returns true; returns false for every source language text ID.
bool draw_host_menu_text_438ff0_r23(FrontendState &, std::uint32_t ecx, std::uint32_t edx,
                                    const std::array<std::uint32_t, 11> &stack);
// NEW HOST (R29): draws the GRAPHICS page's scroll arrows requested by this
// frame's event 3 (centred above the first / below the last shown row, only
// when rows exist in that direction). Called by 437EC0 after 437ED0 consumed
// the lists and before 437E80 clears them; a no-op on frames without a request.
void draw_host_graphics_page_arrows_r29(FrontendState &, PolygonSubmissionSink &, const TextSubmission &);
} // namespace sgn::retail
