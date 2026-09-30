#include "sgn/source_menu_camera_geometry.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session.hpp"
#include "sgn/source_camera_frame_consumers.hpp"
#include "sgn/retail_world_view_prefix.hpp"
#include "sgn/retail_camera_vehicle.hpp"
namespace sgn::retail
{
namespace
{
RaceSessionState &world_for_menu(FrontendState &frontend)
{
    const auto &platform = frontend.shared_platform();
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry())
        throw SourceAllocationError(0x441d70, "menu camera binding requires source platform");
    auto &world = frontend.world();
    if (&world != &platform->state())
        throw SourceAllocationError(0x441d70, "menu camera platform/world identity differs");
    if (world.scene_mode != 2)
        throw SourceAllocationError(0x441d70, "menu camera binding requires source scene2");
    return world;
}
} // namespace
void prepare_camera_402e90(FrontendState &frontend)
{
    (void)world_for_menu(frontend); // retain the real scene2 menu admission
    prepare_source_camera_view_402e90(frontend.shared_platform());
}
void prepare_geometry_44fd60(FrontendState &frontend)
{
    auto &w = world_for_menu(frontend);
    reset_view_matrix_cursor(w.view_matrices); // 6227CC=61C7CC native offset0;6227D0=0
}
} // namespace sgn::retail
