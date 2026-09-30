#pragma once
// Plain C++ world vertex/lighting leaves (perf_world R19, ROOT DIRECTIVE).
// NEW representation of recovered arithmetic; formulas cite the original x87
// sequences, which remain in src/source_track_vertices_x64.asm,
// src/source_part_vertices_x64.asm and src/source_world_lighting_x64.asm as the
// reference (define SGN_WORLD_X87_REFERENCE to route the callers back to them).
//
// Numeric equivalence: every race frame runs 429510 (prepare_source_numeric_
// win64_429510), which selects x87 precision control PC53 and round-to-nearest
// and mirrors rounding/masks into MXCSR without FTZ/DAZ. Under PC53 each x87
// FLD/FMUL/FADD/FSUB/FDIV on binary32 or register operands is exactly one IEEE
// binary64 operation (binary32 inputs are exact in binary64; the wider x87
// exponent range cannot be reached from binary32 inputs by these short
// expressions), and each FST/FSTP DWORD is one binary64->binary32 rounding.
// The functions below perform exactly those operations in the original order.
// Every including TU must be compiled with /fp:strict (no contraction or
// reassociation). NaN payload selection for two NaN operands (x87 vs SSE) is
// not claimed; it affects only rendered values of corrupt geometry.
#include <bit>
#include <cstdint>
#include <cstring>

