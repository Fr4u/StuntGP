#pragma once
#include "sgn/source_menu_navigation.hpp"
#include "sgn/retail_frontend_page_pool.hpp"

namespace sgn::retail
{
// The same frontend.source.page_pool owns parent32B and two MenuText200B objects.
// Source addresses/callback identities are uint32 values, never native pointers.
std::uint32_t create_frontend_main_page_43e000(FrontendState &, std::uint32_t music_key,
                                               SourceNavigationPageServices &);
void destroy_frontend_main_page_children_4531d0(FrontendState &, std::uint32_t page);
// Full437100/456000 count over source input IDs1..4; same existing owners.
std::uint32_t count_frontend_players_437100(FrontendState &);
void refresh_frontend_main_page_4531a0(FrontendState &);
std::uint32_t initialize_frontend_menu_text_43c860(FrontendState &, std::uint32_t object, std::uint32_t descriptor,
                                                   SourceNavigationPageServices &);
void select_frontend_menu_text_43c930(FrontendState &, std::uint32_t object, std::uint32_t selection,
                                      SourceNavigationPageServices &);
} // namespace sgn::retail
