#pragma once
#include <array>
#include <cstdint>

namespace sgn::retail
{
// Sole persistent owners for424AA0/424C30/425070/4250A0. Origin and depth_bits
// are inputs owned here, supplied by the platform's screen initialization.
struct ViewportState
{
    std::int32_t width{}, height{};                                                   // 576B80/84
    std::int32_t center_pixel_x{}, center_pixel_y{};                                  // 576B8C/90
    float center_x{}, center_y{}, scale_x{}, scale_y{}, right_edge{}, bottom_edge{};  // 576B94..A8
    std::int32_t min_x{}, min_y{}, max_x{}, max_y{}, depth_bits{}, bytes_per_pixel{}; // 576BAC..C0
    std::int32_t full_width{}, full_height{};                                         // 576BC4/C8
    std::uint32_t cropped{}, selector{}; // 576BD8/DC;576BE0 belongs to session.split_setting
    float origin_x{}, origin_y{};        // 576BE4/E8
    std::array<float, 4> clip_virtual{}, clip_screen{}, clip_expanded{}; // 576C04..30, x0/x1/y0/y1
    std::uint32_t applied_selector{}, requested_selector{};              // 624988/8C
    float ui_scale_x{}, ui_scale_y{};                                    // 624990/94
};
void initialize_viewport(ViewportState &, std::uint32_t &split_setting);         // full424AA0 and4250A0; borrowed576BE0
void select_viewport(ViewportState &, std::int32_t selector);                    // full424C30 incl425070/4250A0
void update_viewport_ui_scale(ViewportState &);                                  // 425070
void set_viewport_clip(ViewportState &, float x0, float y0, float x1, float y1); // 4250A0
// 42E870 prefix gate only. Rendering/entity stages remain owned by their caller.
void apply_requested_viewport(ViewportState &);
} // namespace sgn::retail
