#pragma once
#include "sgn/source_main_page_navigation.hpp"
#include "sgn/source_menu_page_draw.hpp"
namespace sgn::retail
{
// Existing438A60 at fixed source scale1.25 and its mutable text-owner borrow.
// Adds actual61BDCC..61BECF award buffer, adjacent to campaign_result61BED0.
std::span<std::uint8_t> borrow_frontend_panel_text(FrontendState &, std::uint32_t identity);
double measure_frontend_panel_text_438a60(FrontendState &, std::uint32_t identity);
// Borrow actual source rows or a caller's local {color,text} DWORD pairs.
// Reads are lazy in both45BE20 passes; this does not snapshot row contents.
struct SourceTextPanelRows
{
    std::span<const std::uint32_t> local_words;
    std::uint32_t source_address{};
};
// Full finite PC53/nearest45BE20; stack={x,y,width,gap}, source float bits.
// Text identities use the accepted language representation or the actual
// 61B9C8/61BDCC campaign or61C0C0 stunt text owners. Other raw pointers are unbound.
void draw_frontend_text_panel_45be20(FrontendState &, SourceTextPanelRows, std::uint32_t count,
                                     std::array<std::uint32_t, 4> stack, SourceMenuPageDrawServices &);
std::uint32_t create_frontend_text_splash_page_43e000(FrontendState &, std::uint32_t key,
                                                      SourceNavigationPageServices &, SourceMainPageServices &);
void destroy_frontend_text_splash_page_45be00(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_text_splash_page_45bf40(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                        SourceNavigationPageServices &, CampaignScoreMusic &,
                                                        SourceMainPageServices &, SourceMenuPageDrawServices &);
} // namespace sgn::retail
