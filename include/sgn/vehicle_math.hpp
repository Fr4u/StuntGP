#pragma once
#include "sgn/assets.hpp"
#include "sgn/vehicle_tables.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace sgn::retail
{
using VehicleMatrix = std::array<float, 16>;
struct VehicleVector
{
    float x{}, y{}, z{}, w{};
};
inline float vehicle_f32(double x)
{
    return static_cast<float>(x);
}
inline VehicleMatrix vehicle_identity()
{
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
} // 0044ee10
inline VehicleVector vehicle_normalize(VehicleVector v, float *length = nullptr)
{ // 0044ed40 / 0044eda0
    const double squared = (double(v.x) * v.x + double(v.y) * v.y) + double(v.z) * v.z;
    if (squared == 0)
    {
        if (length)
            *length = 0;
        return {0, 0, 0, v.w};
    }
    const double magnitude = std::sqrt(squared);
    if (length)
        *length = vehicle_f32(magnitude);
    const double inverse = 1 / magnitude;
    return {vehicle_f32(inverse * v.x), vehicle_f32(inverse * v.y), vehicle_f32(inverse * v.z), v.w};
}
inline VehicleMatrix vehicle_transpose(const VehicleMatrix &m)
{ // 0044ee50, nonalias result
    VehicleMatrix out{};
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 4; ++j)
            out[i * 4 + j] = m[j * 4 + i];
    return out;
}
inline VehicleMatrix vehicle_basis(VehicleVector x, VehicleVector y, VehicleVector z)
{ // 0044f050
    return {x.x, x.y, x.z, x.w, y.x, y.y, y.z, y.w, z.x, z.y, z.z, z.w, 0, 0, 0, 1};
}
inline VehicleVector vehicle_cross(VehicleVector a, VehicleVector b)
{
    return {vehicle_f32(double(a.y) * b.z - double(a.z) * b.y), vehicle_f32(double(a.z) * b.x - double(a.x) * b.z),
            vehicle_f32(double(a.x) * b.y - double(a.y) * b.x), 0};
}
inline VehicleVector vehicle_column(const VehicleMatrix &m, unsigned column)
{
    const auto at = column * 4;
    return {m.at(at), m.at(at + 1), m.at(at + 2), m.at(at + 3)};
}
inline void vehicle_orthogonalize(VehicleMatrix &m)
{ // 0044f8e0; xyz is defined, first column w is not.
    auto z = vehicle_normalize(vehicle_column(m, 2));
    auto y = vehicle_normalize(vehicle_column(m, 1));
    auto x = vehicle_normalize(vehicle_cross(y, z));
    y = vehicle_normalize(vehicle_cross(z, x));
    const float old_x_w = m[3], old_y_w = m[7]; // New policy for the retail undefined stack word: retain opaque lane.
    const std::array<VehicleVector, 3> columns = {x, y, z};
    for (unsigned i = 0; i < 3; ++i)
    {
        m[i * 4] = columns[i].x;
        m[i * 4 + 1] = columns[i].y;
        m[i * 4 + 2] = columns[i].z;
    }
    m[3] = old_x_w;
    m[7] = old_y_w;
}
struct VehicleRandom
{ // 00463289 and 0042f1c0, explicit ownership replaces the retail global.
    std::uint32_t state{};
    double signed_value(float amplitude)
    {
        state = state * 0x343fdu + 0x269ec3u;
        const auto value = static_cast<std::int32_t>((state >> 16) & 0x7fff) - 0x3fff;
        return double(value) * amplitude * double(3.051851e-05f);
    }
};
inline unsigned vehicle_angle_index(float angle)
{
    // 0044f640/0044f800: __ftol(angle * [0046eba0]) & 0x1fff.
    const double index = double(angle) * double(std::bit_cast<float>(std::uint32_t{0x41b60b61}));
    if (!std::isfinite(index) || index < -2147483648.0 || index > 2147483647.0)
        throw std::domain_error("vehicle rotation outside recovered finite range");
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(index)) & 8191;
}
inline VehicleMatrix vehicle_rotation(unsigned axis, float angle)
{ // 0044f790/0044f800/0044f870
    const auto index = vehicle_angle_index(angle);
    const float s = vehicle_tables::sine(index), c = vehicle_tables::sine(index + 2048);
    if (axis == 0)
        return {1, 0, 0, 0, 0, c, s, 0, 0, -s, c, 0, 0, 0, 0, 1};
    if (axis == 1)
        return {c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, 0, 0, 0, 1};
    if (axis == 2)
        return {c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    throw std::domain_error("invalid vehicle rotation axis");
}
inline void vehicle_premultiply(VehicleMatrix &target, const VehicleMatrix &left)
{ // 0044ef40
    // Only the nine basis elements change. Preserve homogeneous/payload fields.
    for (unsigned j = 0; j < 3; ++j)
    {
        const unsigned a = j * 4;
        const double x = target[a], y = target[a + 1], z = target[a + 2];
        const float ox = vehicle_f32((z * left[8] + double(left[4]) * y) + double(left[0]) * x);
        const float oy = vehicle_f32((double(left[1]) * x + z * left[9]) + double(left[5]) * y);
        const float oz = vehicle_f32((double(left[2]) * x + z * left[10]) + double(left[6]) * y);
        target[a] = ox;
        target[a + 1] = oy;
        target[a + 2] = oz;
    }
}
inline void vehicle_rotate(VehicleMatrix &target, unsigned axis, float angle)
{ // 0044eeb0 via 0044f640/6b0/720
    const auto left = vehicle_rotation(axis, angle);
    for (unsigned j = 0; j < 3; ++j)
    {
        const unsigned a = j * 4;
        const double x = target[a], y = target[a + 1], z = target[a + 2];
        // The immediate-matrix entry has a different instruction summation order.
        const float ox = vehicle_f32((double(left[8]) * z + double(left[0]) * x) + double(left[4]) * y);
        const float oy = vehicle_f32((double(left[9]) * z + double(left[1]) * x) + double(left[5]) * y);
        const float oz = vehicle_f32((double(left[10]) * z + double(left[2]) * x) + double(left[6]) * y);
        target[a] = ox;
        target[a + 1] = oy;
        target[a + 2] = oz;
    }
}
inline void vehicle_multiply_basis(VehicleMatrix &out, const VehicleMatrix &right, const VehicleMatrix &left)
{ // 0044efd0
    // Original alias path computes into a stack temporary and never copies it
    // back. Preserve that behavior; the physical caller passes distinct buffers.
    if (&out == &right || &out == &left)
        return;
    for (unsigned j = 0; j < 3; ++j)
    {
        const unsigned a = j * 4;
        const double x = right[a], y = right[a + 1], z = right[a + 2];
        out[a] = vehicle_f32((double(left[8]) * z + y * left[4]) + double(left[0]) * x);
        out[a + 1] = vehicle_f32((y * left[5] + double(left[1]) * x) + z * left[9]);
        out[a + 2] = vehicle_f32((y * left[6] + double(left[2]) * x) + z * left[10]);
    }
}
inline void vehicle_level_matrix(VehicleMatrix &matrix, float divisor)
{ // 0040f330
    const double scale = 1.0 / divisor;
    const float stored_scale = vehicle_f32(scale);
    auto x = vehicle_column(matrix, 0), y = vehicle_column(matrix, 1), z = vehicle_column(matrix, 2);
    x.x = vehicle_f32(scale * x.x);
    y.y = vehicle_f32(double(stored_scale) * y.y);
    z.z = vehicle_f32(double(z.z) * stored_scale);
    // Retail builds three temporary xyz vectors with unwritten w lanes. Retain
    // the input opaque lanes as the explicit native policy for those words.
    matrix = vehicle_basis(vehicle_normalize(x), vehicle_normalize(y), vehicle_normalize(z));
}
inline void vehicle_steer_increase(float &current, float maximum, float amount)
{ // 004143e0
    const double value = double(amount) + current;
    current = vehicle_f32(value);
    if (value > maximum)
        current = maximum;
}
inline void vehicle_steer_decrease(float &current, float maximum, float amount)
{ // 00414420
    const double value = double(current) - amount;
    current = vehicle_f32(value);
    if (value < -double(maximum))
        current = -maximum;
}
} // namespace sgn::retail
