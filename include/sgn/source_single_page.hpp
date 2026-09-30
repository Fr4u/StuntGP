#pragma once
#include "sgn/source_main_page.hpp"
namespace sgn::retail
{
std::uint32_t create_frontend_single_page_43e000(FrontendState &, std::uint32_t music_key,
                                                 SourceNavigationPageServices &);
void destroy_frontend_single_page_children_45a7f0(FrontendState &, std::uint32_t page);
std::uint32_t create_frontend_game_loop_page_43e000(FrontendState &, std::uint32_t music_key);
} // namespace sgn::retail
