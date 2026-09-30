#include "sgn/host_menu_arrow.hpp"
#include "sgn/host_wide_layout.hpp"
#include "sgn/host_wide_overlay.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <stdexcept>

// NEW HOST (R29) procedural scroll arrow; see host_menu_arrow.hpp.
namespace sgn::host_menu_arrow
{
namespace
{
struct Point
{
    float x{}, y{};
};
struct Colour
{
    float r{}, g{}, b{}, a{};
};
// BoxBack "PAUSE" lettering, measured from the texels (overlay.wad
// graphics24\overlays\boxback.pc rows 4..44): red top, orange middle, yellow
// bottom; outline navy (0..16, 0..16, 16..64).
constexpr Colour top_red{255.0f, 6.0f, 3.0f, 255.0f};
constexpr Colour middle_orange{255.0f, 128.0f, 6.0f, 255.0f};
constexpr Colour bottom_yellow{248.0f, 222.0f, 24.0f, 255.0f};
constexpr Colour outline_navy{8.0f, 8.0f, 40.0f, 255.0f};
constexpr Colour shadow_black{0.0f, 0.0f, 0.0f, 140.0f};
constexpr float height_ratio = 0.62f;  // chunky: height = 0.62 * width
constexpr float italic_skew = 0.18f;   // x shift per unit of height, like the italic lettering
constexpr float outline_ratio = 0.09f; // outline thickness relative to the width
constexpr float shadow_ratio = 0.10f;  // drop shadow offset (right, down) relative to the width

std::uint32_t pack(const Colour &c)
{
    const auto channel = [](float v) { return static_cast<std::uint32_t>(std::lround(std::clamp(v, 0.0f, 255.0f))); };
    return (channel(c.a) << 24) | (channel(c.r) << 16) | (channel(c.g) << 8) | channel(c.b);
}
Colour mix(const Colour &a, const Colour &b, float t)
{
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
}
Colour transparent(Colour c)
{
    c.a = 0.0f;
    return c;
}
// Gradient by height inside the fill's vertical extent [top, bottom].
Colour gradient(float y, float top, float bottom)
{
    const float t = std::clamp((y - top) / (bottom - top), 0.0f, 1.0f);
    return t < 0.5f ? mix(top_red, middle_orange, t * 2.0f) : mix(middle_orange, bottom_yellow, t * 2.0f - 1.0f);
}
// Convex polygon offset by `d` along the outward mitred vertex normals.
std::vector<Point> offset(const std::vector<Point> &polygon, float d)
{
    float area = 0;
    for (std::size_t i = 0; i < polygon.size(); ++i)
    {
        const auto &a = polygon[i];
        const auto &b = polygon[(i + 1) % polygon.size()];
        area += a.x * b.y - b.x * a.y;
    }
    const float orientation = area > 0 ? 1.0f : -1.0f;
    std::vector<Point> result(polygon.size());
    for (std::size_t i = 0; i < polygon.size(); ++i)
    {
        const auto &previous = polygon[(i + polygon.size() - 1) % polygon.size()];
        const auto &current = polygon[i];
        const auto &next = polygon[(i + 1) % polygon.size()];
        const auto normal = [&](const Point &a, const Point &b) {
            const float dx = b.x - a.x, dy = b.y - a.y, length = std::hypot(dx, dy);
            if (length <= 0)
                throw std::logic_error("host arrow polygon has a zero-length edge");
            return Point{orientation * dy / length, -orientation * dx / length};
        };
        const auto n1 = normal(previous, current), n2 = normal(current, next);
        const float cosine = n1.x * n2.x + n1.y * n2.y;
        const float scale = d / (1.0f + cosine); // mitre: (n1 + n2) * d / (1 + n1.n2)
        result[i] = {current.x + (n1.x + n2.x) * scale, current.y + (n1.y + n2.y) * scale};
    }
    return result;
}
class Builder
{
    Mesh &mesh_;

