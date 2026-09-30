#include "sgn/camera_path.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>
#include <limits>

namespace sgn::retail
{
namespace
{
float f32(double x)
{
    return static_cast<float>(x);
}
using sgn::bit_reinterpret::as_float;
std::uint32_t floor_index(double value)
{
    const double result = std::floor(value);
    if (!std::isfinite(result) || result < double(std::numeric_limits<std::int32_t>::min()) ||
        result > double(std::numeric_limits<std::int32_t>::max()))
        throw FormatError("camera path index outside signed conversion domain");
    return std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(result));
}
} // namespace
CameraPathSample camera_sample_path(SplineView spline, float parameter)
{
    const auto count = spline.descriptor.point_count;
    if (count == 0 || !std::isfinite(parameter))
        throw FormatError("invalid camera path count or parameter");
    if (spline.descriptor.opaque[0] == 1)
    {
        auto first = floor_index(parameter);
        if (first >= count)
            first = 0;
        auto second = first + 1;
        if (second >= count)
            second = 0;
        const auto a = spline_point(spline, first), b = spline_point(spline, second);
        const double fraction = std::fmod(double(parameter), 1.0);
        const float x = f32((double(b.value.x) - a.value.x) * fraction);
        const float y = f32(double(f32(double(b.value.y) - a.value.y)) * fraction);
        const float z = f32(double(b.value.z) - a.value.z);
        return {{f32(double(x) + a.value.x), f32(double(y) + a.value.y), f32(fraction * z + a.value.z)},
                a.adjacent_storage_read || b.adjacent_storage_read};
    }
    // 428520 retains the extended +3 for floor but stores binary32 before fmod.
    const double shifted = double(parameter) + 3.0;
    auto index = floor_index(shifted);
    const float t = f32(std::fmod(double(f32(shifted)), 1.0));
    const double t2 = double(t) * t, t3 = t2 * t, times3 = double(t) * 3.0, k = as_float(0x3e2aaaabu);
    const std::array<float, 4> weights{f32((((t2 * 3.0 - t3) - times3) + 1.0) * k),
                                       f32(((t3 * 3.0 - t2 * 6.0) + 4.0) * k),
                                       f32(((((3.0 - times3) * t + 3.0) * t) + 1.0) * k), f32(t3 * k)};
    double x = 0, y = 0, z = 0;
    bool adjacent = false;
    for (int i = 3; i >= 0; --i)
    {
        auto selected = index;
        if (selected >= count)
            selected = spline.descriptor.opaque[1] == 2 ? selected % count : count - 1;
        const auto p = spline_point(spline, selected);
        adjacent |= p.adjacent_storage_read;
        x = double(weights[static_cast<unsigned>(i)]) * p.value.x + x;
        y = double(p.value.y) * weights[static_cast<unsigned>(i)] + y;
        z = double(p.value.z) * weights[static_cast<unsigned>(i)] + z;
        if (index > 0)
            --index;
    }
    return {{f32(x), f32(y), f32(z)}, adjacent};
}
void camera_advance_path(float &parameter, float &speed, CameraPathInput input)
{
    if (!std::isfinite(parameter) || !std::isfinite(speed))
        throw FormatError("non-finite camera path state");
    speed = f32(double(speed) * as_float(0x3f733333u));
    if (input.increase_speed && speed < as_float(0x3ac49ba6u))
        speed = f32(double(speed) + as_float(0x37d1b717u));
    if (input.decrease_speed && speed > as_float(0xbac49ba6u))
        speed = f32(double(speed) - as_float(0x37d1b717u));
    const float next = f32(double(parameter) + speed);
    parameter = f32(next < 0 ? double(next) + 1.0 : std::fmod(double(next), 1.0));
    if (std::abs(double(speed)) < as_float(0x35d1b717u))
        speed = 0;
}
} // namespace sgn::retail
