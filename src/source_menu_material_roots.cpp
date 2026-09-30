#include "sgn/session_world_materials.hpp"
#include "sgn/race_session.hpp"
#include "sgn/source_script_path_formatter.hpp"
namespace sgn::retail
{
void SessionWorldMaterials::initialize_source_material_roots_42e3e0(SourceScriptPathCall &call)
{
    const auto platform = require_registry_live();
    const auto app = platform->source_application_literals();
    const auto memory = app.read_model_memory_budget_42e3e0(); // 42E3E0, before either store.
    if (initialize_source_material_limits_42e3e0(memory))      // Models' SAME6249A0/6249A4.
        (void)format_source_graphics_root_42e40f(platform->state().boot_config, app, call);
    // 42E417: CF skips format/destination reads entirely; the frame stays unused.
    // Successful CRT frame ends at42E414; stack cleanup has no persistent effect.
}
void SessionWorldMaterials::initialize_source_material_roots_42e3e0()
{
    SourceScriptPathCall call;
    initialize_source_material_roots_42e3e0(call);
}
SourceGraphicsRoots SessionWorldMaterials::source_graphics_roots() const &
{
    const auto platform = require_registry_live();
    return source_graphics_roots_from_config(platform->state().boot_config);
}
} // namespace sgn::retail
