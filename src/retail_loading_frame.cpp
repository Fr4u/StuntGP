#include "sgn/retail_loading_frame.hpp"
#include <bit>

namespace sgn::retail
{
void begin_loading_scene_433ee0(std::uint32_t &scene_active, LoadingFrameDevice &device)
{
    if (scene_active != 1)
    {
        scene_active = 1;
        if (device.begin_scene() != 0)
            throw FormatError("source433EE0 BeginScene failed; legacy error termination is unsupported");
    }
}
void end_loading_scene_433f20(std::uint32_t &scene_active, LoadingFrameDevice &device)
{
    if (scene_active != 0)
    {
        scene_active = 0;
        device.end_scene();
    }
}
void prepare_loading_surface_432a60(LoadingFrameDevice &device)
{
    if (device.surface_operation_432a60(0, 1) != 0)
        throw FormatError("source432A60 surface operation failed; legacy error termination is unsupported");
}
void finalize_loading_frame_431a80(std::uint32_t &finalized, LoadingFrameDevice &device)
{
    if (finalized != 1)
    {
        device.finalize_423430(0);
        finalized = 1;
    }
}
void clear_loading_frame_4328c0(LoadingFrameWorld world, std::uint32_t color, PolygonSubmissionSink &sink,
                                LoadingFrameDevice &device)
{
    const auto previous = world.scene_active;
    begin_loading_scene_433ee0(world.scene_active, device);
    const auto &viewport = world.quads.viewport;
    const auto width = std::bit_cast<std::uint32_t>(static_cast<float>(viewport.full_width));
    const auto height = std::bit_cast<std::uint32_t>(static_cast<float>(viewport.full_height));
    const auto converted = 0xff000000u | ((color & 0xffu) << 16) | (color & 0xff00u) | ((color >> 16) & 0xffu);
    std::array<PolygonSubmissionVertex, 4> vertices{}; // new host policy for undefined source UVs
    for (unsigned i = 0; i < 4; ++i)
    {
        auto &v = vertices[i].words;
        v[0] = (i == 1 || i == 2) ? width : 0;
        v[1] = i >= 2 ? height : 0;
        v[2] = 0;
        v[3] = 0x3f7fbe77;
        v[4] = converted;
        v[5] = 0xff000000;
    }
    // 432190 ECX28 falls through the default source jump-table case.
    sink.set_render_state(27, 0);
    sink.set_render_state(19, 2);
    sink.set_render_state(20, 1);
    sink.set_texture_stage_state(0, 2, 2);
    sink.set_render_state(23, 8);
    sink.set_render_state(28, 0);
    sink.set_texture(0, 0);
    sink.draw(6, 0x2c4, vertices, 0x14, viewport);
    if (world.depth_capability != 0)
    {
        sink.set_render_state(23, 7);
        sink.set_render_state(7, 1);
        world.submission.depth_scale_bits = 0x43000000;
        world.submission.depth_enabled = 1;
        world.submission.depth_write = 1;
        world.submission.depth_other = 0;
    }
    if (world.fog_a == 1 || world.fog_b == 1)
        sink.set_render_state(28, 1);
    if (previous == 0)
        end_loading_scene_433f20(world.scene_active, device);
}
void draw_loading_frame_4458b0(LoadingFrameWorld world, PolygonSubmissionSink &sink, LoadingFrameDevice &device)
{
    ++world.render_counter;
    prepare_loading_surface_432a60(device);
    begin_loading_scene_433ee0(world.scene_active, device);
    clear_loading_frame_4328c0(world, 0, sink, device);
    world.quads.selected_material = 0xffffffffu;
    draw_loading_overlay_before_442a00(world.quads, sink);
    draw_loading_tiles_445800(world.quads, sink);
    draw_loading_overlay_after_442a20(world.quads, sink);
    finalize_loading_frame_431a80(world.finalized, device);
    end_loading_scene_433f20(world.scene_active, device);
}
} // namespace sgn::retail
