#include "sgn/retail_track_part_preparation.hpp"
#include "sgn/vehicle_tables.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_word;
float rounded(double v)
{
    return static_cast<float>(v);
}
unsigned polygon_corner(const std::array<std::uint8_t, 8> &polygon)
{
    return unsigned(polygon[6]) | (unsigned(polygon[7]) << 8);
}
void initialize(TrackPartPreparationScratch &s)
{
    s.values = {};
    s.values[0].x = s.values[0].y = 10000;
    s.values[2].x = -10000;
    s.values[2].y = 10000;
    s.values[4].x = 10000;
    s.values[4].y = -10000;
    s.values[6].x = s.values[6].y = -10000;
}
void scan(TrackPartPreparationView w, std::uint16_t normal)
{
    initialize(w.scratch);
    auto &a = w.scratch.values[0];
    auto &au = w.scratch.values[1];
    auto &b = w.scratch.values[2];
    auto &bu = w.scratch.values[3];
    auto &c = w.scratch.values[4];
    auto &cu = w.scratch.values[5];
    auto &d = w.scratch.values[6];
    auto &du = w.scratch.values[7];
    for (std::size_t i = 0; i < w.vertices.size(); ++i)
    {
        const auto &v = w.vertices[i];
        const auto &uv = w.coordinates[i];
        if (static_cast<std::uint16_t>(uv[2] >> 16) == normal)
        {
            if (v.x <= a.x && v.y <= a.y)
            {
                a = v;
                au.x = as_float(uv[0]);
                au.y = as_float(uv[1]);
            }
            else if (v.x >= b.x && v.y <= b.y)
            {
                b = v;
                bu.x = as_float(uv[0]);
                bu.y = as_float(uv[1]);
            }
            if (v.x <= c.x && v.y >= c.y)
            {
                c = v;
                cu.x = as_float(uv[0]);
                cu.y = as_float(uv[1]);
            }
            else if (v.x >= d.x && v.y >= d.y)
            {
                d = v;
                du.x = as_float(uv[0]);
                du.y = as_float(uv[1]);
            }
        }
        else
        {
            if (v.x <= a.x && v.y <= a.y)
            {
                au.z = as_float(uv[0]);
                au.w = as_float(uv[1]);
            }
            else if (v.x >= b.x && v.y <= b.y)
            {
                bu.z = as_float(uv[0]);
                bu.w = as_float(uv[1]);
            }
            if (v.x <= c.x && v.y >= c.y)
            {
                cu.z = as_float(uv[0]);
                cu.w = as_float(uv[1]);
            }
            else if (v.x >= d.x && v.y >= d.y)
            {
                du.z = as_float(uv[0]);
                du.w = as_float(uv[1]);
            }
        }
    }
    a.z = b.z = c.z = d.z = 0;
}
} // namespace
void prepare_animated_track_part(TrackPartPreparationView w, std::uint32_t kind)
{
    const auto count = w.polygons.size(), half = count / 2, side = half * 2 + 2;
    if (kind < 1 || kind > 3 || count < 2 || count > 65535 || (count & 1u) || w.vertices.size() > 65535 ||
        w.vertices.size() < side * 2 || w.coordinates.size() < w.vertices.size())
        throw FormatError("animated track part outside authored source counts");
    for (std::size_t i = 0; i < w.vertices.size(); ++i)
    {
        const auto &vertex = w.vertices[i];
        const auto &coordinate = w.coordinates[i];
        const auto index = coordinate[2] >> 16;
        if (index >= w.normals.size() || !std::isfinite(vertex.x) || !std::isfinite(vertex.y) ||
            !std::isfinite(as_float(coordinate[0])) || !std::isfinite(as_float(coordinate[1])))
            throw FormatError("animated track part has unbounded/nonfinite source inputs");
        const auto &n = w.normals[index];
        if (!std::isfinite(n.x) || !std::isfinite(n.y) || !std::isfinite(n.z))
            throw FormatError("animated track part has nonfinite normal");
    }
    for (std::size_t i = 0; i < half; ++i)
    {
        const auto corner = polygon_corner(w.polygons[i]);
        if (corner > w.corners.size() || half * 4 + 4 > w.corners.size() - corner)
            throw FormatError("animated track corner writes leave the source owner");
    }
    std::uint16_t selected = 0xffff;
    constexpr float epsilon = std::bit_cast<float>(0x3727c5acu), negative_z = std::bit_cast<float>(0xbf7fff58u);
    for (std::size_t i = 0; i < w.vertices.size(); ++i)
    {
        const auto index = static_cast<std::uint16_t>(w.coordinates[i][2] >> 16);
        const auto &n = w.normals[index];
        if (std::abs(n.x) < epsilon && std::abs(n.y) < epsilon && n.z < negative_z)
            selected = index;
    }
    scan(w, selected);
    const auto &a = w.scratch.values[0];
    const auto &au = w.scratch.values[1];
    const auto &b = w.scratch.values[2];
    const auto &bu = w.scratch.values[3];
    const auto &c = w.scratch.values[4];
    const auto &cu = w.scratch.values[5];
    const auto &d = w.scratch.values[6];
    const auto &du = w.scratch.values[7];
    // Match x87 live values and the compiler's explicit binary32 spills.
    // 425EC1..425EF7 retain first-edge deltas; second/back deltas spill tofloat.
    const double ax = double(b.x) - a.x, ay = double(b.y) - a.y, az = double(b.z) - a.z;
    const double au_delta = double(bu.x) - au.x, av_delta = double(bu.y) - au.y;
    const float cx = rounded(double(d.x) - c.x), cy = rounded(double(d.y) - c.y), cz = rounded(double(d.z) - c.z);
    const float cu_delta = rounded(double(du.x) - cu.x), cv_delta = rounded(double(du.y) - cu.y);
    const float back_au = rounded(double(bu.z) - au.z), back_av = rounded(double(bu.w) - au.w);
    const float back_cu = rounded(double(du.z) - cu.z), back_cv = rounded(double(du.w) - cu.w);
    const float step = rounded(1.0 / double(half));
    double t = 0;
    unsigned wave = 0;
    for (std::size_t i = 0; i < side; i += 2)
    {
        auto &va = w.vertices[i];
        auto &vc = w.vertices[i + 1];
        auto &qa = w.coordinates[i];
        auto &qc = w.coordinates[i + 1];
        qa[2] = (qa[2] & 0xffff0000u) | static_cast<std::uint16_t>(i);
        va.x = rounded(double(a.x) + ax * t);
        va.y = rounded(double(a.y) + rounded(double(rounded(ay)) * t));
        va.z = rounded(double(a.z) + rounded(double(rounded(az)) * t));
        qa[0] = as_word(rounded(au_delta * t + au.x));
        qa[1] = as_word(rounded(av_delta * t + au.y));
        qa[3] = 0x00ffffff;
        qc[2] = (qc[2] & 0xffff0000u) | static_cast<std::uint16_t>(i + 1);
        vc.x = rounded(double(c.x) + double(cx) * t);
        vc.y = rounded(double(c.y) + rounded(double(cy) * t));
        vc.z = rounded(double(c.z) + rounded(double(cz) * t));
        qc[0] = as_word(rounded(double(cu_delta) * t + cu.x));
        qc[1] = as_word(rounded(double(cv_delta) * t + cu.y));
        qc[3] = 0x00ffffff;
        if (kind == 1)
        {
            const double height = double(as_float(vehicle_tables::sine_bits[wave / side])) * -256.0;
            va.y = rounded(double(a.y) + height);
            vc.y = rounded(double(c.y) + height);
        }
        w.vertices[side + i] = va;
        w.vertices[side + i + 1] = vc;
        auto &back_a = w.coordinates[side + i];
        auto &back_c = w.coordinates[side + i + 1];
        back_a = qa;
        back_c = qc;
        back_a[2] = (back_a[2] & 0xffff0000u) | static_cast<std::uint16_t>(side + i);
        back_c[2] = (back_c[2] & 0xffff0000u) | static_cast<std::uint16_t>(side + i + 1);
        back_a[0] = as_word(rounded(double(back_au) * t + au.z));
        back_a[1] = as_word(rounded(double(back_av) * t + au.w));
        back_c[0] = as_word(rounded(double(back_cu) * t + cu.z));
        back_c[1] = as_word(rounded(double(back_cv) * t + cu.w));
        wave += 0x2000;
        t += step; // 4251AC keeps accumulated t in a live x87 register
    }
    for (std::size_t i = 0; i < half; ++i)
    {
        auto &polygon = w.polygons[i];
        polygon[3] &= 0x7f;
        const auto at = polygon_corner(polygon);
        const auto front = i * 2;
        w.corners[at + 1] = static_cast<std::uint16_t>(front + 1);
        w.corners[at + 2] = static_cast<std::uint16_t>(front + 3);
        w.corners[at + 3] = static_cast<std::uint16_t>(front + 2);
        w.corners[at] = static_cast<std::uint16_t>(front);
        w.corners[at + half * 4] = static_cast<std::uint16_t>(front + side);
        w.corners[at + half * 4 + 1] = static_cast<std::uint16_t>(front + side + 1);
        w.corners[at + half * 4 + 2] = static_cast<std::uint16_t>(front + side + 3);
        w.corners[at + half * 4 + 3] = static_cast<std::uint16_t>(front + side + 2);
    }
}
} // namespace sgn::retail