namespace sgn::retail::detail
{
struct WorldVertexProjection
{
    float x, y, z, offset, near_plane;
}; // 61C3E8/3FC/410/420, camera+198
inline float world_f32(double value) noexcept
{
    return static_cast<float>(value);
}
inline float world_load_f32(const void *bytes, std::size_t offset) noexcept
{
    float value;
    std::memcpy(&value, static_cast<const std::uint8_t *>(bytes) + offset, 4);
    return value;
}
inline void world_store_f32(void *bytes, std::size_t offset, float value) noexcept
{
    std::memcpy(static_cast<std::uint8_t *>(bytes) + offset, &value, 4);
}
inline void world_store_u16(void *bytes, std::size_t offset, std::uint16_t value) noexcept
{
    std::memcpy(static_cast<std::uint8_t *>(bytes) + offset, &value, 2);
}
inline std::uint32_t world_sign(float value) noexcept
{
    return std::bit_cast<std::uint32_t>(value) >> 31;
}
// Clip-code WORD shared by 4299A0..429A03 and 429600..42964C: the sign bits of
// the spilled binary32 differences, in the original bit positions, plus bit1.
inline std::uint16_t world_clip_mask(float near_minus_z, float minus_w_minus_x, float x_minus_w, float minus_w_minus_y,
                                     float y_minus_w) noexcept
{
    return static_cast<std::uint16_t>((world_sign(y_minus_w) << 5) | (world_sign(minus_w_minus_y) << 4) |
                                      (world_sign(x_minus_w) << 3) | (world_sign(minus_w_minus_x) << 2) |
                                      world_sign(near_minus_z) | 2u);
}
// Clip tail common to 4298F1 and 429544 after X/Y were spilled and Z (W) kept.
inline void world_clipped_tail(float fx, float fy, double z, const WorldVertexProjection &p, void *out) noexcept
{
    const double px = double(p.x) * fx;           // FLD px; FMUL [X]
    world_store_f32(out, 0, world_f32(px));       // FST [out]
    const float py = world_f32(double(p.y) * fy); // FLD py; FMUL [Y]; FST spill; FSTP [out+4]
    world_store_f32(out, 4, py);
    const double pz = double(p.z) * z + double(p.offset);                        // FLD pz; FMUL st(2); FADD offset
    world_store_f32(out, 8, world_f32(pz));                                      // FST [out+8]
    const float near_minus_z = world_f32(double(p.near_plane) - pz);             // FLD near; FSUB st(1); FSTP
    const double minus_w = -z;                                                   // FLD st(1); FCHS
    const float minus_w_spill = world_f32(minus_w);                              // FST spill
    const float minus_w_minus_x = world_f32(minus_w - px);                       // FSUB st(1); FSTP
    const float x_minus_w = world_f32(px - z);                                   // FSUB st(1); FSTP
    const float minus_w_minus_y = world_f32(double(minus_w_spill) - double(py)); // FLD spill; FSUB [py]
    const float y_minus_w = world_f32(double(py) - z);                           // FLD [py]; FSUB st(1)
    world_store_f32(out, 28, world_f32(z));                                      // FSTP [out+1C]
    world_store_u16(out, 16, world_clip_mask(near_minus_z, minus_w_minus_x, x_minus_w, minus_w_minus_y, y_minus_w));
}
// 42979A..42983C (track, inside): X unspilled, Y spilled, Z order YZX.
inline void world_track_vertex_inside_42979a(const float *m, const void *xyz, const WorldVertexProjection &p,
                                             void *out) noexcept
{
    const double x = world_load_f32(xyz, 0), y = world_load_f32(xyz, 4), z = world_load_f32(xyz, 8);
    const double vx = ((double(m[0]) * x + double(m[4]) * y) + double(m[8]) * z) + double(m[12]);
    const float vy = world_f32(((double(m[1]) * x + double(m[5]) * y) + double(m[9]) * z) + double(m[13]));
    const double vz = ((double(m[6]) * y + double(m[10]) * z) + double(m[2]) * x) + double(m[14]);
    world_store_f32(out, 0, world_f32(double(p.x) * vx));
    world_store_f32(out, 4, world_f32(double(p.y) * vy));
    world_store_f32(out, 8, world_f32(double(p.z) * vz + double(p.offset)));
    world_store_f32(out, 28, world_f32(vz));
}
// 4298F1..429A03 (track, clipped): X/Y spilled, Z order YZX, then the clip
// tail; the stamp WORD at+12 is stored from the caller's frame stamp (4299E5).
inline void world_track_vertex_clipped_4298f1(const float *m, const void *xyz, const WorldVertexProjection &p,
                                              std::uint32_t stamp, void *out) noexcept
{
    const double x = world_load_f32(xyz, 0), y = world_load_f32(xyz, 4), z = world_load_f32(xyz, 8);
    const float vx = world_f32(((double(m[0]) * x + double(m[4]) * y) + double(m[8]) * z) + double(m[12]));
    const float vy = world_f32(((double(m[1]) * x + double(m[5]) * y) + double(m[9]) * z) + double(m[13]));
    const double vz = ((double(m[6]) * y + double(m[10]) * z) + double(m[2]) * x) + double(m[14]);
    world_store_u16(out, 18, static_cast<std::uint16_t>(stamp));
    world_clipped_tail(vx, vy, vz, p, out);
}
// 429544..42964C (model part, clipped): X/Y spilled, Z order XYZ.
inline void world_part_vertex_clipped_429544(const float *m, const void *xyz, const WorldVertexProjection &p,
                                             void *out) noexcept
{
    const double x = world_load_f32(xyz, 0), y = world_load_f32(xyz, 4), z = world_load_f32(xyz, 8);
    const float vx = world_f32(((double(m[0]) * x + double(m[4]) * y) + double(m[8]) * z) + double(m[12]));
    const float vy = world_f32(((double(m[1]) * x + double(m[5]) * y) + double(m[9]) * z) + double(m[13]));
    const double vz = ((double(m[2]) * x + double(m[6]) * y) + double(m[10]) * z) + double(m[14]);
    world_clipped_tail(vx, vy, vz, p, out);
}
// 429673..42971A (model part, inside): X unspilled, Y/Z spilled, all YZX.
inline void world_part_vertex_inside_429673(const float *m, const void *xyz, const WorldVertexProjection &p,
                                            void *out) noexcept
{
    const double x = world_load_f32(xyz, 0), y = world_load_f32(xyz, 4), z = world_load_f32(xyz, 8);
    const double vx = ((double(m[4]) * y + double(m[8]) * z) + double(m[0]) * x) + double(m[12]);
    const float vy = world_f32(((double(m[5]) * y + double(m[9]) * z) + double(m[1]) * x) + double(m[13]));
    const float vz = world_f32(((double(m[6]) * y + double(m[10]) * z) + double(m[2]) * x) + double(m[14]));
    world_store_f32(out, 28, vz);
    world_store_f32(out, 0, world_f32(double(p.x) * vx));
    world_store_f32(out, 4, world_f32(double(p.y) * vy));
    world_store_f32(out, 8, world_f32(double(p.z) * vz + double(p.offset)));
}
// sgn_source_model_light_dot (42A3xx/42A6xx incidence): (Z*Z'+Y*Y')+X*X',
// FSTP binary32, raw bits (sign of zero retained).
inline std::uint32_t world_light_dot(const void *prepared, const void *normal) noexcept
{
    const double yz = double(world_load_f32(prepared, 8)) * world_load_f32(normal, 8) +
                      double(world_load_f32(prepared, 4)) * world_load_f32(normal, 4);
    return std::bit_cast<std::uint32_t>(
        world_f32(yz + double(world_load_f32(prepared, 0)) * world_load_f32(normal, 0)));
}
// FISTP QWORD under a truncating CW (source_model_ftol), low DWORD. Values
// outside the signed 64-bit range and NaN store the integer indefinite
// 0x8000000000000000, whose low DWORD is zero.
inline std::uint32_t world_ftol_low(double value) noexcept
{
    if (!(value >= -9223372036854775808.0 && value < 9223372036854775808.0))
        return 0;
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(value)));
}
inline std::uint32_t world_light_ftol(std::uint32_t bits) noexcept
{
    return world_ftol_low(std::bit_cast<float>(bits));
}
// sgn_source_model_light_point: light+20/24/28 minus vertex XYZ, light+8 times
// 4177920.0f (PE 46E6F0), divided by (dz*dz+dy*dy)+dx*dx, truncated low DWORD.
inline std::uint32_t world_light_point(const void *light, const void *vertex) noexcept
{
    const double dx = double(world_load_f32(light, 0x20)) - world_load_f32(vertex, 0);
    const double dy = double(world_load_f32(light, 0x24)) - world_load_f32(vertex, 4);
    const double dz = double(world_load_f32(light, 0x28)) - world_load_f32(vertex, 8);
    const double numerator = double(world_load_f32(light, 8)) * double(std::bit_cast<float>(0x4a7f0000u));
    return world_ftol_low(numerator / ((dz * dz + dy * dy) + dx * dx));
}
} // namespace sgn::retail::detail
