#include "sgn/source_renderer_bootstrap.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session.hpp"
#include <bit>
namespace sgn::retail
{
void SessionWorldMaterials::initialize_source_format_431383(FrontendState &frontend)
{
    const auto platform = require_registry_live();
    if (platform != frontend.shared_platform())
        throw FormatError("431383 material owner belongs to a different frontend");
    state_.format = 1;
}
void initialize_source_renderer_431360(SourceRendererBootstrapContext c)
{
    auto &f = c.frontend;
    f.shared_platform()->require_owner_thread();
    if (!(f.shared_platform()->source_application_literals() == c.application.reference()))
        throw FormatError("431360 application owner differs from the frontend binding");
    auto &world = f.world();
    // EAX,ECX,ESI,EDI captured before source stores and all callbacks.
    const auto width = world.boot_config.display_width, height = world.boot_config.display_height;
    const auto adapter = world.boot_config.device_index, device = world.boot_config.driver_index;
    world.outer.device.flag_6244f0 = 1;
    c.materials.initialize_source_format_431383(f);
    c.application.mutable_model_memory_budget_477c30() = 0x3ffff8;
    world.viewport.width = std::bit_cast<std::int32_t>(width);
    world.viewport.height = std::bit_cast<std::int32_t>(height);
    reset_device_source_globals_42f300(world.outer.device, c.submission);
    c.calls.enumerate_adapters_42f6a0(f);
    c.calls.create_surfaces_42fd80(f, adapter);
    c.calls.create_depth_42ff20(f, adapter, device);
    c.calls.attach_depth_42fff0(f);
    auto fatal = [&]() {
        c.calls.fatal_431330(f);
        throw FormatError("431330 nonreturning exit returned");
    };
    if (c.calls.create_device_430020(f, adapter, device) == 0)
        fatal();
    c.calls.publish_selected_device_4313d5(f);
    if (c.calls.create_viewport_42f840(f) == 0)
        fatal();
    c.calls.create_white_material_430ff0(f);
    if (initialize_retail_device(c.calls.device_setup_world(f)) != 1)
        fatal();
    c.calls.query_gamma_432070(f);
    // 431350 is literally RET, no synthetic receiver call.
    c.calls.driver_restrictions_42f220(f);
    auto reset = c.calls.reset_context(f);
    if (&reset.frontend != &f)
        throw FormatError("431360 reset context has a foreign frontend");
    reset_source_renderer_431300(reset);
    // 6244F8 and624980/984 are caller42E6E0 stores, not part of431360.
}
} // namespace sgn::retail
