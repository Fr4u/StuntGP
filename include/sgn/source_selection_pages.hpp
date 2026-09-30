#pragma once
#include "sgn/source_main_page.hpp"
namespace sgn::retail
{
std::uint32_t source_car_class_437cc0(std::uint32_t car);
std::uint32_t create_frontend_car_page_43e000(FrontendState &, std::uint32_t, SourceNavigationPageServices &);
void destroy_frontend_car_page_children_451a80(FrontendState &, std::uint32_t);
std::uint32_t create_frontend_track_page_43e000(FrontendState &, std::uint32_t, SourceNavigationPageServices &);
void destroy_frontend_track_page_children_45c640(FrontendState &, std::uint32_t);
void finalize_frontend_track_page_45c670(FrontendState &, std::uint32_t);
std::uint32_t create_frontend_start_page_43e000(FrontendState &, std::uint32_t, SourceNavigationPageServices &);
} // namespace sgn::retail