  public:
    explicit Builder(Mesh &mesh) : mesh_(mesh) {}
    std::uint16_t vertex(Point p, const Colour &c)
    {
        if (mesh_.vertices.size() >= 0xffffu)
            throw std::length_error("host arrow mesh exceeds 16-bit indices");
        mesh_.vertices.push_back({p.x, p.y, pack(c)});
        return static_cast<std::uint16_t>(mesh_.vertices.size() - 1);
    }
    // Every triangle is emitted with the winding of the 4291F0 glyph fans
    // (clockwise on the y-down screen), whatever the cull mode.
    void triangle(std::uint16_t a, std::uint16_t b, std::uint16_t c)
    {
        const auto &p = mesh_.vertices[a], &q = mesh_.vertices[b], &r = mesh_.vertices[c];
        const float cross = (q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x);
        if (cross == 0.0f)
            return;
        if (cross > 0.0f)
            mesh_.indices.insert(mesh_.indices.end(), {a, b, c});
        else
            mesh_.indices.insert(mesh_.indices.end(), {a, c, b});
    }
    // Convex polygon (fan) with per-vertex colours plus its feather band:
    // colours fade to alpha 0 at the polygon offset outwards by `feather`.
    void shape(const std::vector<Point> &polygon, const std::vector<Colour> &colours, float feather)
    {
        std::vector<std::uint16_t> inner, outer;
        for (std::size_t i = 0; i < polygon.size(); ++i)
            inner.push_back(vertex(polygon[i], colours[i]));
        const auto fringe = offset(polygon, feather);
        for (std::size_t i = 0; i < polygon.size(); ++i)
            outer.push_back(vertex(fringe[i], transparent(colours[i])));
        for (std::size_t i = 1; i + 1 < polygon.size(); ++i)
            triangle(inner[0], inner[i], inner[i + 1]);
        for (std::size_t i = 0; i < polygon.size(); ++i)
        {
            const auto j = (i + 1) % polygon.size();
            triangle(inner[i], inner[j], outer[j]);
            triangle(inner[i], outer[j], outer[i]);
        }
    }
};
} // namespace

Mesh build(Direction direction, float center_x, float center_y, float width, float feather)
{
    if (!(width > 0.0f) || !(feather > 0.0f))
        throw std::invalid_argument("host arrow needs a positive width and feather");
    const float height = width * height_ratio, half_w = width * 0.5f, half_h = height * 0.5f;
    const float tip_y = direction == Direction::up ? center_y - half_h : center_y + half_h;
    const float base_y = direction == Direction::up ? center_y + half_h : center_y - half_h;
    const float mid_y = (tip_y + base_y) * 0.5f;
    const auto skew = [&](float x, float y) { return Point{x + italic_skew * (center_y - y), y}; };
    // Five-point convex outline (tip, both edge midpoints, both base corners)
    // so the three-stop gradient is exact along every edge.
    const float mid_half = half_w * 0.5f;
    std::vector<Point> fill{skew(center_x, tip_y), skew(center_x + mid_half, mid_y), skew(center_x + half_w, base_y),
                            skew(center_x - half_w, base_y), skew(center_x - mid_half, mid_y)};
    const float top = center_y - half_h, bottom = center_y + half_h;
    const float outline = width * outline_ratio, shadow = width * shadow_ratio;
    Mesh mesh;
    Builder builder(mesh);
    const auto outer = offset(fill, outline);
    std::vector<Point> shadow_shape = outer;
    for (auto &p : shadow_shape)
    {
        p.x += shadow;
        p.y += shadow;
    }
    builder.shape(shadow_shape, std::vector<Colour>(shadow_shape.size(), shadow_black), feather * 1.5f);
    builder.shape(outer, std::vector<Colour>(outer.size(), outline_navy), feather);
    // Fill: the fan above would blend the middle stop away, so the gradient is
    // drawn as three triangles (tip band and base band) plus the fill feather.
    std::vector<Colour> fill_colours;
    for (const auto &p : fill)
        fill_colours.push_back(gradient(p.y, top, bottom));
    std::vector<std::uint16_t> v;
    for (std::size_t i = 0; i < fill.size(); ++i)
        v.push_back(builder.vertex(fill[i], fill_colours[i]));
    builder.triangle(v[0], v[1], v[4]); // tip .. middle band
    builder.triangle(v[4], v[1], v[2]); // middle .. base band
    builder.triangle(v[4], v[2], v[3]);
    const auto fringe = offset(fill, feather);
    std::vector<std::uint16_t> outer_index;
    for (std::size_t i = 0; i < fill.size(); ++i)
        outer_index.push_back(builder.vertex(fringe[i], transparent(fill_colours[i])));
    for (std::size_t i = 0; i < fill.size(); ++i)
    {
        const auto j = (i + 1) % fill.size();
        builder.triangle(v[i], v[j], outer_index[j]);
        builder.triangle(v[i], outer_index[j], outer_index[i]);
    }
    return mesh;
}
float one_pixel_feather() noexcept
{
    const auto pixels = host_wide::current().pixels_per_unit;
    return pixels > 0.0f && std::isfinite(pixels) ? 1.0f / pixels : 1.0f;
}
void draw(retail::PolygonSubmissionSink &sink, const retail::ViewportState &viewport, std::span<const Mesh> meshes,
          float depth, const Restore &restore)
{
    const auto map = host_wide_overlay::overlay_mapping(viewport);
    // The 432190 alpha mode 0x48 (as an alpha 432E00 quad): blend on, SRCALPHA /
    // INVSRCALPHA, stage 0 argument state as 432190 leaves it.
    sink.set_render_state(27, 1);
    sink.set_render_state(19, 5);
    sink.set_render_state(20, 6);
    sink.set_texture_stage_state(0, 2, 2);
    sink.set_render_state(24, 1); // ALPHAREF 1: keep the feathered texels
    sink.set_texture(0, 0);       // untextured: the stage passes the diffuse colour
    std::vector<retail::PolygonSubmissionVertex> vertices;
    for (const auto &mesh : meshes)
    {
        vertices.clear();
        for (const auto &v : mesh.vertices)
        {
            retail::PolygonSubmissionVertex out{};
            auto &w = out.words;
            w[0] = std::bit_cast<std::uint32_t>(
                static_cast<float>((double(v.x) - 320.0) * double(map.scale_x) + double(map.center_x)));
            w[1] = std::bit_cast<std::uint32_t>(
                static_cast<float>((double(v.y) - 240.0) * double(map.scale_y) + double(map.center_y)));
            w[2] = w[3] = std::bit_cast<std::uint32_t>(depth);
            w[4] = v.argb;
            w[5] = 0xff000000u; // no fog, as 432E00
            vertices.push_back(out);
        }
        if (!mesh.indices.empty())
            sink.draw_indexed(4, 0x2c4, vertices, mesh.indices, 0x1c, viewport);
    }
    sink.set_render_state(27, 0); // as 432E00 after an alpha quad
    sink.set_render_state(24, restore.alpha_ref);
    sink.set_texture(0, restore.texture_token);
}
} // namespace sgn::host_menu_arrow
