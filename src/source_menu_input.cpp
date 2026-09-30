#include "sgn/source_menu_input.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
void produce_frontend_menu_input_40d860(FrontendState &frontend)
{
    frontend.shared_platform()->require_owner_thread();
    auto &s = frontend.world();
    auto &input = s.source_input;
    auto &raw = input.acquisition;
    auto physical = menu_physical_producer_views(raw, s.menu_input);
    produce_menu_input_40d860({s.scene_mode, menu_keyboard_bytes(s.startup), physical,
                               source_menu_selectors(input.profiles), raw.source_device_pointer_576b6c});
}
void SourceMenuInputChildren::acquire_input_45f210(FrontendState &frontend)
{
    acquire_frontend_input_45f210(frontend, keyboard_, direct_input_);
}
void SourceMenuInputChildren::update_controls_40d860(FrontendState &frontend)
{
    produce_frontend_menu_input_40d860(frontend);
}
} // namespace sgn::retail
