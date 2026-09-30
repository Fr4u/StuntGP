#include "sgn/retail_vehicle_strip.hpp"
namespace sgn::retail
{
void clear_vehicle_strip_headers(VehicleStripHeaders &headers) noexcept
{
    headers = {};
}
const VehicleStripTransform &vehicle_strip_transform() noexcept
{
    static constexpr VehicleStripTransform transform{
        {0, 0, 0, 0}, {1, 1, 1, 0}, {0, 0, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}};
    return transform;
}
void initialize_strip_geometry(VehicleStripGeometry &g, VehicleStripBinding &binding)
{
    // Six source allocations are newly zeroed; normals/texture globals are
    // existing storage and have different preservation rules.
    g.polygons = {};
    g.corner_indices = {};
    g.vertices = {};
    g.corners = {};
    g.part_flags = 1;
    g.model_field20 = g.model_field22 = g.part_field20 = g.part_field22 = 0;
    g.model_flags_02 = 1;
    g.part_flags_02 = 0;
    g.outer_transform = nullptr; // 41C607/41C692/41C696
    g.polygon_count = g.corner_count = g.vertex_count = 0;
    g.polygons[0] = {4, 5, 0x70, 0x50, 0, 0}; // 474818
    g.corners[0] = {1, 0, 0, 0x37, 0x3737};
    g.corners[1] = {0, 0, 1, 0x37, 0x3737};
    g.corners[2] = {1, 2, 2, 0x37, 0x3737};
    g.corners[3] = {0, 2, 3, 0x37, 0x3737}; // 4747d8
    g.vertices[0] = {0, 0, -50, 0};
    g.vertices[1] = {0, 0, 50, 0};
    g.vertices[2] = {100, 0, -50, 0};
    g.vertices[3] = {100, 0, 50, 0};
    g.bound_index = 0; // 41C651 publishes5712D8 ->5712E0;5712EC is retained
    g.texture_names = {};
    g.normals[0].x = 0;
    g.normals[0].y = 1;
    g.normals[0].z = 0;
    binding.bind(g);
}
void expand_strip_geometry(VehicleStripGeometry &g, VehicleStripBinding &binding)
{
    initialize_strip_geometry(g, binding);
    for (unsigned j = 0; j < 4; ++j)
        g.corners[j].color = 0x00ffffff;
    g.polygons[0].texture = 5;
    g.polygons[0].flags = 0x70;
    for (unsigned q = 1; q < 128; ++q)
    {
        const auto first = static_cast<std::uint16_t>(q * 4);
        for (unsigned j = 0; j < 4; ++j)
        {
            g.corners[q * 4 + j] = g.corners[j];
            g.vertices[q * 4 + j] = g.vertices[j];
            g.corners[q * 4 + j].vertex = static_cast<std::uint16_t>(first + j);
        }
        g.corner_indices[q * 4] = static_cast<std::uint16_t>(first + 2);
        g.corner_indices[q * 4 + 1] = first;
        g.corner_indices[q * 4 + 2] = static_cast<std::uint16_t>(first + 1);
        g.corner_indices[q * 4 + 3] = static_cast<std::uint16_t>(first + 3);
        g.normals[q] = g.normals[0];
        g.polygons[q] = g.polygons[0];
        g.polygons[q].normal = static_cast<std::uint16_t>(q);
        g.polygons[q].first_corner = first;
    }
    // Source intentionally leaves the first four indices and all active draw
    // counts zero. Replacing them with a conventional quad would alter state.
    g.polygon_count = g.corner_count = g.vertex_count = 0;
}
void initialize_vehicle_strip(VehicleStripGeometry &geometry, VehicleStripHeaders &headers,
                              VehicleStripBinding &binding)
{
    geometry.field_5712f8 = 1;
    geometry.field_5712f0 = 0;
    geometry.field_5712f4 = 1;
    expand_strip_geometry(geometry, binding);
    clear_vehicle_strip_headers(headers);
}
} // namespace sgn::retail
